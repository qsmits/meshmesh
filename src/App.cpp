#include "UsbEvents.h"
#include "HistoryJson.h"
#include "Version.h"
#include <Utils.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "App.h"
#include "Companion.h"
#include "Remote.h"
#include "MeshRadio.h"
#include "Hardware.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "Internet.h"
#include "Power.h"
#include "Regions.h"
#include "NetFind.h"
#include "Notifications.h"
#include "ChannelPolicy.h"
#include "QuickSend.h"
#include "Wardrive.h"
#include <LittleFS.h>
#include <nvs.h>
#include "Storage.h"
#include <time.h>
String statusJson() {
  StaticJsonDocument<3072> d;
  d["version"] = MESHMM_VERSION;
  d["revision"] = MESHMM_REVISION;
  d["board"] = MM_BOARD_ID;
  d["board_name"] = hardware.boardName;
  d["max_power"] = MM_MAX_POWER;
#if defined(MM_COMPACT)
  d["family"] = "compact";
  d["button"] = MM_BUTTON;
#else
  d["family"] = "full";
  d["mic_key"] = hardware.micKey;
#endif
  {
    static const int absent[] = {MM_ABSENT - 1};
    JsonArray a = d.createNestedArray("absent");
    for (int i : absent)
      if (i >= 0) a.add(i);
  }
  d["firmware"] = MESHMM_FIRMWARE;
  d["node"] = meshRadio.idText(meshRadio.nodeId);
  d["name"] = config.name;
  d["network"] = meshRadio.networkId;
  char buildHash[65];
  mesh::Utils::toHex(buildHash, esp_ota_get_app_description()->app_elf_sha256, 32);
  d["build_sha256"] = buildHash;
  d["protocol"] = "MeshCore";
  d["public_key"] = meshRadio.publicKeyText();
  d["channel"] = "Public";
  d["channels"] = meshRadio.channelCount;
  d["public_message_limit"] = meshRadio.messageLimit();
  d["unix_time"] = int64_t(time(nullptr));
  d["clock_source"] = hardware.clockSource;
  d["clock_age_seconds"] = hardware.clockAge() / 1000;
  d["clock_conflict"] = hardware.clockConflict;
  d["uptime"] = millis() / 1000;
  d["boot"] = config.bootCounter;
  d["reset_reason"] = int(esp_reset_reason());
  d["heap"] = ESP.getFreeHeap();
  d["psram"] = ESP.getFreePsram();
  d["cpu_mhz"] = powerMhz();
  d["sleeps"] = powerSleeps();
  d["sleep_ms"] = powerSleptMs();
  d["slow_ms"] = powerSlowMs();
  d["heap_max_block"] =
      ESP.getMaxAllocHeap(); // the largest internal-RAM allocation: what a board without PSRAM can still answer
  d["history_count"] = meshRadio.historyCount;
  d["idle_waits"] = powerIdleWaits();
  d["idle_wait_ms"] = powerIdleMs();
  d["idle_radio_events"] = powerRadioEvents();
  {
    int way = powerOnWay();
    d["power_on"] = way >= 0 ? "button" : way == -2 ? "power_key" : "reset";
    if (way >= 0) d["power_button"] = MM_BUTTON;
  } // "poweroff": what turns the board on again
  d["radio"] = meshRadio.ready;
  d["radio_error"] = meshRadio.radioError;
  d["tx"] = meshRadio.txCount;
  d["radio_recal"] = meshRadio.recalibrations;
  d["rx"] = meshRadio.rxCount;
  d["rejected"] = meshRadio.rejected;
  d["relayed"] = meshRadio.relayed;
  d["contacts_replaced"] = meshRadio.replaced;
  d["contacts_saved"] = meshRadio.contactsSaved;
  d["diagnostic_rx"] = meshRadio.diagnosticRx;
  d["rssi"] = meshRadio.lastRssi;
  d["snr"] = meshRadio.lastSnr;
  d["keyboard"] = hardware.keyboardOk;
  d["key_count"] = hardware.keyCount;
  d["last_key"] = hardware.lastKey;
  d["battery_mv"] = hardware.batteryMv;
  if (hardware.usbPower >= 0) d["usb_power"] = hardware.usbPower > 0;
  d["sd"] = hardware.sdOk;
  d["storage"] = hardware.fsOk;
  d["rtc"] = hardware.rtcOk;
  d["rtc_valid"] = hardware.rtcValid;
  d["compass"] = hardware.compassOk;
  d["compass_sample"] = hardware.compassSample;
  d["imu"] = hardware.imuOk;
  d["imu_sample"] = hardware.imuSample;
  d["gps_bytes"] = hardware.gpsBytes;
  d["gps_sentences"] = hardware.gps.passedChecksum();
  d["gps_failed"] = hardware.gps.failedChecksum();
  d["gps_fix"] = hardware.gpsFix();
  d["satellites"] = hardware.gps.satellites.value();
  if (d["gps_fix"].as<bool>()) {
    d["latitude"] = hardware.gps.location.lat();
    d["longitude"] = hardware.gps.location.lng();
  }
  JsonArray a = d.createNestedArray("mag");
  for (float n : hardware.mag) a.add(n);
  a = d.createNestedArray("accel");
  for (float n : hardware.accel) a.add(n);
  d["clock_trusted"] = hardware.clockTrusted;
  d["busy"] = meshRadio.busy();
  if (meshRadio.lastRxAt) d["rx_age"] = (millis() - meshRadio.lastRxAt) / 1000;
  {
    nvs_stats_t nvs;
    if (nvs_get_stats(nullptr, &nvs) == ESP_OK) {
      d["nvs_used"] = nvs.used_entries;
      d["nvs_free"] = nvs.free_entries;
    }
  } // 32-byte entries; the contacts no longer live there
  d["quick_send"] = true;
  d["wardrive"] = true;
  d["notification_dropped"] = notifications::dropped();
#if defined(MM_COMPACT)
  d["notification_led"] = pins::led >= 0;
#else
  d["notification_led"] = false;
#endif
  d["event"] = meshRadio.event;
  d["wifi"] = portalActive();
  d["internet"] = internet.online();
  d["ble"] = bleActive();
  d["companion"] = companion::connected();
  String s;
  serializeJson(d, s);
  return s;
}
String messagesJson() {
  // Do not reserve a 32 KB JSON pool in addition to the serialized history on small nRF52 heaps.
  // Measure first so allocation failure is an explicit error, never a misleading empty array.
  StaticJsonDocument<1024> d;
  auto record = [&](unsigned i) { historyJsonRecord(d, meshRadio.history[i]); };
  size_t bytes = 2;
  for (unsigned i = 0; i < meshRadio.historyCount; i++) {
    record(i);
    if (d.overflowed()) return "ERR history JSON capacity";
    bytes += measureJson(d) + (i ? 1 : 0);
  }
  String s;
  if (!s.reserve(bytes)) return "ERR history response memory";
  s += '[';
  for (unsigned i = 0; i < meshRadio.historyCount; i++) {
    record(i);
    if (i) s += ',';
    serializeJson(d, s);
  }
  s += ']';
  if (s.length() != bytes) return "ERR history response truncated";
  return s;
}
String nodesJson() {
  DynamicJsonDocument d(16384);
  JsonArray a = d.to<JsonArray>();
  for (unsigned i = 0; i < meshRadio.peerCount; i++) {
    auto& p = meshRadio.peers[i];
    JsonObject j = a.createNestedObject();
    char key[65];
    mesh::Utils::toHex(key, p.publicKey, 32);
    j["public_key"] = key;
    j["type"] = p.type;
    j["heard"] = p.heard;
    j["path_length"] = p.pathLength;
    j["id"] = meshRadio.idText(p.id);
    j["name"] = p.name;
    j["rssi"] = p.rssi;
    j["snr"] = p.snr;
    if (p.heard)
      j["age_seconds"] = (millis() - p.seen) / 1000;
    else
      j["age_seconds"] = nullptr;
    j["hops"] = p.hops;
    j["position"] = p.position;
    if (p.position) {
      j["latitude"] = p.latitude;
      j["longitude"] = p.longitude;
    }
  }
  String s;
  serializeJson(d, s);
  return s;
}
String channelsJson(bool secrets) {
  DynamicJsonDocument d(6144);
  d["max"] = channels::Max;
  JsonArray a = d.createNestedArray("channels");
  for (unsigned i = 0; i < meshRadio.channelCount; i++) {
    const auto& c = meshRadio.channelList[i];
    JsonObject j = a.createNestedObject();
    bool open = !i || channels::isHashtag(c);
    j["id"] = meshRadio.idText(c.id);
    j["name"] = c.name;
    j["kind"] = !i ? "public" : open ? "hashtag" : "private";
    char hash[3];
    snprintf(hash, 3, "%02X", channels::hashOf(c.secret));
    j["hash"] = hash;
    if (secrets || open) j["link"] = channels::link(c);
    j["region"] = c.region;
    JsonObject policy = j.createNestedObject("policy");
    policy["led"] = c.policy.led;
    policy["wake"] = c.policy.wake;
    policy["popup"] = c.policy.popup;
    policy["priority"] = c.policy.priority;
    policy["device_limit"] = c.policy.deviceLimit;
    policy["app_limit"] = c.policy.appLimit;
    policy["sound"] = c.policy.sound;
  }
  a = d.createNestedArray("heard");
  for (unsigned i = 0; i < meshRadio.heardCount; i++) {
    const auto& h = meshRadio.heard[i];
    JsonObject j = a.createNestedObject();
    char hash[3];
    snprintf(hash, 3, "%02X", h.hash);
    j["hash"] = hash;
    j["packets"] = h.packets;
    j["age"] = (millis() - h.at) / 1000;
    if (h.name[0]) j["name"] = h.name;
  }
  d["samples"] = meshRadio.heardSamples;
  d["region"] = config.region;
  {
    StaticJsonDocument<1024> r;
    if (!deserializeJson(r, regions::json())) d["regions"] = r.as<JsonObjectConst>();
  } // the last region search
  String s;
  serializeJson(d, s);
  return s;
}
// Replies: "OK channel added|exists ID", "OK ..." or "ERR <reason>: ..." (the page translates the reason).
String channelCommand(JsonObjectConst v) {
  String action = v["action"] | "";
  auto id = [](const char* text, uint64_t& out) {
    String s = text ? text : "";
    if (s == "ALL") {
      out = meshmesh::Broadcast;
      return true;
    }
    char* e = nullptr;
    out = strtoull(s.c_str(), &e, 16);
    return s.length() && s.length() <= 16 && e && !*e && out;
  };
  if (action == "add") {
    if (!meshRadio.ready) return "ERR radio: channels need a working radio";
    MeshRadio::ChannelResult r;
    uint64_t added = 0;
    if (v.containsKey("hashtag"))
      r = meshRadio.joinHashtag(v["hashtag"] | "", &added);
    else if (v.containsKey("link"))
      r = meshRadio.joinLink(v["link"] | "", &added);
    else if (v.containsKey("create"))
      r = meshRadio.createChannel(v["create"] | "", &added);
    else if (v.containsKey("key")) {
      uint8_t key[16];
      if (!channels::parseKey(v["key"] | "", key)) return "ERR key: 32 hexadecimal digits or base64 of 16 bytes";
      r = meshRadio.addChannel(v["name"] | "", key, &added);
      memset(key, 0, 16);
    } else
      return "ERR channel add: hashtag, link, create or name+key";
    switch (r) {
    case MeshRadio::ChannelAdded: return "OK channel added " + meshRadio.idText(added);
    case MeshRadio::ChannelExists: return "OK channel exists " + meshRadio.idText(added);
    case MeshRadio::ChannelFull: return "ERR full: " + String(channels::Max) + " channels at most, Public included";
    case MeshRadio::ChannelBadName: return "ERR name: 1-31 bytes of UTF-8";
    case MeshRadio::ChannelBadKey: return "ERR key: 32 hexadecimal digits or base64 of 16 bytes";
    case MeshRadio::ChannelBadLink: return "ERR link: meshcore://channel/add?name=...&secret=<32 hex>";
    case MeshRadio::ChannelUnavailable: return "ERR mode: channels need the normal mode and a working radio";
    default: return "ERR storage: channel not saved";
    }
  }
  if (action == "policy") {
    uint64_t c;
    if (!id(v["channel"], c)) return "ERR channel ID";
    return channelPolicy::set(c, v);
  }
  if (action == "remove") {
    uint64_t c;
    if (!id(v["channel"], c)) return "ERR channel ID";
    if (c == meshmesh::Broadcast) return "ERR public: Public stays";
    if (!meshRadio.channel(c)) return "ERR unknown channel";
    if (meshRadio.sending(c)) return "ERR busy: a message to this channel is being sent";
    return meshRadio.removeChannel(c) ? "OK channel removed" : "ERR storage: channel list not saved";
  }
  if (action == "invite") {
    uint64_t c, to;
    if (!id(v["channel"], c) || !meshRadio.channel(c)) return "ERR unknown channel";
    if (!id(v["to"], to) || channels::isChannel(to)) return "ERR node ID";
    if (channels::link(*meshRadio.channel(c), true).length() > meshRadio.messageLimit(to))
      return "ERR long: the channel name is too long for an invitation";
    return meshRadio.sendInvite(to, c) ? "OK invitation queued"
                                       : "ERR send: contact unknown, queue full or radio offline";
  }
  if (action == "probe") {
    String tag = channels::hashtag(v["hashtag"] | "");
    if (!tag.length()) return "ERR name: 1-31 bytes of UTF-8";
    int n = meshRadio.probeHashtag(tag);
    uint8_t key[16];
    channels::hashtagSecret(tag, key);
    StaticJsonDocument<192> d;
    d["name"] = tag;
    char hash[3];
    snprintf(hash, 3, "%02X", channels::hashOf(key));
    d["hash"] = hash;
    d["opened"] = n;
    d["samples"] = meshRadio.heardSamples;
    d["joined"] = meshRadio.channel(channels::idOf(key)) != nullptr;
    String s;
    serializeJson(d, s);
    return s;
  }
  if (action == "region") {
    uint64_t c;
    if (!id(v["channel"], c) || !meshRadio.channel(c)) return "ERR unknown channel";
    return meshRadio.setChannelRegion(c, v["region"] | "")
               ? "OK channel region saved"
               : "ERR region: letters, digits and '-', up to 30 bytes; \"\" default, \"*\" none";
  }
  if (action == "regions") {
    return regions::find() ? "OK region search started" : "ERR radio: the region search needs a working radio";
  }
  return "ERR channel action add|remove|invite|probe|policy|region|regions";
}
#include "AppSettings.inc"
namespace {
uint32_t restartAt = 0;
}
void restartTick() {
  if (restartAt && int32_t(millis() - restartAt) >= 0) {
    meshRadio.flush();
    ESP.restart();
  }
} // contacts saved a moment later are written first
String executeCommand(const String& input) {
  String line = input;
  line.trim();
  if (line.startsWith("map ")) return maps.command(line);
  if (line == "internet" || line.startsWith("internet ")) return internet.command(line);
  if (line == "ui") return uiStatus();
  if (line == "navigation") return navigation.info();
  if (line == "radar") return radar.json();
  if (line == "radar web" || line.startsWith("radar do ")) return webRadarCommand(line);
  if (line == "connections") return connectionCredentials(); // Wi-Fi password and BLE PIN: USB or a paired BLE client
  if (line == "calibrate start") {
    if (!hardware.compassOk) return "ERR compass unavailable";
    navigation.start();
    return "OK rotate device in all directions for at least 20 seconds";
  }
  if (line == "calibrate finish")
    return navigation.finish() ? "OK compass calibration saved" : "ERR calibration needs 20 samples and wider rotation";
  if (line == "clock") return hardware.clockInfo();
  if (line.startsWith("clock ")) {
    StaticJsonDocument<128> d;
    if (deserializeJson(d, line.substring(6)) || !d["unix"].is<uint32_t>()) return "ERR clock JSON unix seconds";
    return hardware.setUtc(d["unix"], d["source"] == "phone" ? "phone" : "manual", true) ? "OK UTC clock synchronized"
                                                                                         : "ERR clock range 2025..2038";
  }
  if (line == "status") return statusJson();
  // Repeater or room server: status (no passwords), its passwords, its MeshCore CLI as the local admin, a room post.
  if (line == "config") return configJson();
  if (line.startsWith("radio ")) return radioCommand(line);
  if (line == "key") return configJson(true); // explicitly requested; never put in ordinary diagnostics
  // Backup (docs/backup.md): the node key (USB, a paired BLE client, the access point: Portal.cpp) and a contact of a backup.
  if (line == "identity") {
    String k = meshRadio.privateKeyText();
    return k.length() ? "{\"private_key\":\"" + k + "\",\"public_key\":\"" + meshRadio.publicKeyText() + "\"}"
                      : "ERR identity unavailable";
  }
  if (line.startsWith("identity set ")) {
    int r = meshRadio.restoreKey(line.substring(13));
    if (r == 1) restartAt = millis() + 1500;
    return r == 1    ? "OK identity saved; restarting"
           : r == 0  ? "OK identity unchanged"
           : r == -1 ? "ERR identity: 128 hexadecimal digits of a MeshCore private key"
                     : "ERR identity storage";
  }
  if (line.startsWith("contact add ")) {
    StaticJsonDocument<384> d;
    uint8_t key[32];
    const char* k = nullptr;
    bool ok = !deserializeJson(d, line.substring(12)) && (k = d["key"]) && strlen(k) == 64;
    for (int i = 0; ok && i < 64; i++) ok = isxdigit((unsigned char)k[i]);
    if (!ok || !mesh::Utils::fromHex(key, 32, k)) return "ERR contact add {key,name[,type,lat,lon]}";
    return meshRadio.learnContact(key, d["name"] | "", d["type"] | 1, lroundf((d["lat"] | 0.0f) * 1e6f),
                                  lroundf((d["lon"] | 0.0f) * 1e6f))
               ? "OK contact saved"
               : "ERR contact: the normal mode, type 1..4";
  }
  if (line == "wardrive") return wardrive::json();
  if (line.startsWith("wardrive do ")) {
    StaticJsonDocument<512> d;
    if (deserializeJson(d, line.substring(12)) || !d.is<JsonObject>()) return "ERR wardrive do {JSON}";
    return wardrive::command(d.as<JsonObjectConst>());
  }
  if (line.startsWith("wardrive log ")) return wardrive::log(line.substring(13));
  if (line == "quick") return quickSend::json();
  if (line.startsWith("quick do ")) {
    StaticJsonDocument<512> d;
    if (deserializeJson(d, line.substring(9)) || !d.is<JsonObject>()) return "ERR quick do {JSON}";
    return quickSend::command(d.as<JsonObjectConst>());
  }
  if (line == "messages") return messagesJson();
  if (line == "nodes") return nodesJson();
  if (line == "channels") return channelsJson(true); // USB and a paired BLE client: the private links too
  if (line == "regions") return regions::json();
  // Network search (NetFind.h): every regional preset this radio takes; "use N" tests result N.
  if (line == "netfind") return netfind::json();
  if (line == "netfind stop") {
    netfind::stop();
    return "OK network search stopped";
  }
  if (line.startsWith("netfind use ")) return netfind::use(line.substring(12).toInt());
  if (line == "netfind start")
    return netfind::start()                           ? "OK network search started"
           : meshRadio.busy() || regions::searching() ? "ERR radio busy; retry"
                                                      : "ERR radio offline";
  if (line == "regions find") return regions::find() ? "OK region search started" : "ERR radio offline";
  if (line.startsWith("channel do ")) {
    StaticJsonDocument<512> d;
    if (deserializeJson(d, line.substring(11)) || !d.is<JsonObject>()) return "ERR channel do {JSON}";
    return channelCommand(d.as<JsonObjectConst>());
  }
  if (line == "hello") return meshRadio.sendHello() ? "OK hello queued" : "ERR hello failed";
  if (line == "resends") return meshRadio.resendsJson(); // diagnostics: resent messages and their routes
  if (line == "events") return config.usbEvents ? "OK events on" : "OK events off";
  if (line == "events on" || line == "events off") {
    Config next = config;
    next.usbEvents = line.endsWith("on");
    if (!next.save()) return "ERR storage: setting not saved";
    config = next;
    return config.usbEvents ? "OK events on" : "OK events off";
  }
  if (line == "position") return meshRadio.sendPosition() ? "OK position queued" : "ERR position needs GPS fix";
  if (line == "txframe") return meshRadio.diagnosticFrame();
  if (line.startsWith("ingest ")) {
    String hex = line.substring(7);
    if (hex.length() % 2 || hex.length() > meshmesh::MaxPacket * 2) return "ERR frame size";
    uint8_t bytes[meshmesh::MaxPacket];
    for (unsigned i = 0; i < hex.length() / 2; i++) {
      String part = hex.substring(i * 2, i * 2 + 2);
      char* end;
      unsigned long v = strtoul(part.c_str(), &end, 16);
      if (*end || !isxdigit(part[0]) || !isxdigit(part[1])) return "ERR hex";
      bytes[i] = v;
    }
    return meshRadio.diagnosticIngest(bytes, hex.length() / 2) ? "OK diagnostic frame accepted (USB, not RF)"
                                                               : "ERR diagnostic frame rejected";
  }
  if (line == "recalibrate") return meshRadio.recalibrate() ? "OK radio set up again" : "ERR radio busy with a packet";
  if (line == "selftest") return meshRadio.selfTest() ? "OK crypto/UTF-8/tamper selftest" : "ERR selftest";
  if (line == "wifi") {
    portalToggle();
    return portalActive() ? "OK Wi-Fi portal on; credentials on device" : "OK Wi-Fi off";
  }
  if (line == "remote" || line.startsWith("remote ")) return remote::usbCommand(line);
  if (line == "ble") {
    bleToggle();
    return bleActive() ? "OK BLE on" : "OK BLE off";
  }
  if (line == "restart") {
    restartAt = millis() + 1000;
    return "OK restarting";
  }
  if (line == "poweroff") return powerOff(); // e.g. after fsformat: the settings are read at boot
  if (line == "flashstatus") return flashInfo();
  if (line == "flashprobe") {
    if (meshRadio.busy()) return "ERR radio busy";
    return flashProbe(hardware.fsOk);
  }
  if (line == "fsformat") {
    if (hardware.fsOk) return "ERR filesystem already mounted; no format";
    if (meshRadio.busy()) return "ERR radio busy";
    String reply = formatStorage();
    hardware.fsOk = reply.startsWith("OK");
    return reply;
  }
  if (line.startsWith("resetpath ") || line.startsWith("forget ")) { // node card actions, as on the M9 screen
    bool reset = line.startsWith("resetpath ");
    String hex = line.substring(reset ? 10 : 7);
    char* end = nullptr;
    uint64_t id = strtoull(hex.c_str(), &end, 16);
    if (!id || !end || *end || hex.length() > 16) return "ERR node ID";
    if (reset) return meshRadio.resetPath(id) ? "OK path reset; next message floods" : "ERR path reset failed";
    return meshRadio.removeContact(id) ? "OK contact removed; its next advert adds it again"
                                       : "ERR contact not removed";
  }
  // The app over USB or BLE: text with line breaks, which a command line cannot carry.
  if (line.startsWith("sendjson ")) {
    StaticJsonDocument<1024> d;
    if (deserializeJson(d, line.substring(9)) || !d["to"].is<const char*>() || !d["text"].is<const char*>())
      return "ERR sendjson {\"to\":\"ALL|NODE_ID\",\"text\":\"...\"}";
    return executeCommand("send " + d["to"].as<String>() + " " + d["text"].as<String>());
  }
  if (line.startsWith("send ")) {
    int at = line.indexOf(' ', 5);
    if (at < 0) return "ERR send ALL|NODE_ID text";
    String to = line.substring(5, at);
    uint64_t id = meshmesh::Broadcast;
    if (to != "ALL") {
      char* end = nullptr;
      id = strtoull(to.c_str(), &end, 16);
      if (!id || !end || *end || to.length() > 16) return "ERR node ID";
    }
    return meshRadio.sendMessage(line.substring(at + 1), id) ? "OK message queued" : "ERR message rejected";
  }
  if (line.startsWith("set ")) {
    DynamicJsonDocument d(2048);
    if (deserializeJson(d, line.substring(4)) || !d.is<JsonObject>()) return "ERR set {JSON object}";
    return applySettings(d.as<JsonObjectConst>());
  }
  return "Commands: status, config, key, identity, identity set HEX, contact add {JSON}, connections, messages, quick, quick do {JSON}, events on|off, wardrive, wardrive do {JSON}, wardrive log mesh|nets FROM, radar, radar web, radar do {JSON}, set {JSON}, radio try {JSON}|keep|revert|previous, send ALL|NODE_ID|CHANNEL_ID text, sendjson {JSON}, channels, channel do {JSON}, regions, regions find, netfind, netfind start|stop|use N, hello, position, resetpath NODE_ID, forget NODE_ID, selftest, wifi, internet, ble, remote, remote login|status|cli|trace NODE_ID [text], recalibrate, fsformat, restart, poweroff";
}
