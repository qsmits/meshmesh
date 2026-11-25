#include "App.h"
#include <new>
#include "Hardware.h"
#include "MeshRadio.h"
#include "Remote.h"
#include "Radar.h"
#include "Power.h"
#include "Regions.h"
#include "NetFind.h"
#include "Notifications.h"
#include "QuickSend.h"
#include "Wardrive.h"
#include "Emoji.h"
#include <algorithm>
#include <math.h>
#include <time.h>
// One PRG button: click = next screen or next menu item; hold = the screen's action, or its menu. With the
// double click option (config.doubleBack) a double click steps back.
// Folders (App.h): a folder is a screen of the cycle with its apps listed; a hold enters it, then clicks go
// through its apps and after the last one back to the folder screen; a double click returns to it.
// Folders are made on the web page and in the app.
namespace {
enum Page {
  Home,
  Messages,
  Nodes,
  Signals,
  Gps,
  Wifi,
  Ble,
  Settings,
  Modules,
  NotifyPage,
  QuickPage,
  WardrivePage,
  FolderPage,
  PageCount
};
const char* pageNames[] = {"home",     "messages", "nodes",         "radar", "gps",      "wifi",  "ble",
                           "settings", "modules",  "notifications", "quick", "wardrive", "folder"};
unsigned quickIndex = 0, quickTargetIndex = 0;
bool quickTargets = false, quickReply = false;
uint64_t quickReplyTo = 0;
int page = Home, menuIndex = 0, messageOffset = 0, nodeIndex = 0;
bool menuOpen = false, dirty = true, screenOff = false;
// One button: quick send is entered with a hold, so clicking through the pages passes it.
bool appOpen = false;
bool appPage() {
  return page == QuickPage;
}
bool appClosed() {
  return appPage() && !appOpen;
}
uint32_t drawAt = 0, lastInput = 0, menuAt = 0, actionAt = 0, popupAt = 0, ledAt = 0, pingAt = 0, pingedSamples = 0;
String action;
inline __attribute__((always_inline)) void led(bool on) {
  if (pins::led >= 0) digitalWrite(pins::led, on ? pins::ledOn : !pins::ledOn);
}
ChatMessage incomingPopup{};
uint8_t ledPattern = 0;
unsigned unreadCount = 0;
struct {
  uint64_t source = 0;
  uint32_t session = 0, id = 0;
} newest;
const uint8_t* activeFont = nullptr;
const uint8_t* const small = u8g2_font_5x8_t_cyrillic;
const uint8_t* const body = u8g2_font_6x13_t_cyrillic;
const uint8_t* const bold = u8g2_font_6x13B_t_cyrillic;
#define t(en) String(tr(en)) // a macro: tr() keys its translation at compile time (I18n.h)
// Lengths in 6 px columns: CJK glyphs take two.
unsigned chars(const String& value) {
  unsigned n = 0;
  for (unsigned i = 0; i < value.length();) n += glyphCells(utf8Next(value, i));
  return n;
}
String clipped(const String& value, unsigned count) {
  unsigned i = 0, n = 0, cut = 0;
  while (i < value.length()) {
    unsigned at = i, w = glyphCells(utf8Next(value, i));
    if (n + w > count) {
      i = at;
      break;
    }
    if (n + w + 2 <= count) cut = i;
    n += w;
  }
  return i < value.length() && count > 2 ? value.substring(0, cut) + ".." : value.substring(0, i);
}
void notice(const String& value) {
  action = value;
  actionAt = millis();
  dirty = true;
}
// SetFont resets transparency; the monochrome canvas treats any non-zero colour as lit.
void useFont(const uint8_t* f) {
  if (f != activeFont) {
    hardware.font.setFont(f);
    hardware.font.setFontMode(1);
    activeFont = f;
  }
}
// Emoji (Emoji.h), two columns (and the 1 px gap) wide: 12 px (8 px in the small fonts) from 1 px under the baseline,
// its lit group in `color` on the monochrome panel. An emoji
// without a picture here gets a frame. -1: not an emoji.
int emojiGlyph(int x, int y, uint32_t cp, const uint8_t* f, uint16_t color, bool paint) {
  if (emoji::modifier(cp)) return 0;
  int e = emoji::find(cp);
  if (e < 0) return -1;
  int s = f == small || f == u8g2_font_4x6_t_cyrillic ? 8 : emoji::Size;
  uint8_t px[emoji::Size * emoji::Size];
  if (!paint || !emoji::decode(e, px)) return s + 1;
  emoji::mono(px);
  emoji::draw(px, x, y - s + 1 + s / 12, s,
              [color](int a, int b, uint8_t) { hardware.canvas->drawPixel(a, b, color); });
  return s + 1;
}
// Glyph by glyph: what the font lacks comes from its fallback fonts (I18n.h); Arabic in visual order.
int glyph(int x, int y, uint32_t cp, const uint8_t* f, uint16_t color, bool paint) {
  if (char l = emoji::flagLetter(cp)) cp = uint8_t(l);
  if (cp >= 0x203c) {
    int w = emojiGlyph(x, y, cp, f, color, paint);
    if (w >= 0) return w;
  }
  const uint8_t* use = cp <= 0xffff && fontHasGlyph(f, cp) ? f : nullptr;
  if (!use && cp <= 0xffff)
    for (const uint8_t* const* x = fallbackFonts(f); *x; x++)
      if (fontHasGlyph(*x, cp)) {
        use = *x;
        break;
      }
  if (!use) {
    if (!emoji::pictograph(cp)) return 0;
    if (paint) hardware.canvas->drawRect(x + 1, y - 9, 10, 10, color);
    return 12;
  }
  useFont(use);
  if (!paint) return u8g2_GetGlyphWidth(&hardware.font.u8g2, cp);
  hardware.font.setForegroundColor(color);
  return hardware.font.drawGlyph(x, y, cp);
}
int width(const String& value, const uint8_t* f) {
  String s = visualText(value);
  int w = 0;
  for (unsigned i = 0; i < s.length();) w += glyph(0, 0, utf8Next(s, i), f, 0, false);
  return w;
}
void say(int x, int y, const String& value, const uint8_t* f = body, uint16_t color = 1) {
  String s = visualText(value);
  for (unsigned i = 0; i < s.length();) x += glyph(x, y, utf8Next(s, i), f, color, true);
}
void sayRight(int x, int y, const String& value, const uint8_t* f = small, uint16_t color = 1) {
  say(x - width(value, f), y, value, f, color);
}
// Up to `maximum` 21-column rows, wrapped at spaces; longer text pages every four seconds.
void textLines(const String& value, int y, unsigned maximum) {
  String rows[12];
  unsigned at = 0, count = 0;
  while (at < value.length() && count < 12) {
    unsigned n = 0, end = at, space = 0;
    while (end < value.length() && value[end] != '\n') {
      unsigned next = end;
      uint32_t c = utf8Next(value, next);
      if (n + glyphCells(c) > 21) break;
      n += glyphCells(c);
      end = next;
      if (c == ' ') space = end;
    }
    if (end < value.length() && value[end] != '\n' && space > at) end = space;
    rows[count++] = value.substring(at, end);
    at = end;
    if (at < value.length() && value[at] == '\n') at++;
  }
  unsigned first = count > maximum ? (millis() / 4000) % (count - maximum + 1) : 0;
  for (unsigned i = first; i < count && i < first + maximum; i++) say(0, y + (i - first) * 12, rows[i]);
}
String clockText(time_t at) {
  if (at < 1700000000) return "--:--";
  at += config.utcOffset * 60;
  tm* v = gmtime(&at);
  char b[6];
  snprintf(b, sizeof b, "%02d:%02d", v->tm_hour, v->tm_min);
  return b;
}
String ago(uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 60) return String(s) + t("s");
  if (s < 3600) return String(s / 60) + t("m");
  if (s < 86400) return String(s / 3600) + t("h");
  return String(s / 86400) + t("d");
}
String pathText(const Peer& p) {
  if (p.pathLength == 255) return t("path ?");
  if (!(p.pathLength & 63)) return t("direct");
  return String(p.pathLength & 63) + t(" hops");
}
String typeText(uint8_t type) {
  switch (type) {
  case 2: return t("repeater");
  case 3: return t("room");
  case 4: return t("sensor");
  }
  return t("chat");
}
// The latest answer of a repeater or room, or of a route trace, in two short lines (empty when none).
String remoteLogin(const Peer& p) {
  remote::Session* s = remote::find(p.id);
  if (!s || s->login == remote::Idle) return "";
  switch (s->login) {
  case remote::Waiting: return t("login: waiting");
  case remote::Done: return s->admin ? t("logged in: admin") : t("logged in");
  case remote::Refused: return t("login refused");
  default: return t("login: no answer");
  }
}
String remoteResult(const Peer& p) {
  auto& tr = remote::trace;
  if (tr.id == p.id && tr.state != remote::Idle) {
    if (tr.state == remote::Waiting) return t("trace: waiting");
    if (tr.state != remote::Done) return t("trace: no answer");
    String r;
    for (unsigned i = 0; i < tr.hops; i++) r += String(tr.snr[i] / 4.0f, 0) + ">";
    return t("SNR ") + r + String(tr.snr[tr.hops] / 4.0f, 0);
  }
  remote::Session* s = remote::find(p.id);
  if (!s || s->status == remote::Idle) return "";
  if (s->status != remote::Done) return s->status == remote::Waiting ? t("status: waiting") : t("status: no answer");
  return String(s->battery / 1000.0f, 2) + t("V up ") + String(s->uptime / 3600) + t("h rx ") + String(s->received);
}
bool distanceTo(const Peer& p, float& metres, float& bearing) {
  if (!p.position || !hardware.gpsFix()) return false;
  double la1 = hardware.gps.location.lat() * M_PI / 180, la2 = p.latitude * M_PI / 180,
         dl = (p.longitude - hardware.gps.location.lng()) * M_PI / 180;
  double a = sin((la2 - la1) / 2) * sin((la2 - la1) / 2) + cos(la1) * cos(la2) * sin(dl / 2) * sin(dl / 2);
  metres = 12742000 * atan2(sqrt(a), sqrt(1 - a));
  bearing =
      fmod(atan2(sin(dl) * cos(la2), cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dl)) * 180 / M_PI + 360, 360);
  return true;
}
// Nodes, most recently heard first.
unsigned sortedNodes(unsigned* order) {
  unsigned n = meshRadio.peerCount;
  uint32_t now = millis();
  for (unsigned i = 0; i < n; i++) order[i] = i;
  auto before = [&](const Peer& a, const Peer& b) {
    if (a.heard != b.heard) return a.heard;
    return a.heard && now - a.seen < now - b.seen;
  };
  for (unsigned i = 1; i < n; i++)
    for (unsigned j = i; j > 0 && before(meshRadio.peers[order[j]], meshRadio.peers[order[j - 1]]); j--)
      std::swap(order[j], order[j - 1]);
  return n;
}
Peer* shownNode() {
  unsigned order[24];
  unsigned n = sortedNodes(order);
  if (!n) return nullptr;
  nodeIndex %= n;
  return &meshRadio.peers[order[nodeIndex]];
}
const ChatMessage* shownMessage() {
  if (!meshRadio.historyCount) return nullptr;
  messageOffset = constrain(messageOffset, 0, int(meshRadio.historyCount) - 1);
  return &meshRadio.history[meshRadio.historyCount - 1 - messageOffset];
}
// Signal radar (see Radar.h): the selection follows the target, not its row.
uint64_t signalId = 0;
RadarTarget::Kind signalKind = RadarTarget::Wifi;
bool signalManual = false; // until "Next signal", the strongest
int shownSignal() {
  if (!signalManual && radar.count) {
    signalId = radar.targets[0].id;
    signalKind = radar.targets[0].kind;
    return 0;
  }
  for (unsigned i = 0; i < radar.count; i++)
    if (radar.targets[i].id == signalId && radar.targets[i].kind == signalKind) return i;
  if (!radar.count) return -1;
  signalId = radar.targets[0].id;
  signalKind = radar.targets[0].kind;
  return 0;
}
String signalName(const RadarTarget& r) {
  if (r.name[0]) return r.name;
  if (r.kind == RadarTarget::Ble) switch (r.device) {
    case RadarTarget::Phone: return t("phone");
    case RadarTarget::Watch: return t("watch");
    case RadarTarget::Audio: return t("headphones");
    case RadarTarget::Personal: return t("phone/watch");
    default: return t("BLE device");
    }
  return r.kind == RadarTarget::Wifi ? t("hidden network") : meshRadio.idText(r.id);
}
const char* kindLetter(const RadarTarget& r) {
  return r.kind == RadarTarget::Lora ? "L " : r.kind == RadarTarget::Ble ? "B " : "W ";
}
float signalLevel(int rssi) {
  return constrain((rssi + 100) / 65.f, 0.f, 1.f);
} // -100 .. -35 dBm
bool homingFresh() {
  return radar.fresh();
}
String wifiState() {
  switch (radar.wifi) {
  case Radar::WifiPortal: return t("Wi-Fi: access point");
  case Radar::WifiBusy: return t("Wi-Fi busy");
  case Radar::WifiFailed: return t("Wi-Fi error");
  default: return radar.sweeps ? "" : t("scanning...");
  }
}
String bleState() {
  return radar.ble == Radar::BleBusy ? t("BLE busy") : radar.ble == Radar::BleFailed ? t("BLE error") : "";
}
} // namespace
// The pages after Home, in the default order; the chosen order and the hidden ones: config.apps (App.h).
// The default order: messages, places, tools, then the device itself.
const char* const uiApps[] = {"chats",    "quick", "nodes", "gps",    "radar",
                              "wardrive", "wifi",  "ble",   "health", "settings"};
const uint8_t uiAppCount = sizeof uiApps / sizeof *uiApps;
namespace {
const uint8_t appPages[] = {Messages, QuickPage, Nodes, Gps, Signals, WardrivePage, Wifi, Ble, Modules, Settings};
static_assert(sizeof appPages == sizeof uiApps / sizeof *uiApps, "a page for every app ID");
// Folders: folderAt is the folder of the folder screen. A place in the cycle is a page, or FolderPage|folder<<8
// (showPage takes both).
int folderAt = 0, folderSel = 0;
int folderCode(int f) {
  return FolderPage | f << 8;
}
unsigned folderPages(int f, uint8_t* out) {
  uint8_t e[AppsMax];
  unsigned k = appsShown(e, f);
  for (unsigned i = 0; i < k; i++) out[i] = appPages[e[i]];
  return k;
}
// What a click goes through: Home, then the shown apps and folders in the chosen order (a folder without a shown app is left out).
unsigned pageCycle(int* cycle) {
  unsigned n = 0;
  cycle[n++] = Home;
  uint8_t e[AppEntriesMax], p[AppsMax];
  unsigned k = appsShown(e);
  for (unsigned i = 0; i < k; i++) {
    if (!(e[i] & AppFolder))
      cycle[n++] = appPages[e[i]];
    else if (folderPages(e[i] & ~AppFolder, p))
      cycle[n++] = folderCode(e[i] & ~AppFolder);
  }
  return n;
}
// The folder of the app shown when that folder is in the cycle and shows the app (however the page was opened:
// a click, a menu, news), else -1.
int pageFolder() {
  for (uint8_t i = 0; i < uiAppCount && page != FolderPage; i++)
    if (appPages[i] == page) {
      int f = appsFolderOf(i), c[AppEntriesMax + 1];
      unsigned n = f < 0 ? 0 : pageCycle(c);
      for (unsigned k = 0; k < n; k++)
        if (c[k] == folderCode(f)) {
          uint8_t p[AppsMax];
          unsigned m = folderPages(f, p);
          for (unsigned j = 0; j < m; j++)
            if (p[j] == page) return f;
        }
    }
  return -1;
}
int here() {
  int f = pageFolder();
  return page == FolderPage ? folderCode(folderAt) : f >= 0 ? folderCode(f) : page;
}
// In a folder: its apps, the folder screen before the first and after the last. A hidden page opened by an event: back to Home.
int stepPage(int step) {
  int f = pageFolder();
  if (f >= 0) {
    uint8_t p[AppsMax];
    unsigned n = folderPages(f, p);
    for (unsigned i = 0; i < n; i++)
      if (p[i] == page) {
        int j = int(i) + step;
        return j >= 0 && j < int(n) ? p[j] : folderCode(f);
      }
  }
  int c[AppEntriesMax + 1];
  unsigned n = pageCycle(c);
  int h = here();
  for (unsigned i = 0; i < n; i++)
    if (c[i] == h) return c[(i + n + step) % n];
  return Home;
}
int nextPage() {
  return stepPage(1);
}
int previousPage() {
  return stepPage(-1);
}
int folderBack() {
  int f = pageFolder();
  return f >= 0 ? folderCode(f) : -1;
} // the folder screen an app of a folder returns to
void showPage(int next) {
  page = next & 0xff;
  appOpen = false;
  if (page == FolderPage) {
    folderAt = next >> 8;
    folderSel = 0;
  }
  if (page == Signals) {
    signalManual = false;
    radar.open();
  } else if (!webRadarActive() && !wardrive::holdsRadar())
    radar.close();
  if (page == QuickPage) {
    quickTargets = false;
    quickIndex = 0;
  }
  if (page == Messages) {
    messageOffset = 0;
    unreadCount = 0;
  }
}
// An app's name in a folder's list.
String appName(int p) {
  switch (p) {
  case Messages: return t("Messages");
  case QuickPage: return t("Quick send");
  case Nodes: return t("Nodes");
  case Gps: return "GPS";
  case Signals: return t("Radar");
  case WardrivePage: return t("Wardrive");
  case Wifi: return "Wi-Fi";
  case Ble: return "Bluetooth";
  case Settings: return t("Settings");
  case NotifyPage: return t("Notifications");
  case FolderPage: return appsFolderName(folderAt);
  }
  return t("Modules");
}
template <class T> String applyOne(const char* key, T value) {
  StaticJsonDocument<96> d;
  d[key] = value;
  return applySettings(d.as<JsonObjectConst>());
}
// Radio settings on the device: a draft saved at once, as on the M9. One button: click - next row,
// hold - change it (click - next value, hold - done); the frequency goes digit by digit (click - the
// digit +1, hold - the next digit).
// Region: the default one of our floods (Regions.h), chosen among the regions found; Find regions: asks the
// repeaters that hear this node (hold). Previous: the profile before the last change of frequency, BW, SF or CR (hold
// returns to it); during a test of a network profile from the page or app it is the test (hold keeps it, a restart
// returns the saved one). Network: the network search (NetFind.h): hold tries the regional presets (RadioPresets.h)
// in turn (or stops the search), and once a network is found tests it (radio try).
enum RadioRow {
  RowFreq,
  RowBw,
  RowSf,
  RowCr,
  RowPower,
  RowHops,
  RowRelay,
  RowHash,
  RowRegion,
  RowFind,
  RowPrev,
  RowNet,
  RowSave,
  RowCancel,
  RadioRows
};
bool radioEdit = false, radioEditing = false;
int radioRow = 0, radioDigit = 0;
// The draft lives in zeroed memory, made a copy of config when the editor opens.
alignas(Config) uint8_t radioDraftSpace[sizeof(Config)];
Config& radioDraft = *reinterpret_cast<Config*>(radioDraftSpace);
void openRadio() {
  new (radioDraftSpace) Config(config);
  radioEdit = true;
  radioEditing = false;
  radioRow = 0;
  radioDigit = 0;
  dirty = true;
}
String radioName(int i) {
  switch (i) {
  case RowFreq: return t("Frequency");
  case RowBw: return t("Bandwidth");
  case RowSf: return "SF";
  case RowCr: return "CR";
  case RowPower: return t("Power");
  case RowHops: return t("Relay limit");
  case RowRelay: return t("Relaying");
  case RowHash: return t("Path hash");
  case RowRegion: return t("Region");
  case RowFind: return t("Find regions");
  case RowNet: return t("Find network");
  case RowPrev: return config.trial.set() ? t("Test") : t("Previous");
  case RowSave: return t("Save");
  default: return t("Cancel");
  }
}
String radioValue(const Config& c, int i) {
  switch (i) {
  case RowFreq: return String(c.frequency, 3);
  case RowBw: return String(c.bandwidth, 1);
  case RowSf: return String(c.sf);
  case RowCr: return "4/" + String(c.cr);
  case RowPower: return String(c.power) + " dBm";
  case RowHops: return String(c.hops);
  case RowRelay: return c.relay ? t("on") : t("off");
  case RowHash: return plural(c.pathHash, "byte", "bytes");
  case RowRegion: return c.region[0] ? String(c.region) : String(t("none"));
  case RowPrev:
    return config.trial.set()      ? t("keep")
           : config.previous.set() ? String(config.previous.frequency, 3) + "/" + String(config.previous.sf)
                                   : String("-");
  case RowNet: {
    auto& s = netfind::state;
    if (netfind::running()) return String(s.step + 1) + "/" + String(s.count);
    return s.phase == netfind::State::Done
               ? (s.best != 255
                      ? "SF" + String(s.results[s.best].radio.sf) + " " + String(s.results[s.best].radio.frequency, 3)
                      : String("-"))
               : String("");
  }
  case RowFind: {
    auto& s = regions::search;
    return regions::searching()               ? String(s.repeaters) + t(" rpt...")
           : s.state == regions::Search::Done ? plural(s.count, "found", "found")
                                              : String("");
  }
  }
  return "";
}
bool radioChanged() {
  for (int i = 0; i < RowSave; i++)
    if (radioValue(radioDraft, i) != radioValue(config, i)) return true;
  return false;
}
// The frequency digit being set: 0 - megahertz (863..870), 1..3 - the kilohertz digits.
void radioDigitSpan(const String& v, unsigned& from, unsigned& to) {
  if (radioDigit == 0) {
    from = 0;
    to = v.indexOf('.');
  } else {
    from = v.indexOf('.') + radioDigit;
    to = from + 1;
  }
}
void radioStep(int dir) {
  auto& d = radioDraft;
  switch (radioRow) {
  case RowFreq: {
    long k = lroundf(d.frequency * 1000);
    if (radioDigit == 0) {
      k += dir * 1000L;
      if (k >= 871000) k -= 8000;
      if (k < 863000) k += 8000;
    } else {
      long unit = radioDigit == 1 ? 100 : radioDigit == 2 ? 10 : 1;
      int digit = k / unit % 10;
      k += ((digit + dir + 10) % 10 - digit) * unit;
    }
    d.frequency = constrain(k, 863000L, 870000L) / 1000.f;
    break;
  }
  case RowBw: {
    const float bw[] = {62.5f, 125, 250, 500};
    int i = 0;
    while (i < 3 && d.bandwidth != bw[i]) i++;
    d.bandwidth = bw[(i + dir + 4) % 4];
    break;
  }
  case RowSf: d.sf = 7 + (d.sf - 7 + dir + 6) % 6; break;
  case RowCr: d.cr = 5 + (d.cr - 5 + dir + 4) % 4; break;
  case RowPower: d.power = (d.power + dir + MM_MAX_POWER + 1) % (MM_MAX_POWER + 1); break;
  case RowHops: d.hops = (d.hops + dir + 8) % 8; break;
  case RowRelay: d.relay = !d.relay; break;
  case RowHash: d.pathHash = 1 + (d.pathHash - 1 + dir + 3) % 3; break;
  case RowRegion: strlcpy(d.region, regions::cycle(d.region, dir, false).c_str(), sizeof d.region); break;
  case RowFind:
    if (!regions::find()) notice(t("Radio offline"));
    break;
  case RowNet: {
    auto& s = netfind::state;
    if (netfind::running()) {
      netfind::stop();
      break;
    }
    if (s.phase == netfind::State::Done && s.best != 255 && !config.sameRadio(s.results[s.best].radio)) {
      String r = netfind::use(s.best);
      bool done = r.startsWith("OK");
      if (done) radioDraft.setRadio(config.radio());
      notice(done                             ? t("Radio test: restart returns")
             : r.startsWith("ERR radio busy") ? t("Radio busy, retry")
                                              : r.substring(4));
      break;
    }
    if (!netfind::start())
      notice(meshRadio.busy() || regions::searching() ? t("Radio busy, retry") : t("Radio offline"));
    break;
  }
  case RowPrev: {
    if (!config.trial.set() && !config.previous.set()) {
      notice(t("No previous settings"));
      break;
    }
    String r = executeCommand(config.trial.set() ? "radio keep" : "radio previous");
    bool done = r.startsWith("OK");
    if (done) radioDraft.setRadio(config.radio());
    notice(done                             ? t("Radio settings changed")
           : r.startsWith("ERR radio busy") ? t("Radio busy, retry")
                                            : r.substring(4));
    break;
  }
  }
  dirty = true;
}
void radioSave() {
  if (!radioChanged()) {
    radioEdit = false;
    notice(t("Nothing to save"));
    return;
  }
  StaticJsonDocument<256> j;
  auto& d = radioDraft;
  j["frequency"] = d.frequency;
  j["bandwidth"] = d.bandwidth;
  j["sf"] = int(d.sf);
  j["cr"] = int(d.cr);
  j["power"] = int(d.power);
  j["hops"] = int(d.hops);
  j["relay"] = d.relay;
  j["path_hash"] = int(d.pathHash);
  j["region"] = d.region;
  String r = applySettings(j.as<JsonObjectConst>());
  bool saved = r.startsWith("OK");
  if (saved) radioEdit = false;
  notice(saved ? t("Settings saved") : r.startsWith("ERR radio busy") ? t("Radio busy, retry") : r.substring(4));
}
void radioButton(bool click, bool hold) {
  if (radioEditing) {
    if (click)
      radioStep(1);
    else if (hold) {
      if (radioRow == RowFreq && radioDigit < 3)
        radioDigit++;
      else {
        radioEditing = false;
        radioDigit = 0;
      }
    }
    return;
  }
  if (click) {
    radioRow = (radioRow + 1) % RadioRows;
    return;
  }
  if (!hold) return;
  if (radioRow == RowSave)
    radioSave();
  else if (radioRow == RowCancel)
    radioEdit = false;
  else if (radioRow == RowRelay || radioRow == RowFind || radioRow == RowPrev || radioRow == RowNet)
    radioStep(1);
  else {
    radioEditing = true;
    radioDigit = 0;
  }
}
// The key hints of the radio screen: what a click and a hold do now.
String radioHint() {
  if (radioEditing) return radioRow == RowFreq ? t("click-digit+ hold-next") : t("click-change hold-done");
  return radioRow == RowSave     ? t("click-next hold-save")
         : radioRow == RowCancel ? t("click-next hold-leave")
                                 : t("click-next hold-change");
}

// Wardrive (Wardrive.h): its summary or the repeaters heard (wardriveList); hold opens the menu.
bool wardriveList = false;
String wdHash(const uint8_t* h, uint8_t size) {
  String s;
  char b[3];
  for (uint8_t i = 0; i < size && i < 3; i++) {
    snprintf(b, 3, "%02X", h[i]);
    s += b;
  }
  return s;
}
String wdSnr(int8_t q) {
  char b[12];
  snprintf(b, sizeof b, "%+.1f", q / 4.0f);
  return b;
}
String wdMode() {
  if (!wardrive::running()) return t("Off");
  String s;
  if (wardrive::settings.passive) s = t("Listening");
  if (wardrive::settings.ping) s += String(s.length() ? " + " : "") + t("pings");
  if (wardrive::settings.nets) s += String(s.length() ? " + " : "") + "Wi-Fi";
  return s;
}
String wdPosition() {
  int32_t lat, lon;
  bool phone;
  if (!wardrive::position(lat, lon, phone)) return t("No position: GPS/phone");
  return String(phone ? t("Phone ") : "GPS ") + String(lat / 1e6, 5) + " " + String(lon / 1e6, 5);
}
String wdCounts() {
  String s = plural(wardrive::points(), "point", "points");
  if (wardrive::settings.nets || wardrive::nets()) s += " · Wi-Fi " + String(wardrive::nets());
  return s;
}
String wdLast() {
  const auto& r = wardrive::last();
  if (wardrive::pinging()) return t("Ping: listening...");
  if (!r.done) return t("No pings yet");
  if (!r.count) return t("Ping: nobody repeated");
  return t("Ping: ") + String(r.count) + t(" rpt, ") + wdHash(r.hash, r.size) + " " + wdSnr(r.snr);
}
unsigned wdOrder(unsigned* order, const wardrive::Repeater*& list) {
  unsigned n = wardrive::repeaters(list);
  for (unsigned i = 0; i < n; i++) order[i] = i;
  std::sort(order, order + n, [&](unsigned a, unsigned b) {
    unsigned x = list[a].echoes * 4 + list[a].rx, y = list[b].echoes * 4 + list[b].rx;
    return x != y ? x > y : list[a].snr > list[b].snr;
  });
  return n;
}
// Actions: a screen with one action runs it on hold; several open a menu.
enum Act {
  ActFormat,
  ActJoin,
  ActJoinHeard,
  ActAdvert,
  ActReplyOk,
  ActReplyAck,
  ActOlder,
  ActNewer,
  ActNextNode,
  ActNodeOk,
  ActResetPath,
  ActGps,
  ActPosition,
  ActWifi,
  ActBle,
  ActLanguage,
  ActBattery,
  ActScreen,
  ActContrast,
  ActSelfTest,
  ActHoming,
  ActNextSignal,
  ActStopHoming,
  ActResetPeak,
  ActCsiBeacon,
  ActCsiSensor,
  ActCalibrate,
  ActQuick,
  ActNotify,
  ActNotifyLed,
  ActNotifyWake,
  ActNotifyPopup,
  ActNotifyFailed,
  ActPowerOff,
  ActRadio,
  ActRemoteLogin,
  ActRemoteStatus,
  ActTrace,
  ActWardriveRun,
  ActWardrivePing,
  ActWardriveView,
  ActWardriveNets,
  ActDoubleBack,
  ActClose
};
constexpr unsigned MenuMax = 14;
// Channels are added on the web page or in the app; here: an invitation in a message and a hashtag heard on air.
const HeardChannel* heardTag() {
  for (unsigned i = 0; i < meshRadio.heardCount; i++)
    if (meshRadio.heard[i].name[0]) return &meshRadio.heard[i];
  return nullptr;
}
String channelTag(uint64_t id) {
  const channels::Channel* c = meshRadio.channel(id);
  return channels::isPublic(c ? c->secret : channels::publicSecret)
             ? String(" #")
             : " " + String(c->name[0] == '#' ? "" : "#") + c->name;
}
String messageText(const ChatMessage& m) {
  String name;
  uint8_t key[16];
  return channels::parseLink(m.text, name, key) ? t("Invitation to channel ") + name : String(m.text);
}
void joinedNotice(MeshRadio::ChannelResult r, const String& name) {
  notice(r == MeshRadio::ChannelAdded    ? t("Joined ") + name
         : r == MeshRadio::ChannelExists ? t("Already in ") + name
         : r == MeshRadio::ChannelFull   ? t("8 channels at most")
                                         : t("Not joined"));
}
unsigned actions(Act* out) {
  unsigned n = 0;
  switch (page) {
  case Home:
    // Storage left by another firmware: the key, settings and contacts are in NVS, the history is not kept.
    if (!hardware.fsOk) out[n++] = ActFormat;
    out[n++] = ActAdvert;
    break;
  case Messages: {
    String name;
    uint8_t key[16];
    const ChatMessage* m = shownMessage();
    if (m && channels::parseLink(m->text, name, key) && !meshRadio.channel(channels::idOf(key))) out[n++] = ActJoin;
  }
    if (heardTag()) out[n++] = ActJoinHeard;
    if (meshRadio.historyCount) {
      out[n++] = ActReplyOk;
      out[n++] = ActReplyAck;
      out[n++] = ActOlder;
      out[n++] = ActNewer;
      out[n++] = ActQuick;
    }
    break;
  case Nodes:
    if (Peer* p = shownNode()) {
      out[n++] = ActNextNode;
      if (p->type == 1) out[n++] = ActNodeOk;
      if (p->type == 2 || p->type == 3) {
        out[n++] = ActRemoteLogin;
        out[n++] = ActRemoteStatus;
      } // with the saved password, else as a guest
      if (remote::traceable(p->id)) out[n++] = ActTrace;
      if (p->pathLength != 255) out[n++] = ActResetPath;
    }
    out[n++] = ActAdvert;
    break;
  case Signals:
    if (radar.csi == Radar::CsiSensor) {
      out[n++] = ActCalibrate;
      out[n++] = ActCsiSensor;
    } else if (radar.tracking) {
      out[n++] = ActStopHoming;
      out[n++] = ActResetPeak;
    } else {
      out[n++] = ActCsiBeacon;
      if (radar.csi == Radar::CsiOff) {
        out[n++] = ActCsiSensor;
        if (radar.count) {
          out[n++] = ActHoming;
          out[n++] = ActNextSignal;
        }
      }
    }
    break; // CSI first: the beacon is the usual Heltec role
  case Gps:
    out[n++] = ActGps;
    out[n++] = ActPosition;
    break;
  case Wifi: out[n++] = ActWifi; break;
  case Ble: out[n++] = ActBle; break;
  case Settings:
    out[n++] = ActRadio;
    out[n++] = ActLanguage;
    out[n++] = ActBattery;
    out[n++] = ActScreen;
    out[n++] = ActContrast;
    out[n++] = ActDoubleBack;
    out[n++] = ActPowerOff;
    out[n++] = ActNotify;
    break;
  case WardrivePage:
    out[n++] = ActWardriveRun;
    if (wardrive::running()) out[n++] = ActWardrivePing;
    out[n++] = ActWardriveView;
    out[n++] = ActWardriveNets;
    break;
  case NotifyPage:
    if (pins::led >= 0) out[n++] = ActNotifyLed;
    out[n++] = ActNotifyWake;
    out[n++] = ActNotifyPopup;
    out[n++] = ActNotifyFailed;
    break;
  case Modules:
    if (!hardware.fsOk) out[n++] = ActFormat;
    out[n++] = ActSelfTest;
    break; // FS ERR is shown here
  }
  if (n > 1) out[n++] = ActClose;
  return n;
}
bool keepsMenu(Act a) {
  return a == ActFormat || a == ActPowerOff || a == ActNextSignal || a == ActOlder || a == ActNewer ||
         a == ActNextNode || a == ActLanguage || a == ActBattery || a == ActDoubleBack || a == ActScreen ||
         a == ActContrast;
}
String flag(bool value) {
  return value ? t("On") : t("Off");
}
String actName(Act a) {
  const ChatMessage* m = shownMessage();
  bool publicChat = m && channels::isChannel(m->destination);
  switch (a) {
  case ActQuick: return t("Quick send");
  case ActWardriveRun: return wardrive::running() ? t("Stop wardrive") : t("Start: listen + ping");
  case ActWardrivePing: return t("Ping now");
  case ActWardriveView: return wardriveList ? t("Show summary") : t("Repeaters heard");
  case ActWardriveNets: return "Wi-Fi/BLE: " + flag(wardrive::settings.nets);
  case ActNotify: return t("Notifications");
  case ActNotifyLed:
  case ActNotifyWake:
  case ActNotifyPopup: {
    uint8_t mode = a == ActNotifyLed ? config.notifyLed : a == ActNotifyWake ? config.notifyWake : config.notifyPopup;
    return (a == ActNotifyLed    ? String("LED: ")
            : a == ActNotifyWake ? t("Wake: ")
                                 : t("Popup: ")) +
           (mode == 0   ? t("off")
            : mode == 1 ? t("all")
                        : String("@"));
  }
  case ActNotifyFailed: return t("TX alert: ") + (config.notifyFailed ? t("on") : t("off"));
  case ActFormat: return t("Create storage...");
  case ActJoin: {
    String name;
    uint8_t key[16];
    channels::parseLink(m ? m->text : "", name, key);
    return t("Join ") + name;
  }
  case ActJoinHeard: {
    const HeardChannel* h = heardTag();
    return t("Join heard ") + (h ? h->name : "");
  }
  case ActAdvert: return t("Announce node");
  case ActReplyOk: return t("Reply: OK") + String(publicChat ? " #" : "");
  case ActReplyAck: return t("Reply: Got it");
  case ActOlder: return t("Older message");
  case ActNewer: return t("Newer message");
  case ActNextNode: return t("Next node");
  case ActNodeOk: return t("Send: OK");
  case ActResetPath: return t("Reset path");
  case ActRemoteLogin: {
    Peer* p = shownNode();
    return p && remote::saved(p->id) ? t("Log in") : t("Log in as guest");
  }
  case ActRemoteStatus: return t("Server status");
  case ActTrace: return t("Trace route");
  case ActGps: return config.gps ? t("Turn GPS off") : t("Turn GPS on");
  case ActPosition: return t("Share position");
  case ActWifi: return portalActive() ? t("Turn Wi-Fi off") : t("Turn Wi-Fi on");
  case ActBle: return bleActive() ? t("Turn BLE off") : t("Turn BLE on");
  case ActLanguage: return t("Language: ") + langNames[config.lang < LangCount ? config.lang : 0];
  case ActBattery: return config.batteryVolts ? t("Battery: volts") : t("Battery: percent");
  case ActDoubleBack: return config.doubleBack ? t("Double click: back") : t("Double click: off");
  case ActScreen: return t("Screen off: ") + (config.dimAfter ? String(config.dimAfter) + t(" s") : t("never"));
  case ActContrast: return t("Contrast: ") + String(config.brightness);
  case ActHoming: {
    int i = shownSignal();
    return t("Home in: ") + (i >= 0 ? signalName(radar.targets[i]) : String("-"));
  }
  case ActNextSignal: return t("Next signal");
  case ActCsiBeacon: return radar.csi == Radar::CsiBeacon ? t("Stop beacon") : t("CSI beacon: on");
  case ActCsiSensor: return radar.csi == Radar::CsiSensor ? t("CSI motion: off") : t("CSI sensor");
  case ActCalibrate: return t("Calibrate (10 s still)");
  case ActStopHoming: return t("Stop homing");
  case ActResetPeak: return t("Reset peak");
  case ActPowerOff: return t("Turn off...");
  case ActRadio: return t("Radio...");
  case ActSelfTest: return t("Encryption test");
  case ActClose: return t("< Close menu");
  default: break;
  }
  return "";
}
void reply(const String& text) {
  const ChatMessage* m = shownMessage();
  if (!m) return;
  uint64_t to = channels::isChannel(m->destination) ? m->destination : m->outgoing ? m->destination : m->source;
  bool sent = to != meshRadio.nodeId && meshRadio.sendMessage(text, to);
  notice(sent ? t("Reply ") + text + t(" queued") : t("Reply not queued"));
}
void run(Act a) {
  switch (a) {
  case ActQuick: {
    const ChatMessage* m = shownMessage();
    if (m) {
      quickReplyTo = channels::isChannel(m->destination) ? m->destination : m->outgoing ? m->destination : m->source;
      quickReply = true;
      showPage(QuickPage);
      appOpen = true;
      quickIndex = 1;
    }
    break;
  }
  case ActNotify: showPage(NotifyPage); break;
  case ActWardriveRun:
  case ActWardriveNets: {
    StaticJsonDocument<128> d;
    d["action"] = a == ActWardriveRun && wardrive::running() ? "stop" : "settings";
    if (a == ActWardriveNets)
      d["nets"] = !wardrive::settings.nets;
    else if (!wardrive::running()) {
      d["passive"] = true;
      d["ping"] = true;
    }
    String r = wardrive::command(d.as<JsonObjectConst>());
    notice(r.startsWith("ERR")    ? r
           : a == ActWardriveNets ? "Wi-Fi/BLE: " + flag(wardrive::settings.nets)
           : wardrive::running()  ? t("Wardrive on")
                                  : t("Wardrive off"));
    break;
  }
  case ActWardrivePing: {
    int32_t lat, lon;
    bool phone;
    notice(wardrive::ping()                       ? t("Ping queued")
           : !wardrive::position(lat, lon, phone) ? t("No position yet")
                                                  : t("A ping is under way"));
    break;
  }
  case ActWardriveView: wardriveList = !wardriveList; break;
  case ActNotifyLed: notice(applyOne("notify_led", (config.notifyLed + 1) % 3)); break;
  case ActNotifyWake: notice(applyOne("notify_wake", (config.notifyWake + 1) % 3)); break;
  case ActNotifyPopup: notice(applyOne("notify_popup", (config.notifyPopup + 1) % 3)); break;
  case ActNotifyFailed: notice(applyOne("notify_failed", !config.notifyFailed)); break;
  case ActFormat: {
    static uint32_t armed = 0;
    if (!armed || millis() - armed > 5000) {
      armed = millis();
      notice(t("Hold again: erase"));
      break;
    }
    armed = 0;
    String r = executeCommand("fsformat");
    if (r.startsWith("OK")) executeCommand("restart");
    notice(r.startsWith("OK") ? t("Storage created, restart") : r);
    break;
  }
  case ActJoin: {
    const ChatMessage* m = shownMessage();
    String name;
    uint8_t key[16];
    if (m && channels::parseLink(m->text, name, key)) joinedNotice(meshRadio.joinLink(m->text), name);
    break;
  }
  case ActJoinHeard: {
    const HeardChannel* h = heardTag();
    if (h) {
      String name = h->name;
      joinedNotice(meshRadio.joinHashtag(name), name);
    }
    break;
  }
  case ActRadio: openRadio(); break;
  case ActAdvert: notice(meshRadio.sendHello() ? t("Node announced") : t("Announcement failed")); break;
  case ActReplyOk: reply("OK"); break;
  case ActReplyAck: reply(t("Got it")); break;
  case ActOlder:
    messageOffset++;
    shownMessage();
    break;
  case ActNewer: messageOffset = max(0, messageOffset - 1); break;
  case ActNextNode:
    nodeIndex++;
    menuIndex = 0;
    break;
  case ActNodeOk: {
    Peer* p = shownNode();
    bool sent = p && meshRadio.sendMessage("OK", p->id);
    notice(sent ? t("OK queued") : t("Not queued"));
    break;
  }
  case ActResetPath: {
    Peer* p = shownNode();
    notice(p && !meshRadio.busy() && meshRadio.resetPath(p->id) ? t("Path reset") : t("Path reset failed"));
    break;
  }
  case ActRemoteLogin:
  case ActRemoteStatus:
  case ActTrace: {
    Peer* p = shownNode();
    bool sent = p && (a == ActRemoteLogin    ? remote::login(p->id, "")
                      : a == ActRemoteStatus ? remote::status(p->id)
                                             : remote::traceTo(p->id));
    notice(sent ? t("Sent, waiting") : t("Not sent"));
    break;
  }
  case ActGps:
    notice(applyOne("gps", !config.gps).startsWith("OK") ? "GPS: " + String(config.gps ? t("on") : t("off"))
                                                         : t("Radio busy, retry"));
    break;
  case ActPosition: notice(meshRadio.sendPosition() ? t("Position shared") : t("Needs a GPS fix")); break;
  case ActWifi: portalToggle(); break;
  case ActBle: bleToggle(); break;
  case ActLanguage: applyOne("lang", langCodes[langStep(config.lang, 1)]); break;
  case ActBattery: applyOne("battery_volts", !config.batteryVolts); break;
  case ActDoubleBack: applyOne("double_back", !config.doubleBack); break;
  case ActScreen: {
    const uint16_t steps[] = {0, 15, 30, 60, 120, 300};
    int i = 0;
    while (i < 5 && steps[i] != config.dimAfter) i++;
    applyOne("dim_after", steps[(i + 1) % 6]);
    break;
  }
  case ActContrast: {
    const uint8_t steps[] = {10, 60, 150, 255};
    int i = 0;
    while (i < 3 && steps[i] < config.brightness) i++;
    applyOne("brightness", steps[(i + 1) % 4]);
    break;
  }
  case ActHoming: {
    int i = shownSignal();
    signalManual = true;
    if (i >= 0 && radar.track(i)) {
      pingedSamples = radar.samples;
      notice(t("Homing started"));
    }
    break;
  }
  case ActNextSignal: {
    int i = shownSignal();
    signalManual = true;
    if (i >= 0) {
      i = (i + 1) % radar.count;
      signalId = radar.targets[i].id;
      signalKind = radar.targets[i].kind;
    }
    menuIndex = 0;
    break;
  }
  case ActStopHoming: radar.untrack(); break;
  case ActResetPeak:
    radar.resetPeak();
    notice(t("Peak reset"));
    break;
  case ActCsiBeacon: radar.setCsi(radar.csi == Radar::CsiBeacon ? Radar::CsiOff : Radar::CsiBeacon); break;
  case ActCsiSensor: radar.setCsi(radar.csi == Radar::CsiSensor ? Radar::CsiOff : Radar::CsiSensor); break;
  case ActCalibrate:
    if (radar.beaconHeard()) {
      radar.calibrate();
      notice(t("Calibrating 10 s"));
    } else
      notice(t("No beacon heard"));
    break;
  case ActPowerOff: {
    static uint32_t armed = 0;
    if (!armed || millis() - armed > 5000) {
      armed = millis();
      notice(t("Hold again: turn off"));
      break;
    }
    armed = 0;
    powerOff();
    menuOpen = false;
    break;
  }
  case ActClose:
  default: break;
  case ActSelfTest: {
    bool valid = meshRadio.selfTest();
    meshRadio.event = valid ? "Encryption test OK" : "Encryption test FAILED";
    notice(valid ? t("Encryption: OK") : t("Encryption: ERROR"));
    break;
  }
  }
}

// HUD: title on the left; unread, GPS, links, signal and battery on the right.
void header(const String& title) {
  auto& c = *hardware.canvas;
  int x = 127;
  unsigned mv = hardware.batteryMv;
  // Measured battery: volts or the charge estimate as text; the icon stays for no battery and for USB power.
  if (mv && (config.batteryVolts || mv <= 4250)) {
    String v = config.batteryVolts ? String(mv / 1000.f, 2) + "V" : String(batteryPercent(mv)) + "%";
    sayRight(x + 1, 7, v);
    x -= width(v, small) + 3;
  } else {
    unsigned pct = batteryPercent(mv);
    c.drawRect(x - 12, 1, 12, 7, 1);
    c.drawFastVLine(x, 3, 3, 1);
    if (mv > 4250) {
      c.drawLine(x - 8, 2, x - 6, 4, 1);
      c.drawLine(x - 6, 4, x - 4, 4, 1);
      c.drawLine(x - 4, 4, x - 2, 6, 1);
    } else if (mv)
      c.fillRect(x - 10, 3, max(1u, 8 * pct / 100), 3, 1);
    x -= 16;
  }
  bool fresh = meshRadio.lastRxAt && millis() - meshRadio.lastRxAt < 600000;
  unsigned level = !meshRadio.ready || !fresh ? 0
                   : meshRadio.lastSnr >= 5   ? 4
                   : meshRadio.lastSnr >= 0   ? 3
                   : meshRadio.lastSnr >= -5  ? 2
                                              : 1;
  for (unsigned k = 0; k < 4; k++) {
    int h = 2 + k * 2;
    if (k < level)
      c.fillRect(x - 14 + k * 4, 8 - h, 3, h, 1);
    else
      c.drawPixel(x - 13 + k * 4, 7, 1);
  }
  if (!meshRadio.ready) c.drawLine(x - 15, 0, x - 1, 8, 1);
  x -= 18;
  if (config.gps) {
    c.drawCircle(x - 3, 3, 2, 1);
    if (hardware.gpsFix()) c.fillCircle(x - 3, 3, 2, 1);
    c.drawLine(x - 5, 4, x - 3, 8, 1);
    c.drawLine(x - 1, 4, x - 3, 8, 1);
    x -= 8;
  }
  if (portalActive()) {
    for (int r : {2, 4, 6})
      for (int a = -45; a <= 45; a += 15)
        c.drawPixel(x - 4 + roundf(r * sinf(a * M_PI / 180)), 8 - roundf(r * cosf(a * M_PI / 180)), 1);
    x -= 10;
  }
  if (bleActive()) {
    c.drawLine(x - 3, 0, x - 3, 8, 1);
    c.drawLine(x - 3, 0, x - 1, 2, 1);
    c.drawLine(x - 1, 2, x - 5, 6, 1);
    c.drawLine(x - 3, 8, x - 1, 6, 1);
    c.drawLine(x - 1, 6, x - 5, 2, 1);
    x -= 8;
  }
  if (unreadCount) {
    String n = String(unreadCount);
    sayRight(x, 7, n);
    x -= width(n, small) + 1;
    c.drawRect(x - 9, 1, 9, 7, 1);
    c.drawLine(x - 9, 1, x - 5, 5, 1);
    c.drawLine(x - 1, 1, x - 5, 5, 1);
    x -= 12;
  }
  say(0, 7, clipped(title, max(3, (x - 2) / 5)), small);
  c.drawFastHLine(0, 10, 128, 1);
}
// The longest start of value that fits px with "..", measured in the font (fallback glyphs are wider than a column).
String fitted(const String& value, int px, const uint8_t* f) {
  if (width(value, f) <= px) return value;
  int room = px - width("..", f);
  unsigned i = 0, cut = 0;
  while (i < value.length()) {
    unsigned next = i;
    utf8Next(value, next);
    if (width(value.substring(0, next), f) > room) break;
    cut = i = next;
  }
  return value.substring(0, cut) + "..";
}
void footer(const String& hint) {
  auto& c = *hardware.canvas;
  int x = 1, cycle[AppEntriesMax + 1];
  unsigned n = pageCycle(cycle);
  int h = here();
  for (unsigned i = 0; i < n; i++) {
    if (cycle[i] == h)
      c.fillRect(x, 58, 2, 4, 1);
    else
      c.drawPixel(x, 61, 1);
    x += 3;
  }
  if (hint.length()) sayRight(128, 63, fitted(clipped(hint, 20), 128 - x, small));
}
String hint() {
  Act acts[MenuMax];
  unsigned n = actions(acts);
  if (!n) return "";
  return n == 1 ? t("hold: ") + actName(acts[0]) : t("hold: menu");
}
unsigned quickTargetCount() {
  return quickSend::targetCount();
}
uint64_t quickTarget(unsigned i) {
  return quickSend::target(i);
}
String quickName(uint64_t id) {
  const auto* ch = meshRadio.channel(id);
  const Peer* p = meshRadio.findContact(id);
  return ch ? String(ch->name) : p ? String(p->name) : t("Choose recipient");
}
String quickLabel(unsigned i) {
  if (!i) return t("Choose recipient");
  if (i <= quickSend::Count)
    return quickSend::state.text[i - 1][0] ? String(quickSend::state.text[i - 1]) : String("--");
  return "< " + t("Exit");
}
bool quickKey(int key) {
  if (page != QuickPage) return false;
  if (key == 0x86) {
    if (quickTargets) {
      quickTargets = false;
      quickIndex = 0;
    } else {
      quickReply = false;
      showPage(Home);
    }
    return true;
  }
  bool click = key == 13 || key == 0x82 || key == 0xb6, hold = key == 0xa3;
  if (click) {
    if (quickTargets) {
      unsigned n = quickTargetCount();
      if (n) quickTargetIndex = (quickTargetIndex + 1) % n;
    } else
      quickIndex = (quickIndex + 1) % (quickSend::Count + 2);
  }
  if (hold) {
    if (quickTargets) {
      if (!quickTargetCount()) {
        notice(t("No recipients"));
        quickTargets = false;
        quickIndex = 0;
        return true;
      }
      quickTargetIndex %= quickTargetCount();
      StaticJsonDocument<128> d;
      d["action"] = "target";
      d["to"] = meshRadio.idText(quickTarget(quickTargetIndex));
      String r = quickSend::command(d.as<JsonObjectConst>());
      notice(r);
      if (r.startsWith("OK")) {
        quickReply = quickTargets = false;
        quickIndex = 1;
      }
    } else if (!quickIndex) {
      quickTargets = true;
      quickTargetIndex = 0;
    } else if (quickIndex > quickSend::Count) {
      quickReply = false;
      showPage(nextPage());
    } else
      notice(quickSend::send(quickIndex - 1, quickReply ? quickReplyTo : 0));
  }
  return true;
}
void drawQuickMono() {
  auto& c = *hardware.canvas;
  if (quickTargets) {
    say(0, 20, fitted(t("Choose recipient"), 128, small), small);
    unsigned n = quickTargetCount();
    int first = max(0, min(int(quickTargetIndex) - 1, int(n) - 3));
    for (unsigned i = first; i < n && i < unsigned(first + 3); i++) {
      int y = 23 + (i - first) * 11;
      bool focus = i == quickTargetIndex;
      if (focus) c.fillRect(0, y, 128, 11, 1);
      say(2, y + 9, fitted(quickName(quickTarget(i)), 124, small), small, focus ? 0 : 1);
    }
    return;
  }
  bool focus = !quickIndex && appOpen;
  c.drawRoundRect(0, 12, 128, 17, 3, 1);
  if (focus) c.fillRoundRect(0, 12, 128, 17, 3, 1);
  say(3, 25, fitted(quickName(quickReply ? quickReplyTo : quickSend::state.to), 112, body), body, focus ? 0 : 1);
  c.fillTriangle(119, 17, 119, 23, 123, 20, focus ? 0 : 1);
  int first = max(1, min(int(quickIndex) - 1, int(quickSend::Count)));
  for (unsigned i = first; i <= quickSend::Count + 1 && i < unsigned(first + 2); i++) {
    int y = 31 + (i - first) * 13;
    bool selected = i == quickIndex && appOpen;
    if (selected) c.fillRect(0, y, 128, 13, 1);
    say(2, y + 10, fitted(quickLabel(i), 124, small), small, selected ? 0 : 1);
  }
}

void drawMenu() {
  auto& c = *hardware.canvas;
  Act acts[MenuMax];
  unsigned n = actions(acts);
  if (!n) {
    menuOpen = false;
    return;
  }
  menuIndex %= n;
  int first = max(0, min(menuIndex - 1, int(n) - 3));
  c.fillRect(0, 12, 128, 45, 0);
  c.drawRect(0, 12, 128, 45, 1);
  for (unsigned i = first; i < n && i < unsigned(first + 3); i++) {
    int y = 14 + (i - first) * 14;
    bool focus = int(i) == menuIndex;
    if (focus) c.fillRect(2, y, 124, 13, 1);
    say(5, y + 10, clipped(actName(acts[i]), 20), body, focus ? 0 : 1);
  }
  c.fillRect(0, 57, 128, 7, 0);
  footer(t("click-next hold-run"));
}
void drawPopup(const ChatMessage& m) {
  auto& c = *hardware.canvas;
  c.fillScreen(0);
  c.drawRect(3, 3, 11, 8, 1);
  c.drawLine(3, 3, 8, 7, 1);
  c.drawLine(13, 3, 8, 7, 1);
  say(18, 11, clipped(String(m.name) + (channels::isChannel(m.destination) ? channelTag(m.destination) : String()), 18),
      bold);
  textLines(messageText(m), 25, 3);
  sayRight(126, 62, t("click: close"));
}
// Radio settings: three rows; the value being changed (or the frequency digit) drawn inverted.
void drawRadioEdit() {
  auto& c = *hardware.canvas;
  c.fillScreen(0);
  int first = max(0, min(radioRow - 1, RadioRows - 3));
  for (int r = first; r < first + 3; r++) {
    int y = 21 + (r - first) * 12;
    bool focus = r == radioRow;
    if (focus) c.fillRect(0, y - 10, 128, 12, 1);
    say(3, y, radioName(r), small, !focus);
    if (r >= RowSave) continue;
    String v = radioValue(radioDraft, r);
    int x = 125 - width(v, small);
    sayRight(125, y, v, small, !focus);
    if (radioValue(config, r) != v) c.fillRect(x - 4, y - 5, 2, 2, !focus); // changed, not saved yet
    if (focus && radioEditing) {
      unsigned a = 0, b = v.length();
      if (r == RowFreq) radioDigitSpan(v, a, b);
      int u = x + width(v.substring(0, a), small);
      String part = v.substring(a, b);
      c.fillRect(u - 1, y - 9, width(part, small) + 2, 11, 0);
      say(u, y, part, small, 1);
    }
  }
  header(t("Radio") + (radioChanged() ? " *" : ""));
  sayRight(128, 63, radioHint(), small);
  if (action.length() && millis() - actionAt < 3500) {
    c.fillRect(0, 36, 128, 20, 0);
    c.drawRect(0, 36, 128, 20, 1);
    say(3, 50, clipped(action, 20));
  }
  hardware.flush();
}
// Power off pending (Power.cpp): what turns the board on again.
String powerOnHint() {
  return t("hold ") + MM_BUTTON;
}
bool farewell = false; // the board is about to turn off: the last frame
void drawOff() {
  auto& c = *hardware.canvas;
  c.fillScreen(0);
  say(0, 24, farewell ? t("Device is off") : t("Turning off..."), bold);
  say(0, 44, t("To turn on:"), small);
  say(0, 54, powerOnHint(), small);
  hardware.flush();
}
// What an app has for the user: unread messages.
unsigned appNews(int p) {
  return p == Messages ? unreadCount : 0;
}
// A folder: its apps in one column, two when more than four; the one a hold (OK) opens lit.
void drawFolderMono() {
  auto& c = *hardware.canvas;
  uint8_t p[AppsMax];
  unsigned n = folderPages(folderAt, p), rows = n > 4 ? (n + 1) / 2 : n;
  if (rows > 4) rows = 4;
  int w = n > 4 ? 64 : 128;
  for (unsigned i = 0; i < n && i < rows * 2; i++) {
    int x = i / rows * 64, y = 13 + i % rows * 11;
    bool focus = int(i) == folderSel;
    unsigned k = appNews(p[i]);
    String badge = k ? String(k) : String();
    if (focus) c.fillRect(x, y, w - 1, 11, 1);
    say(x + 2, y + 9, fitted(appName(p[i]), w - 6 - (k ? width(badge, small) + 3 : 0), small), small, !focus);
    if (k) sayRight(x + w - 3, y + 9, badge, small, !focus);
  }
}
void draw() {
  auto& c = *hardware.canvas;
  if (powerOffPending()) {
    drawOff();
    return;
  }
  if (radioEdit) {
    drawRadioEdit();
    return;
  }
  c.fillScreen(0);
  const ChatMessage* last = &incomingPopup;
  if (popupAt && millis() - popupAt < 8000 && last && !last->outgoing) {
    drawPopup(*last);
    hardware.flush();
    return;
  }
  popupAt = 0;
  String title;
  switch (page) {
  case QuickPage:
    title = t("Quick send");
    drawQuickMono();
    break;
  case WardrivePage:
    title = t("Wardrive");
    if (wardriveList) {
      unsigned order[24];
      const wardrive::Repeater* l;
      unsigned n = wdOrder(order, l);
      if (!n) say(0, 30, t("No repeaters yet"), small);
      for (unsigned i = 0; i < n && i < 4; i++) {
        auto& e = l[order[i]];
        say(0, 20 + i * 9,
            clipped(wdHash(e.hash, e.size) + " " + wdSnr(e.snr) + t("dB echo ") + String(e.echoes) + " rx " +
                        String(e.rx),
                    25),
            small);
      }
    } else {
      say(0, 21, clipped(wdMode(), 21), bold);
      say(0, 32, clipped(wdPosition(), 25), small);
      say(0, 41, clipped(wdCounts(), 25), small);
      say(0, 50, clipped(wdLast(), 25), small);
    }
    break;
  case NotifyPage:
    title = t("Notifications");
    say(0, 28, t("LED / screen / popup"), small);
    say(0, 42, t("hold: menu"), small);
    break;
  case Home: {
    title = config.name;
    say(0, 31, clockText(time(nullptr)), u8g2_font_10x20_t_cyrillic);
    if (meshRadio.ready) {
      sayRight(128, 20, String(config.frequency, 3) + t(" MHz"));
      sayRight(128, 30, "SF" + String(config.sf) + " BW" + String(config.bandwidth, 1));
    } else
      sayRight(128, 24, t("Radio error ") + String(meshRadio.radioError));
    unsigned near = 0;
    for (unsigned i = 0; i < meshRadio.peerCount; i++)
      if (meshRadio.peers[i].heard && millis() - meshRadio.peers[i].seen < 1800000) near++;
    say(0, 43, "RX " + String(meshRadio.rxCount) + "  TX " + String(meshRadio.txCount) + t("  near ") + String(near),
        small);
    unsigned mv = hardware.batteryMv;
    say(0, 52, (mv > 4250 ? t("USB power") : String(mv / 1000.f, 2) + "V") + (config.relay ? t("  relay on") : ""),
        small);
    break;
  }
  case Messages: {
    const ChatMessage* m = shownMessage();
    title = t("Messages") +
            (m ? " " + String(meshRadio.historyCount - messageOffset) + "/" + String(meshRadio.historyCount) : "");
    if (m) {
      String who = m->outgoing ? t("You") : String(m->name);
      if (channels::isChannel(m->destination)) who += channelTag(m->destination);
      const char* states[] = {"", tr("queued"), tr("sent"), tr("delivered"), tr("no ACK")};
      String route = meshRadio.routeText(*m, true),
             st = m->outgoing ? String(states[m->status]) + (route.length() ? " " + route : String())
                              : (route.length() ? route + " " : String()) + clockText(m->timestamp);
      say(0, 23, clipped(who, 20 - chars(st)), bold);
      sayRight(128, 22, st);
      textLines(messageText(*m), 36, 2);
    } else {
      say(0, 30, t("No messages yet"));
      say(0, 44, t("They appear here"), small);
    }
    break;
  }
  case Nodes: {
    unsigned order[24];
    unsigned n = sortedNodes(order);
    Peer* p = shownNode();
    title = t("Nodes") + (n ? " " + String(nodeIndex % n + 1) + "/" + String(n) : "");
    if (p) {
      say(0, 23, clipped(p->name, 21), bold);
      say(0, 33, typeText(p->type) + ", " + pathText(*p), small);
      String login = remoteLogin(*p), result = remoteResult(*p);
      if (login.length() || result.length()) {
        say(0, 42, login.length() ? login : String(int(p->rssi)) + " dBm SNR " + String(p->snr, 1), small);
        say(0, 51, result, small);
        break;
      }
      say(0, 42,
          p->heard ? String(int(p->rssi)) + " dBm SNR " + String(p->snr, 1) + ", " + ago(millis() - p->seen)
                   : t("saved, not heard"),
          small);
      float metres, bearing;
      if (distanceTo(*p, metres, bearing)) {
        const char* dirs[] = {tr("N"), tr("NE"), tr("E"), tr("SE"), tr("S"), tr("SW"), tr("W"), tr("NW")};
        int k = int((bearing + 22.5f) / 45) % 8;
        say(0, 51, (metres < 1000 ? String(int(metres)) + t(" m ") : String(metres / 1000, 1) + t(" km ")) + dirs[k],
            small);
      } else if (p->position)
        say(0, 51, t("has GPS position"), small);
    } else {
      say(0, 30, t("No nodes heard"));
      say(0, 44, t("hold: announce"), small);
    }
    break;
  }
  case Signals: {
    if (radar.csi == Radar::CsiSensor) {
      title = t("Motion CSI");
      bool heard = radar.beaconHeard();
      String state = radar.wifi != Radar::WifiReady ? t("no Wi-Fi: AP on?")
                     : !heard                       ? t("no beacon")
                     : radar.csiStale               ? t("CSI frozen")
                     : radar.calibrateUntil         ? t("calibrating")
                     : radar.moving                 ? t("MOTION")
                                                    : t("still");
      say(0, 31, state, u8g2_font_10x20_t_cyrillic);
      float threshold = radar.motionThreshold(), scale = max(threshold * 2.5f, radar.activity * 1.1f);
      c.drawRect(0, 36, 128, 6, 1);
      if (heard) c.fillRect(1, 37, max(1, int(126 * min(1.f, radar.activity / scale))), 4, 1);
      c.drawFastVLine(1 + int(125 * threshold / scale), 34, 10, 1);
      say(0, 52,
          String(radar.activity * 1000, 1) + t(" / thr ") + String(threshold * 1000, 1) +
              (radar.baseline > 0 ? "" : "*"),
          small);
      say(30, 62, String(radar.csiRate) + t("/s"), small);
    } else if (radar.tracking) {
      const RadarTarget& f = radar.focus;
      bool fresh = homingFresh();
      int trend = radar.trend();
      title = t("Homing");
      say(0, 21, clipped(String(kindLetter(f)) + signalName(f), 21), bold);
      int shown = lroundf(radar.fast);
      say(0, 41, radar.samples ? String(shown) : String("--"), u8g2_font_10x20_t_cyrillic);
      say(radar.samples && shown <= -100 ? 42 : 34, 41, "dBm", small);
      bool wifiLost = (f.kind == RadarTarget::Wifi && radar.wifi != Radar::WifiReady) ||
                      (f.kind == RadarTarget::Ble && radar.ble != Radar::BleReady);
      sayRight(128, 31,
               wifiLost    ? (f.kind == RadarTarget::Ble ? bleState() : wifiState())
               : !fresh    ? (radar.samples > 1 ? t("lost") : t("no signal"))
               : trend > 0 ? t("stronger")
               : trend < 0 ? t("weaker")
                           : t("steady"),
               bold);
      if (fresh && !wifiLost) {
        int x = 70, y = 25;
        if (trend > 0)
          c.fillTriangle(x, y + 4, x + 8, y + 4, x + 4, y - 2, 1);
        else if (trend < 0)
          c.fillTriangle(x, y - 2, x + 8, y - 2, x + 4, y + 4, 1);
      }
      sayRight(128, 41, t("peak ") + (radar.samples ? String(int(lroundf(radar.peak))) : String("-")), small);
      c.drawRect(0, 45, 128, 6, 1);
      if (radar.samples) c.fillRect(1, 46, max(1, int(126 * signalLevel(shown))), 4, 1);
      if (radar.samples) {
        int px = 1 + int(125 * signalLevel(lroundf(radar.peak)));
        c.drawFastVLine(px, 43, 10, 1);
      }
      String st = f.kind == RadarTarget::Lora ? String(radar.samples) + t(" pkts") : String(radar.rate) + t("/s");
      say(30, 62, st, small);
    } else {
      bool beacon = radar.csi == Radar::CsiBeacon;
      int sel = shownSignal();
      // Beacon: sweeps and BLE pause, so the title shows the beacon instead of the counts.
      title = beacon ? t("CSI beacon ") + String(radar.csiRate) + t("/s")
                     : t("Radar") + " W" + String(radar.counted(RadarTarget::Wifi)) + " B" +
                           String(radar.counted(RadarTarget::Ble)) + " L" + String(radar.counted(RadarTarget::Lora));
      String st = beacon ? String() : wifiState();
      if (!st.length() && !beacon) st = bleState();
      if (sel < 0) {
        say(0, 30, beacon ? t("Beacon on") : t("No signals yet"));
        say(0, 44, st.length() ? st : beacon ? t("hold: switch off") : t("Wi-Fi and LoRa"), small);
      } else {
        unsigned rows = st.length() ? 3 : 4; // a status line takes the 4th row, clear of the bottom hint
        int first = max(0, min(sel - 1, int(radar.count) - int(rows)));
        for (int i = first; i < int(radar.count) && i < first + int(rows); i++) {
          const RadarTarget& r = radar.targets[i];
          int y = 19 + (i - first) * 10;
          if (i == sel) c.fillRect(0, y - 8, 128, 10, 1);
          say(1, y, String(kindLetter(r)) + clipped(signalName(r), 17), small, i != sel);
          sayRight(127, y, String(int(r.rssi)), small, i != sel);
        }
        if (st.length()) say(0, 49, clipped(st, 25), small);
      }
    }
    break;
  }
  case Gps: {
    title = "GPS";
    bool fix = hardware.gpsFix();
    say(0, 23,
        !config.gps                     ? t("GPS off")
        : fix                           ? t("Position fix")
        : hardware.clockConflict        ? t("Old GPS date")
        : hardware.gps.passedChecksum() ? t("Searching sky")
                                        : t("No data from GPS"),
        bold);
    say(0, 33,
        t("Satellites ") + String(hardware.gps.satellites.value()) + "  NMEA " + String(hardware.gps.passedChecksum()),
        small);
    if (fix) say(0, 42, String(hardware.gps.location.lat(), 5) + ", " + String(hardware.gps.location.lng(), 5), small);
    say(0, 51,
        t("Clock ") + clockText(time(nullptr)) + " " +
            (hardware.clockSource == "unset" ? t("not set") : hardware.clockSource),
        small);
    break;
  }
  case Wifi:
    title = "Wi-Fi";
    if (portalActive()) {
      say(0, 23, "MM-" + meshRadio.idText(meshRadio.nodeId).substring(6), bold);
      say(0, 36, portalPassword());
      say(0, 48, "192.168.4.1", small);
    } else {
      say(0, 27, t("Access point off"));
      say(0, 41, t("Web chat and settings"), small);
    }
    break;
  case Ble:
    title = "Bluetooth";
    if (bleActive()) {
      String n = bleName();
      say(0, 23, n, width(n, bold) <= 128 ? bold : small);
      say(0, 37, "PIN " + String(blePin()), bold);
    } else {
      say(0, 27, t("Bluetooth off"));
      say(0, 41, t("Secure pairing"), small);
    }
    break;
  case Settings:
    title = t("Settings");
    say(0, 20, t("Language: ") + langNames[config.lang < LangCount ? config.lang : 0], small);
    say(0, 28, t("Battery: ") + (config.batteryVolts ? t("volts") : t("percent")), small);
    say(0, 36, t("Screen off: ") + (config.dimAfter ? String(config.dimAfter) + t(" s") : t("never")), small);
    say(0, 44, t("Contrast: ") + String(config.brightness), small);
    say(0, 52,
        String(config.frequency, 3) + " SF" + String(config.sf) + " CR4/" + String(config.cr) + " " +
            String(config.power) + "dBm",
        small);
    break;
  case FolderPage:
    title = appsFolderName(folderAt);
    drawFolderMono();
    break;
  default:
    title = t("Modules");
    say(0, 21,
        "LoRa " + String(meshRadio.ready ? "OK" : "ERR") + "  FS " + String(hardware.fsOk ? "OK" : "ERR") + "  GPS " +
            String(!config.gps                     ? "-"
                   : hardware.gps.passedChecksum() ? "OK"
                                                   : "?"),
        small);
    say(0, 30,
        String(ESP.getFreeHeap() / 1024) + "K RAM" +
            (ESP.getPsramSize() ? "  " + String(ESP.getFreePsram() / 1024) + "K PSRAM" : String()),
        small);
    say(0, 39, t("relayed ") + String(meshRadio.relayed) + t("  rejected ") + String(meshRadio.rejected), small);
    say(0, 48, t("up ") + ago(millis()) + t("  boot ") + String(config.bootCounter), small);
  }
  header(title);
  if (menuOpen)
    drawMenu();
  else
    footer(page == FolderPage ? t("hold: open")
           : appClosed()      ? t("hold: open")
           : page == QuickPage
               ? (quickTargets || !quickIndex || quickIndex > quickSend::Count ? t("click-next hold-run")
                                                                               : t("click-next hold-send"))
               : hint());
  if (action.length() && millis() - actionAt < 3500) {
    c.fillRect(0, 36, 128, 20, 0);
    c.drawRect(0, 36, 128, 20, 1);
    say(3, 50, clipped(action, 20));
  }
  hardware.flush();
}
} // namespace
// Double click = back, as on the stock MeshCore screen (config.doubleBack, off by default): a click waits DoubleMs
// for a second one, so with the option on a single click acts that much later.
static constexpr uint32_t DoubleMs = 400;
static uint32_t clickAt = 0;
// One step back: a popup or menu closes, an edit is left, an open app closes, else the previous page.
static void stepBack() {
  if (popupAt) {
    popupAt = 0;
    return;
  }
  if (radioEdit) {
    if (radioEditing) {
      radioEditing = false;
      radioDigit = 0;
    } else
      radioEdit = false;
    return;
  }
  if (menuOpen) {
    menuOpen = false;
    return;
  }
  if (page == QuickPage && quickTargets) {
    quickTargets = false;
    return;
  }
  if (appPage() && appOpen) {
    appOpen = false;
    quickReply = false;
    return;
  }
  int f = folderBack();
  showPage(f >= 0 ? f : previousPage()); // an app of a folder: back to the folder screen
}
static void uiKeyNow(int key);
void uiKey(int key) {
  if (powerOffPending()) return;
  if (clickAt) {
    bool second = key == 13;
    clickAt = 0;
    if (second) {
      lastInput = millis();
      dirty = true;
      stepBack();
      return;
    }
    uiKeyNow(13);
  } // another key: the waiting click first
  if (key == 13 && config.doubleBack && !screenOff) {
    clickAt = millis();
    if (!clickAt) clickAt = 1;
    lastInput = millis();
    return;
  }
  uiKeyNow(key);
}
static void uiKeyNow(int key) {
  if (powerOffPending()) return; // the screen says it is turning off
  lastInput = millis();
  dirty = true;
  if (screenOff) {
    screenOff = false;
    hardware.brightness(config.brightness);
    return;
  } // the first press only wakes the panel
  if (radioEdit) {
    radioButton(key == 13 || key == 0x82, key == 0xa3);
    return;
  }
  if (popupAt) {
    popupAt = 0;
    if (key == 13) return;
  }
  if (page == FolderPage && !menuOpen) {
    uint8_t p[AppsMax];
    if (key == 0xa3) {
      if (folderPages(folderAt, p)) showPage(p[0]);
    } else if (key == 0x86)
      showPage(Home);
    else if (key == 13 || key == 0x82)
      showPage(nextPage());
    return;
  }
  if (appClosed() && !menuOpen) {
    if (key == 0xa3) {
      appOpen = true;
      quickTargets = false;
      quickIndex = 0;
    } else if (key == 0x86)
      showPage(Home);
    else if (key == 13 || key == 0x82)
      showPage(nextPage());
    return;
  }
  if (quickKey(key)) return;
  if (menuOpen) {
    menuAt = millis();
    Act acts[MenuMax];
    unsigned n = actions(acts);
    if (!n) {
      menuOpen = false;
      return;
    }
    if (key == 13 || key == 0x82) {
      menuIndex = (menuIndex + 1) % n;
      return;
    }
    if (key == 0xa3) {
      Act a = acts[menuIndex % n];
      run(a);
      if (!keepsMenu(a)) menuOpen = false;
    }
    return;
  }
  if (key == 0x86) {
    showPage(Home);
    return;
  }
  if (key == 13 || key == 0x82) {
    showPage(nextPage());
    return;
  }
  if (key == 0xa3) {
    Act acts[MenuMax];
    unsigned n = actions(acts);
    if (n == 1)
      run(acts[0]);
    else if (n > 1) {
      menuOpen = true;
      menuIndex = 0;
      menuAt = millis();
    }
  }
}
bool uiRadarPage() {
  return page == Signals;
}
void uiBegin() {
  if (pins::led >= 0) pinMode(pins::led, OUTPUT);
  led(false);
  lastInput = millis();
  if (meshRadio.historyCount) {
    auto& m = meshRadio.history[meshRadio.historyCount - 1];
    newest = {m.source, m.session, m.id};
  }
  draw();
}
String uiStatus() {
  StaticJsonDocument<1024> d;
  if (page == QuickPage) {
    d["quick_row"] = quickIndex;
    if (!quickTargets && quickIndex && quickIndex <= quickSend::Count) d["quick_index"] = quickIndex - 1;
    d["quick_targets"] = quickTargets;
    if (quickTargets) d["quick_target_index"] = quickTargetIndex;
    d["quick_to"] = meshRadio.idText(quickReply ? quickReplyTo : quickSend::state.to);
  }
  d["action"] = millis() - actionAt < 3500 ? action : String();
  d["page"] = pageNames[page];
  if (radioEdit) {
    d["page"] = "radio";
    d["radio_row"] = radioRow;
    d["radio_editing"] = radioEditing;
    d["radio_digit"] = radioDigit;
    d["radio_draft"] = radioValue(radioDraft, radioRow);
  }
  d["locked"] = false;
  d["app_open"] = appOpen;
  d["menu"] = menuOpen;
  d["menu_index"] = menuIndex;
  if (menuOpen) {
    Act choices[MenuMax];
    d["menu_count"] = actions(choices);
  }
  d["screen_off"] = screenOff;
  d["popup"] = popupAt != 0;
  d["click_wait"] = clickAt != 0; // a click waiting for a second one (double click = back)
  d["unread"] = unreadCount;
  {
    int f = pageFolder();
    if (page == FolderPage || f >= 0) {
      d["folder"] = f >= 0 ? f : folderAt;
      d["in_folder"] = f >= 0;
      if (page == FolderPage) d["folder_sel"] = folderSel;
    }
  }
  if (page == Signals) {
    d["radar_selected"] = shownSignal();
    d["csi_role"] = radar.csi;
  }
  String s;
  serializeJson(d, s);
  return s;
}
bool uiScreenOff() {
  return screenOff;
}
void uiFarewell() {
  farewell = true;
  screenOff = false;
  hardware.brightness(config.brightness);
  draw();
}
void uiTick() {
  uint32_t now = millis();
  if (powerOffPending()) {
    static bool shown = false;
    if (!shown) {
      shown = true;
      screenOff = false;
      hardware.brightness(config.brightness);
      draw();
    }
    return;
  } // drawn once: the board turns off
  if (clickAt && now - clickAt >= DoubleMs) {
    clickAt = 0;
    uiKeyNow(13);
  } // no second click came
  notifications::Event receivedEvent;
  while (notifications::take(receivedEvent)) {
    const auto& e = receivedEvent;
    bool wake = false;
    if (e.kind == notifications::Incoming) {
      if (page != Messages) unreadCount++;
      incomingPopup = e.message;
      bool muted = notifications::muted(e.message);
      if (!muted && notifications::enabled(notifications::mode(e.message, config.notifyLed, 0), e.mention)) {
        ledAt = now;
        ledPattern = e.mention ? 2 : 1;
      }
      wake = !muted && notifications::enabled(notifications::mode(e.message, config.notifyWake, 1), e.mention);
      if (!muted && notifications::enabled(notifications::mode(e.message, config.notifyPopup, 2), e.mention)) {
        popupAt = now;
        menuOpen = false;
      }
      if (notifications::sound(e.message, e.mention)) hardware.beep();
      dirty = true;
    } else if (config.notifyFailed && (e.kind == notifications::TxError || e.kind == notifications::Unconfirmed)) {
      ledAt = now;
      ledPattern = 3;
      notice(e.kind == notifications::TxError             ? t("Radio TX error")
             : channels::isChannel(e.message.destination) ? t("No repeat heard")
                                                          : t("Delivery not confirmed"));
    }
    if (wake) {
      if (screenOff) {
        screenOff = false;
        hardware.brightness(config.brightness);
      }
      lastInput = now;
    }
  }
  if (ledAt) {
    uint32_t e = now - ledAt;
    led(e < 1500 && (e / (ledPattern == 2 ? 125 : ledPattern == 3 ? 100 : 250)) % 2 == 0);
    if (e >= 1500) {
      ledAt = 0;
      led(false);
    }
  }
  // Homing ping on the LED (the V4 has no buzzer): faster as the signal strengthens.
  if (!ledAt && page == Signals && radar.tracking) {
    bool fresh = homingFresh();
    float level = constrain((radar.fast + 85) / 55.f, 0.f, 1.f);
    lastInput = now;
    if (fresh && radar.focus.kind == RadarTarget::Lora) {
      if (radar.samples != pingedSamples) {
        pingedSamples = radar.samples;
        pingAt = now;
      }
    } else if (fresh && now - pingAt >= uint32_t(1200 - 1140 * level * level))
      pingAt = now;
    led(pingAt && now - pingAt < 40);
  } else if (!ledAt && page == Signals && radar.csi == Radar::CsiSensor) {
    lastInput = now;
    pingAt = 1;
    led(radar.moving);
  } // LED on while motion is sensed
  else if (!ledAt && pingAt) {
    pingAt = 0;
    led(false);
  }
  if (menuOpen && now - menuAt > 10000) {
    menuOpen = false;
    dirty = true;
  }
  if (config.dimAfter && !screenOff && now - lastInput >= config.dimAfter * 1000UL) {
    screenOff = true;
    hardware.brightness(0);
  }
  if (screenOff) return;
  if ((dirty || meshRadio.dirty || radar.dirty || now - drawAt > 1000) && now - drawAt > 150) {
    draw();
    drawAt = now;
    dirty = false;
    meshRadio.dirty = false;
    radar.dirty = false;
  }
}
