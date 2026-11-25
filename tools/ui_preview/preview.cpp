#include <iostream>
// Host renderer for the production screen UI. It links the real src/Ui.cpp
// (or src/UiHeltec.cpp) against in-memory state and writes PPM frames, so
// layout can be reviewed without a device. It is not a hardware check.
#include "App.h"
#include "HistoryReply.h"
#include "Notifications.h"
#include "QuickSend.h"
#include "BlobStore.h"
#include "Hardware.h"
#include "Palette.h"
#include "MeshRadio.h"
#include "Wardrive.h"
#include "Remote.h"
#include "Maps.h"
#include "Navigation.h"
#include "Wire.h"
#include <time.h>
#include <string>
#include <vector>
HardwareSerialShim Serial;
EspShim ESP;
SPIClass SPI;
TwoWire Wire, Wire1;
size_t Print::print(const String& s) {
  return write((const uint8_t*)s.c_str(), s.length());
}
static uint32_t fakeMillis = 100000;
uint32_t millis() {
  return fakeMillis;
}
Hardware hardware;
MeshRadio meshRadio;
Maps maps;
Navigation navigation;
static bool wifiOn = false, bleOn = false;
bool portalActive() {
  return wifiOn;
}
String portalPassword() {
  return "preview-pass";
}
void portalToggle() {
  wifiOn = !wifiOn;
}
bool bleActive() {
  return bleOn;
}
void bleToggle() {
  bleOn = !bleOn;
}
uint32_t blePin() {
  return 123456;
}
// Repeater and room sessions (src/Remote.inc): fake answers, set up with the nodes below.
namespace remote {
Session sessions[MaxSessions];
Trace trace;
Session* find(uint64_t id) {
  for (auto& x : sessions)
    if (x.id && x.id == id) return &x;
  return nullptr;
}
bool login(uint64_t, const String&, bool) {
  return true;
}
bool status(uint64_t) {
  return true;
}
bool command(uint64_t, const String&) {
  return true;
}
bool traceTo(uint64_t) {
  return true;
}
bool traceable(uint64_t id) {
  Peer* p = nullptr;
  for (unsigned i = 0; i < meshRadio.peerCount; i++)
    if (meshRadio.peers[i].id == id) p = &meshRadio.peers[i];
  return p && (p->type == 2 || (p->pathLength != 255 && (p->pathLength & 63)));
}
bool saved(uint64_t) {
  return true;
}
void tick() {}
String json() {
  return "{}";
}
String usbCommand(const String&) {
  return "OK";
}
} // namespace remote
String bleName() {
  return String("MeshCore-") + config.name + " 5EA1";
}
void bleRename() {}
#include "Regions.h"
#include "../../src/AppSettings.inc"
bool MeshRadio::applyConfig() {
  return true;
}
#ifdef MM_DESKTOP_SIMULATOR
#include "simulator_settings.inc"
#else
bool Config::save() {
  return true;
}
#endif
#include "private_records.inc"
static uint8_t screenBrightness = 180;
// Hardware
void Hardware::brightness(uint8_t level) {
  screenBrightness = level;
}
void Hardware::beep() {}
void Hardware::setGps(bool enabled) {
  gps.location.valid = enabled;
}
void Hardware::flush() {}
void Hardware::text(int x, int y, const String& v, uint16_t color) {
  font.setForegroundColor(color);
  font.setCursor(x, y);
  font.print(v);
}
void Hardware::line(int y, const String& v, uint16_t color) {
  text(12, y, v, color);
}
// Radio
static uint32_t nextId = 900;
bool MeshRadio::sendMessage(const String& text, uint64_t destination) {
  ChatMessage m;
  m.source = nodeId;
  m.destination = destination;
  m.outgoing = true;
  m.status = ChatMessage::Queued;
  m.timestamp = time(nullptr);
  m.id = ++nextId;
  strlcpy(m.name, config.name, sizeof m.name);
  strncpy(m.text, text.c_str(), 160);
  addMessage(m, true);
  event = "Queued: waiting for delivery";
  return true;
}
bool MeshRadio::sendHello() {
  txCount++;
  return true;
}
static uint32_t previewProbe = 0;
uint32_t MeshRadio::sendProbe(const String&, uint64_t channel) {
  txCount++;
  return channels::isChannel(channel) ? ++previewProbe : 0;
}
bool MeshRadio::sendPosition() {
  return hardware.gpsFix();
}
bool MeshRadio::selfTest() {
  return true;
}
bool MeshRadio::busy() const {
  return false;
}
String MeshRadio::idText(uint64_t id) const {
  if (id == meshmesh::Broadcast) return "ALL";
  char b[17];
  snprintf(b, sizeof b, "%012llX", (unsigned long long)id);
  return b;
}
String MeshRadio::publicKeyText() const {
  return "5a1f0c9e77d24b0e8a41c3f2d9b6e0717a3c55e2b1d04f86c9e2a7b3d1f06e44";
}
unsigned MeshRadio::messageLimit(uint64_t d) const {
  return d == meshmesh::Broadcast ? 151 - 4 : 151;
}
void MeshRadio::tick() {
  compactHistoryTick();
}
void MeshRadio::pathJson(JsonObject, const ChatMessage&) {}
void MeshRadio::pathRead(JsonObjectConst, ChatMessage&) {}
Peer* MeshRadio::contact(uint64_t id) {
  for (unsigned i = 0; i < peerCount; i++)
    if (peers[i].id == id) return &peers[i];
  return nullptr;
}
#include "preview_hooks.inc"
// Maps: a synthetic street grid instead of SD tiles.
void Maps::begin() {}
void Maps::tick() {}
void Maps::center(double lat, double lon) {
  latitude = lat;
  longitude = lon;
  haveCenter = true;
  dirty = true;
}
void Maps::pan(int dx, int dy) {
  follow = false;
  double scale = 256.0 * (1 << zoom);
  longitude += dx * 360.0 / scale;
  latitude -= dy * 360.0 / scale * cos(latitude * M_PI / 180);
  dirty = true;
}
void Maps::changeZoom(int d) {
  zoom = constrain(int(zoom) + d, 10, 17);
  dirty = true;
}
String Maps::areas() {
  return "[{\"id\":\"utrecht\",\"name\":\"Utrecht\",\"tiles\":1022},{\"id\":\"center\",\"name\":\"Centre 4×4 km\",\"tiles\":218}]";
}
bool Maps::selectArea(const String&) {
  title = "Utrecht";
  return true;
}
String Maps::info() {
  return "{}";
}
void Maps::draw(int x, int y, int w, int h) {
  auto& c = *hardware.canvas;
  visibleTiles = available && haveCenter ? 4 : 0;
  waiting = false;
  if (!visibleTiles) return;
  c.fillRect(x, y, w, h, 0xef5b);
  for (int i = 0; i < 14; i++) {
    int ry = i * 29 + (zoom * 7) % 29, rx = i * 37 + (zoom * 5) % 37;
    if (ry < h - 4) c.fillRect(x, y + ry, w, 4, 0xffff);
    if (rx < w - 3) c.fillRect(x + rx, y, 3, h, 0xffff);
  }
  c.fillRect(x + 40, y + 60, 70, 40, 0xb7d6);
  c.fillRect(x + 180, y + 20, 60, 90, 0xc618);
  c.fillRect(x + 220, y + 110, 90, 30, 0x9e7f);
  c.fillRect(x, y + h - 40, w, 14, 0xfee0);
}
void Navigation::begin() {}
void Navigation::tick() {}
void Navigation::start() {
  calibrating = true;
}
bool Navigation::finish() {
  calibrating = false;
  return true;
}
String Navigation::info() {
  return "{}";
}
static void save(const std::string& path) {
  auto& c = *hardware.canvas;
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", c.width(), c.height());
  for (int y = 0; y < c.height(); y++)
    for (int x = 0; x < c.width(); x++) {
      uint16_t p = c.getPixel(x, y);
      uint8_t rgb[3] = {uint8_t((p >> 11 & 31) * 255 / 31), uint8_t((p >> 5 & 63) * 255 / 63),
                        uint8_t((p & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
}
static std::string outDir;
static void tick() {
  fakeMillis += 1000;
  radar.tick();
  uiTick();
}
// Wardrive along a street: packets heard through repeater 3A and a zero-hop advert, pings repeated by C7 and 3A,
// the middle one by nobody.
static void wardriveData() {
  const double track[][2] = {
      {52.0963, 5.1088}, {52.0968, 5.1112}, {52.0973, 5.1137}, {52.0979, 5.1161}, {52.0984, 5.1186}};
  const uint8_t viaA[] = {0x3A}, viaB[] = {0x3A, 0xC7}, advert[] = {0xB2, 0x41, 0x09};
  auto saved = hardware.gps.location;
  for (unsigned i = 0; i < 5; i++) {
    hardware.gps.location.la = track[i][0];
    hardware.gps.location.lo = track[i][1];
    wardrive::heard(0x1000 + i, 5, true, 1, viaA, nullptr, 0, 6.5f - i * 3, -92 - int(i) * 4);
    wardrive::heard(0x2000 + i, 4, true, 0, nullptr, advert, 3, -2.0f, -104);
    fakeMillis += 130000;
    if (wardrive::ping()) {
      wardrive::probeSent(previewProbe, 0x5000 + i);
      if (i != 2) wardrive::echo(0x5000 + i, 2, viaB, 9.0f - i * 3);
      if (i != 2 && i != 3) wardrive::echo(0x5000 + i, 1, viaA, 3.0f);
      fakeMillis += 31000;
      wardrive::tick();
    }
  }
  hardware.gps.location = saved;
}
static void key(int k) {
  uiKey(k);
  tick();
}
static void shot(const char* name) {
  tick();
  std::string n = name;
  save(outDir + "/" + n + ".ppm");
  printf("%s\n", n.c_str());
}
static uint64_t peerId(int i) {
  return (uint64_t(0x1F + i * 0x2B) << 56) | 0xB2C3D4E5F600ULL | uint64_t(i * 0x1111);
} // 8-byte IDs as MeshCore keys give
// Start from normal mode, then exercise the actual clipboard and device-name editor.
static void showPasteFailure() {
  key(0x81);
  key(13); // Public chat
  for (char c : std::string("Friday field team meeting point")) key(c);
  key(0xa3);
  key('c');
  key(0x90);
  key(0xb6);
  key(13);
  key(13); // Screen & device, Name
  key(0xa3);
  key('r');
}
static void scenario();
#include "wishlist_checks.inc"
#ifdef MM_DESKTOP_SIMULATOR
#include "desktop_simulator.inc"
#endif
int main(int argc, char** argv) {
  outDir = argc > 1 ? argv[1] : ".";
  config.lang = argc > 2 ? max(0, langFromCode(argv[2])) : LangEn; // a language code, English by default
#ifdef MM_DESKTOP_SIMULATOR
  config.lang = LangEn;
#endif
  hardware.canvas = new GFXcanvas16(320, 240);
  strcpy(config.name, "M9");
  hardware.font.begin(*hardware.canvas);
  hardware.font.setFont(u8g2_font_6x13_t_cyrillic);
  hardware.font.setFontMode(1);
  hardware.keyboardOk = hardware.sdOk = hardware.fsOk = hardware.rtcOk = hardware.rtcValid = hardware.compassOk =
      hardware.imuOk = hardware.compassSample = hardware.imuSample = true;
  hardware.batteryMv = 3980;
  hardware.gps.location = {true, 52.0963, 5.1088};
  hardware.gps.satellites.v = 9;
  hardware.gps.hdop.v = 1.1;
  hardware.gps.date.valid = true;
  hardware.gps.sentences = 1200;
  meshRadio.ready = true;
  meshRadio.nodeId = 0x5A1F0C9E77D2A1B3ULL;
  meshRadio.rxCount = 148;
  meshRadio.txCount = 37;
  meshRadio.relayed = 12;
  meshRadio.rejected = 2;
  meshRadio.lastRssi = -71;
  meshRadio.lastSnr = 9.5;
  meshRadio.lastRxAt = fakeMillis - 20000;
  // Node names fit Peer::name (24 bytes, as the firmware allows).
  const char* names[] = {"Heltec V4", "Utrecht RPT-1", "Room Utrecht", "Alex T-Deck", "Sensor-12", "Sam"};
  uint8_t types[] = {1, 2, 3, 1, 4, 1};
  for (int i = 0; i < 6; i++) {
    auto& p = meshRadio.peers[meshRadio.peerCount++];
    p.id = peerId(i);
    strcpy(p.name, names[i]);
    p.type = types[i];
    p.heard = i != 4;
    p.seen = fakeMillis - (i * 47000 + 3000);
    p.rssi = -58 - i * 11;
    p.snr = 11 - i * 3.5f;
    p.hops = i == 0 ? 0 : i;
    p.pathLength = i == 0 ? 0 : i == 2 ? 255 : i;
    p.position = i == 0 || i == 1 || i == 3;
    p.latitude = 52.0963 + 0.0011 * (i + 1) * (i % 2 ? 1 : -1);
    p.longitude = 5.1088 + 0.0013 * (i + 1) * (i % 3 ? 1 : -1);
    for (int k = 0; k < 8; k++) p.publicKey[k] = uint8_t(p.id >> (56 - 8 * k));
    p.publicKey[8] = 0x10 * i + 3;
  }
  {
    auto& r = remote::sessions[0];
    r.id = peerId(1);
    r.login = r.status = r.command = remote::Done;
    r.admin = true;
    r.battery = 4170;
    r.uptime = 93780;
    r.received = 1520;
    r.sent = 611;
    r.airtime = 4210;
    r.noise = -112;
    r.rssi = -79;
    r.snr = 38;
    strcpy(r.reply[0], "> Utrecht RPT-1");
    strcpy(r.reply[1], "v1.17.1-meshmesh (Build: Oct  8 2026)");
    r.replies = 2;
    auto& tr = remote::trace;
    tr.id = peerId(1);
    tr.state = remote::Done;
    tr.hops = 1;
    tr.hashes[0] = uint8_t(peerId(1) >> 56);
    tr.snr[0] = 49;
    tr.snr[1] = 34;
  }
  time_t now = time(nullptr);
  auto add = [&](uint64_t src, uint64_t dst, const char* name, const char* text, bool out, ChatMessage::Status st,
                 int ago) {
    ChatMessage m;
    m.source = src;
    m.destination = dst;
    strcpy(m.name, name);
    strcpy(m.text, text);
    m.outgoing = out;
    m.status = st;
    m.timestamp = now - ago;
    m.id = ++nextId;
    meshRadio.history[meshRadio.historyCount++] = m;
  };
  uint64_t B = meshmesh::Broadcast;
  add(peerId(5), B, "Sam", "Hi all! 👋 Anyone on air near the Dom tower?", false, ChatMessage::Received, 5400);
  add(meshRadio.nodeId, B, "M9", "Here, M9 at Neude", true, ChatMessage::Sent, 5300);
  add(peerId(0), meshRadio.nodeId, "Heltec V4", "Radio test V4 → M9: 1790746076", false, ChatMessage::Received, 3600);
  add(meshRadio.nodeId, peerId(0), "M9", "Copy, loud and clear. SNR about 9 dB", true, ChatMessage::Delivered, 3500);
  add(meshRadio.nodeId, peerId(0), "M9",
      "Testing a long message: the lines should wrap neatly inside the bubble on this screen", true,
      ChatMessage::Failed, 1200);
  add(peerId(0), meshRadio.nodeId, "Heltec V4", "OK 👍🏽", false, ChatMessage::Received, 300);
  add(peerId(3), meshRadio.nodeId, "Alex T-Deck", "Be there in 10 minutes 🚗💨", false, ChatMessage::Received, 120);
  add(meshRadio.nodeId, peerId(3), "M9", "Waiting at the entrance 😊☕️", true, ChatMessage::Sent, 60);
  // Channels: Public, a hashtag and a private one with messages; two more heard on air.
  meshRadio.channelCount = 1;
  strcpy(meshRadio.channelList[0].name, "Public");
  memcpy(meshRadio.channelList[0].secret, channels::publicSecret, 16);
  meshRadio.channelList[0].id = meshmesh::Broadcast;
  {
    uint64_t tag, priv;
    meshRadio.joinHashtag("utrecht", &tag);
    uint8_t k[16];
    for (int i = 0; i < 16; i++) k[i] = uint8_t(200 - i * 9);
    meshRadio.addChannel("Friends", k, &priv);
    add(peerId(3), tag, "Alex T-Deck", "Who is cycling to the lake on Saturday?", false, ChatMessage::Received, 2400);
    add(peerId(5), priv, "Sam", "Meet at 19:00 🔥🎉 🇳🇱🦖", false, ChatMessage::Received, 900);
    meshRadio.heard[0].hash = 0x5A;
    meshRadio.heard[0].packets = 3;
    meshRadio.heard[0].at = fakeMillis - 120000;
    strcpy(meshRadio.heard[0].name, "#nl");
    meshRadio.heard[1].hash = 0x07;
    meshRadio.heard[1].packets = 1;
    meshRadio.heard[1].at = fakeMillis - 600000;
    meshRadio.heardCount = 2;
    meshRadio.heardSamples = 4;
    channels::Channel invite{};
    strcpy(invite.name, "Hiking");
    for (int i = 0; i < 16; i++) invite.secret[i] = uint8_t(i * 13 + 5);
    add(peerId(5), meshRadio.nodeId, "Sam", channels::link(invite).c_str(), false, ChatMessage::Received, 200);
  }
  maps.available = true;
  maps.tileCount = 1240;
  maps.title = "Utrecht";
  maps.zoom = 15;
  maps.center(52.0963, 5.1088);
  navigation.headingValid = true;
  navigation.heading = 37;
  navigation.calibrated = true;
  quickSend::begin();
  wardrive::begin();
#ifdef MM_DESKTOP_SIMULATOR
  bool test = outDir == "--self-test";
  bool proposal = outDir == "--radio-proposal";
  bool pasteDemo = outDir == "--paste-demo";
  bool checkReload = outDir == "--check-saved-settings";
  if (checkReload && argc != 3) return 1;
  if ((test || checkReload) && argc == 3) {
    if (!loadSimulatorSettings(std::string(argv[2]) + "/")) return 1;
  } else if (!test && !pasteDemo && !proposal) {
    char* path = SDL_GetPrefPath("MeshMesh", "M9 Simulator");
    if (!path) {
      fprintf(stderr, "Simulator settings folder: %s\n", SDL_GetError());
      return 1;
    }
    std::string directory = path;
    SDL_free(path);
    if (!loadSimulatorSettings(directory)) return 1;
  }
  if (checkReload) {
    if (!savedSettingsMatch()) {
      fprintf(stderr, "Simulator settings did not survive restart\n");
      return 1;
    }
    puts("Screen, device name, sound, and radio settings survived a fresh simulator process.");
  }
  hardware.brightness(config.brightness);
  hardware.setGps(config.gps);
  uiBegin();
  if (proposal) {
    config.frequency = 869.618f;
    config.sf = 7;
    config.cr = 5;
    config.autoLock = 0;
    config.dimAfter = 0;
    key(13);
  }
  if (pasteDemo) {
    config.dimAfter = 0;
    config.autoLock = 0; // Leave the example on screen for the user to explore.
    key(13);
    showPasteFailure();
  }
  return runSimulator(test || checkReload, checkReload);
#else
  wishlistChecks();
  uiBegin();
  scenario();
  return 0;
#endif
}
#include "scenario.inc"
