// Radio actions added by the UI; kept here so the preview links.
bool MeshRadio::resetPath(uint64_t id) {
  Peer* p = contact(id);
  if (!p) return false;
  p->pathLength = 255;
  return true;
}
bool MeshRadio::removeContact(uint64_t id) {
  Peer* p = contact(id);
  if (!p) return false;
  unsigned i = p - peers;
  memmove(peers + i, peers + i + 1, sizeof(Peer) * (peerCount - i - 1));
  peerCount--;
  return true;
}
void Hardware::ping(uint16_t, uint16_t) {}
// Signal radar: synthetic sweep results; the table and trend logic are the real src/RadarModel.cpp.
#include "Radar.h"
Radar& radar = *new Radar;
void Radar::open() {
  if (active) return;
  active = true;
  wifi = WifiReady;
  sweeps = 3;
  count = 0;
  struct {
    uint64_t id;
    const char* name;
    uint8_t ch;
    bool open;
    int rssi;
  } aps[] = {{0x5A1F0C9E1101ULL, "MM-9E77D2", 1, false, -41},     {0x34E8941A2B3CULL, "Ziggo-4471", 6, false, -58},
             {0xA0F3C1223344ULL, "TP-Link_5GHz", 11, false, -66}, {0x10FEED000001ULL, "", 3, false, -72},
             {0x28CDC1ABCDEFULL, "Cafe de Zaak", 1, true, -79},   {0xB827EB000102ULL, "DIRECT-HP-M28", 6, false, -88}};
  for (auto& a : aps) {
    RadarTarget* t = upsert(RadarTarget::Wifi, a.id);
    strcpy(t->name, a.name);
    t->channel = a.ch;
    t->open = a.open;
    t->rssi = a.rssi;
    t->seen = millis();
  }
  struct {
    uint64_t id;
    const char* name;
    RadarTarget::Device device;
    uint16_t vendor;
    int rssi;
  } bles[] = {{0x6A11, "", RadarTarget::Personal, 0x004c, -52},
              {0x6A12, "Galaxy Watch6", RadarTarget::Watch, 0x0075, -61},
              {0x6A13, "", RadarTarget::Audio, 0x004c, -70},
              {0x6A14, "MeshMesh 0E44", RadarTarget::Unknown, 0xffff, -47},
              {0x6A15, "", RadarTarget::Unknown, 0x0006, -83}};
  for (auto& b : bles) {
    RadarTarget* t = upsert(RadarTarget::Ble, b.id);
    strcpy(t->name, b.name);
    t->device = b.device;
    t->vendor = b.vendor;
    t->rssi = b.rssi;
    t->seen = millis() - (b.id & 3) * 7000;
  }
  ble = BleReady;
  for (unsigned i = 0; i < meshRadio.peerCount; i++) {
    const Peer& p = meshRadio.peers[i];
    if (!p.heard || p.hops) continue;
    RadarTarget* t = upsert(RadarTarget::Lora, p.id);
    strcpy(t->name, p.name);
    t->rssi = p.rssi;
    t->seen = p.seen;
  }
  sort();
}
void Radar::close() {
  setCsi(CsiOff);
  untrack();
  active = false;
  count = 0;
  wifi = WifiOff;
  ble = BleOff;
}
void Radar::release() {}
void Radar::releaseBle() {}
void Radar::tick() {
  if (csi == CsiSensor) csiLast = millis();
}
// CSI: a still room, then someone walking between the boards.
void Radar::setCsi(CsiRole role) {
  if (role == csi) return;
  csi = role;
  csiRunning = role != CsiOff;
  csiRate = role == CsiOff ? 0 : role == CsiBeacon ? 50 : 48;
  motionSamples = 0;
  moving = false;
  if (role == CsiSensor) {
    csiRssi = -41;
    csiLast = millis();
    baseline = .004f;
    for (int i = 0; i < 100; i++) {
      float v = i < 70 ? .0035f + .0015f * ((i * 37) % 7) / 6 : .018f + .03f * ((i * 53) % 9) / 8;
      addActivity(v, millis() - (100 - i) * 500);
    }
  }
}
bool Radar::track(unsigned index) {
  if (index >= count) return false;
  focus = targets[index];
  reset();
  tracking = true;
  // A walk towards the access point: noisy approach, then a short step back.
  for (int i = 0; i < 90; i++) {
    int r = -78 + i / 3 + int((i * 37) % 7) - 3 - (i > 80 ? (i - 80) * 2 : 0);
    addSample(r, millis() - (90 - i) * 100);
  }
  lastSample = millis() - 300;
  rate = 21;
  return true;
}
void Radar::untrack() {
  tracking = false;
  reset();
}
#include <vector>
bool MeshRadio::learnContact(const uint8_t*, const char*, uint8_t, int32_t, int32_t) {
  return true;
}
// Internet over Wi-Fi and the web radar holder: switched off in the preview.
#include "Internet.h"
Internet internet;
static int previewRescans = 0; // the T-Deck touch check: DEL in the network list rescans
void Internet::setEnabled(bool on) {
  enabled = on;
}
void Internet::rescan() {
  previewRescans++;
}
void Internet::connectTo(const String&) {}
bool Internet::save(const String&, const String&) {
  return true;
}
bool Internet::forget(const String&) {
  return true;
}
String Internet::savedName(unsigned) {
  return "";
}
String Internet::stateText() {
  return online() ? String("Home-WiFi · 192.168.1.101") : tr("Off");
}
String Internet::address() {
  return "192.168.1.101";
}
bool Maps::inArea() const {
  return true;
}
bool webRadarActive() {
  return false;
}
static String previewCommands; // commands the screens ran, for the checks
// Power off (Power.cpp): the screens only; the M9 wakes by RESET, the others by a held button.
static bool previewOff = false;
String powerOff() {
  previewOff = true;
  return "OK turning off";
}
bool powerOffPending() {
  return previewOff;
}
int powerOnWay() {
  return -1;
}
String executeCommand(const String& line) {
  previewCommands += line + ";";
  if (line.startsWith("radio ")) return radioCommand(line);
  return "OK";
} // UiHeltec.cpp: the GAT562 GPS menu
// Same as src/MeshRadio.cpp (the preview does not link the radio).
String MeshRadio::routeText(const ChatMessage& m, bool brief) const {
  if (m.protocol != 2 || m.route == ChatMessage::RouteNone ||
      (m.outgoing && (m.status == ChatMessage::Failed || channels::isChannel(m.destination))))
    return "";
  String hops = m.hops == 255 ? String()
                : !m.hops     ? String(brief ? tr("direct") : tr("direct"))
                              : (brief ? String(m.hops) + tr(" rpt") : String(tr("via ")) + m.hops + tr(" rpt"));
  String s = m.route == ChatMessage::RouteDirect ? (hops.length() ? hops : String(tr("routed")))
             : !m.outgoing                       ? (hops.length() ? hops : String("flood")) // incoming: how far it came
             : m.status == ChatMessage::Delivered && hops.length() && !brief ? "flood, " + hops
                                                                             : String("flood");
  if (m.outgoing && m.tries > 1)
    s += m.status == ChatMessage::Delivered ? (brief ? " #" + String(m.tries) : String(tr(" · try ")) + m.tries)
                                            : " " + String(m.tries) + "/3";
  return s;
}
#include "Regions.h"
// Channels: the table in memory (src/MeshRadio.cpp keeps it in NVS and gives it to MeshCore); the names,
// keys, links and QR codes are the real src/Channels.cpp.
const channels::Channel* MeshRadio::channel(uint64_t id) const {
  int i = channelIndex(id);
  return i < 0 ? nullptr : &channelList[i];
}
int MeshRadio::channelIndex(uint64_t id) const {
  for (unsigned i = 0; i < channelCount; i++)
    if (channelList[i].id == id) return i;
  return -1;
}
MeshRadio::ChannelResult MeshRadio::addChannel(const String& name, const uint8_t secret[16], uint64_t* id) {
  uint64_t key = channels::idOf(secret);
  if (id) *id = key;
  if (channel(key)) return ChannelExists;
  if (!channels::validName(name)) return ChannelBadName;
  if (channelCount >= channels::Max) return ChannelFull;
  auto& c = channelList[channelCount++];
  c = {};
  strncpy(c.name, name.c_str(), channels::NameBytes);
  memcpy(c.secret, secret, 16);
  c.id = key;
  uint8_t hash = channels::hashOf(secret);
  for (unsigned i = 0; i < heardCount;)
    if (heard[i].hash == hash) {
      memmove(heard + i, heard + i + 1, sizeof(HeardChannel) * (heardCount - i - 1));
      heardCount--;
    } else
      i++;
  return ChannelAdded;
}
MeshRadio::ChannelResult MeshRadio::joinHashtag(const String& raw, uint64_t* id) {
  String tag = channels::hashtag(raw);
  if (!tag.length()) return ChannelBadName;
  uint8_t k[16];
  channels::hashtagSecret(tag, k);
  return addChannel(tag, k, id);
}
MeshRadio::ChannelResult MeshRadio::joinLink(const String& text, uint64_t* id) {
  String name;
  uint8_t k[16];
  if (!channels::parseLink(text, name, k)) return ChannelBadLink;
  return addChannel(name, k, id);
}
MeshRadio::ChannelResult MeshRadio::createChannel(const String& name, uint64_t* id) {
  if (!channels::validName(name)) return ChannelBadName;
  uint8_t k[16];
  for (int i = 0; i < 16; i++) k[i] = uint8_t(i * 37 + 11);
  return addChannel(name, k, id);
}
bool MeshRadio::removeChannel(uint64_t id) {
  int i = channelIndex(id);
  if (i <= 0) return false;
  memmove(channelList + i, channelList + i + 1, sizeof(channels::Channel) * (channelCount - i - 1));
  channelCount--;
  return true;
}
bool MeshRadio::sendInvite(uint64_t contact, uint64_t id) {
  const channels::Channel* c = channel(id);
  if (!c) return false;
  String l = channels::link(*c);
  if (l.length() > 151) l = channels::link(*c, true);
  return sendMessage(l, contact);
}
bool MeshRadio::setChannelRegion(uint64_t id, const String& raw) {
  int i = channelIndex(id);
  if (i < 0) return false;
  String r = raw;
  r.trim();
  if (r.length() && r != regions::Unscoped) {
    r = regions::normalize(r);
    if (!r.length()) return false;
  }
  strncpy(channelList[i].region, r.c_str(), sizeof(channelList[i].region) - 1);
  return true;
}
// Region search: the answer of two repeaters of the Moscow scheme (docs/regions.md), at once.
namespace regions {
Search search;
bool find() {
  static Found list[MaxFound];
  search = Search();
  search.found = list;
  search.state = Search::Done;
  search.repeaters = search.answered = 2;
  for (const char* n : {"ru", "mow", "msk", "sao"}) {
    strcpy(search.found[search.count].name, n);
    search.found[search.count++].repeaters = 2;
  }
  return true;
}
void tick() {}
String json() {
  return "{}";
}
} // namespace regions
// Network search: an answer with a regional preset found, at once (src/NetFind.inc on the boards).
#include "NetFind.h"
namespace netfind {
State state;
bool start() {
  static Result list[MaxResults];
  state = State();
  state.results = list;
  RadioProfile here = config.radio(), near = here;
  near.sf = 7;
  near.cr = 7;
  list[0] = Result();
  list[0].radio = here;
  list[0].current = true;
  list[0].done = true;
  list[1] = Result();
  list[1].radio = near;
  list[1].place = 1;
  list[1].repeaters = 3;
  list[1].packets = 5;
  list[1].snr = 26;
  list[1].done = true;
  state.count = 2;
  state.best = 1;
  state.phase = State::Done;
  return true;
}
void stop() {}
void cancel() {}
void tick() {}
String json() {
  return "{}";
}
String placeName(uint8_t place) {
  return place == 1 ? "Netherlands" : "";
}
String profileText(const RadioProfile& r) {
  return String(r.frequency, 3) + " SF" + String(r.sf) + " BW" + String(r.bandwidth, 1);
}
String use(uint8_t) {
  return "OK radio test";
}
} // namespace netfind
int MeshRadio::probeHashtag(const String& raw) {
  String tag = channels::hashtag(raw);
  if (!tag.length()) return -1;
  for (unsigned i = 0; i < heardCount; i++)
    if (tag == heard[i].name) return heard[i].packets;
  return tag == "#test" ? 2 : 0;
}
// MeshMesh storage files: none in the preview (read marks fall back to the Preferences shim).
bool readStored(const char*, const char*, void*, size_t) {
  return false;
}
bool writeStored(const char*, const char*, const void*, size_t) {
  return false;
}
bool MeshRadio::clockSet() {
  return time(nullptr) >= 1735689600;
}
