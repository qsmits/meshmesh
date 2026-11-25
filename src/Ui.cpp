#include "App.h"
#include "HandheldPrivate.h"
#include "ScreenCredential.h"
#include "Hardware.h"
#include "UiIcons.h"
#include "SmoothCanvas.h"
#include "MeshRadio.h"
#include "Remote.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "Internet.h"
#include "Power.h"
#include "Regions.h"
#include "NetFind.h"
#include "RadioPresets.h"
#include "Notifications.h"
#include "ChannelPolicy.h"
#include "QuickSend.h"
#include "Wardrive.h"
#include <Preferences.h>
#include <Mm1Packet.h>
#include <time.h>
#include <math.h>
#include <initializer_list>
#include "Emoji.h"
#include <algorithm>
// Defined in U8g2_for_Adafruit_GFX.cpp; used to fall back to Latin-1 and placeholders for missing glyphs.
uint8_t u8g2_IsGlyph(u8g2_font_t* u8g2, uint16_t encoding);
int8_t u8g2_GetGlyphWidth(u8g2_font_t* u8g2, uint16_t encoding);
// The home tiles, in drawHome() order; the chosen order and the hidden ones: config.apps (App.h).
// The default: messages, places, tools, then the device itself.
const char* const uiApps[] = {"chats",    "quick",    "nodes",   "nav",    "map",     "radar",
                              "spectrum", "wardrive", "connect", "health", "settings"};
const uint8_t uiAppCount = sizeof uiApps / sizeof *uiApps;
namespace {
enum Page {
  Home,
  Threads,
  Chat,
  Map,
  Nodes,
  Sensors,
  Settings,
  Radio,
  Display,
  Network,
  Diagnostics,
  Help,
  Library,
  Node,
  Scope,
  Homing,
  Motion,
  NetList,
  ChannelAdd,
  ChannelInfo,
  Remote,
  QuickView,
  WardriveView,
  Folder,
  Memo,
  Spectrum
};
const char* pageNames[] = {"home",  "threads",  "chat",    "map",           "nodes",       "sensors", "settings",
                           "radio", "display",  "network", "diagnostics",   "help",        "library", "node",
                           "radar", "homing",   "motion",  "internet",      "channel_add", "channel", "remote",
                           "quick", "wardrive", "folder",  "contact_notes", "spectrum"};
enum Key {
  Enter = 13,
  Erase = 8,
  KeyMsg = 0x81,
  KeyHome = 0x82,
  KeyMenu = 0x83,
  KeyAdv = 0x84,
  KeyMap = 0x85,
  KeyBack = 0x86,
  KeyGps = 0x87,
  KeyMic = 0x88,
  KeySet = 0x90,
  KeyHold = 0xa3,
  KeyLeft = 0xb4,
  KeyUp = 0xb5,
  KeyDown = 0xb6,
  KeyRight = 0xb7
};
enum class Panel { Closed, Actions, Radios };
Panel activePanel = Panel::Closed;
int panelRow = 0, radioReturnRow = 0;
Page page = Home, chatReturn = Threads,
     subReturn = Settings; // subReturn: where BACK leaves a page opened from several places
// App folders (Apps.cpp): the folder on the Folder page, and the folder the open app came from (BACK returns there, -1: Home).
int folderOpen = -1, appFolder = -1;
bool quickTargets = false;
uint64_t quickReplyTo = 0;
bool scopeHeld = false, scopeWide = false, csiBeaconRole = false;
RadarTarget scopeRows[Radar::MaxTargets]; // Exactly the list last painted, including names and signal strengths.
int scopeCount = 0, scopeRow = 0;
uint32_t scopeFrameAt = 0, scopeFrames = 0;
// radar list: the user moved the selection off the strongest signal
uint32_t lastDraw = 0, lastInput = 0, toastAt = 0;
String toast, eventSeen;
uint16_t toastColor = 0;
int selected = 0, chatOffset = 0, action = 0;
uint64_t recipient = meshmesh::Broadcast, focusNode = 0, focusChannel = 0;
String composer, edit;
bool clipMenu = false, editing = false, dirty = true, keyed = false, locked = false, wakeOnly = false,
     deleteArmed = false;
Config draft;
uint32_t lastPhysicalInput = 0;
struct ComposerDraft {
  uint64_t recipient = 0;
  String text;
  uint32_t touched = 0;
} drafts[16];
struct ReadMark {
  uint64_t id;
  uint32_t at;
} marks[65];
unsigned markCount = 0;
DynamicJsonDocument library(8192);
uint64_t conversations[65];
unsigned conversationCount = 1;
unsigned nodeOrder[24], nodeTotal = 0;

// Palette: neutral dark surfaces, colour reserved for focus and state.
constexpr uint16_t rgb(uint32_t v) {
  return ((v >> 8) & 0xf800) | ((v >> 5) & 0x07e0) | ((v >> 3) & 0x1f);
}
struct Palette {
  uint16_t background, bar, card, selected, line, ink, dim, faint, accent, ok, warn, bad, info, violet, outBubble,
      inBubble;
};
constexpr Palette darkPalette = {rgb(0x101820), rgb(0x101820), rgb(0x1c2934), rgb(0x293b42),
                                 rgb(0x394b58), rgb(0xf6f5eb), rgb(0xb9c8ce), rgb(0x92a6af),
                                 rgb(0xd7ee83), rgb(0x83d4ab), rgb(0xf4c477), rgb(0xff8c80),
                                 rgb(0x93c8eb), rgb(0xc1b0df), rgb(0x28433e), rgb(0x1c2934)};
constexpr Palette lightPalette = {rgb(0xf7f6f0), rgb(0xe8ece6), rgb(0xe9ede8), rgb(0xd7e5e0),
                                  rgb(0xb2c1bb), rgb(0x182b30), rgb(0x344e52), rgb(0x4b6267),
                                  rgb(0x435d16), rgb(0x176341), rgb(0x805000), rgb(0xa22c2c),
                                  rgb(0x215b80), rgb(0x62418b), rgb(0xd6e8d9), rgb(0xe9ede8)};
Palette palette = darkPalette;
auto& [bg, bar, card, cardHi, line, ink, dim, faint, accent, ok, warn, bad, info, violet, outBubble, inBubble] =
    palette;
bool lightTheme = false;
void applyAppearance() {
  const Config& c = page == Display && !locked ? draft : config;
  time_t now = time(nullptr);
  lightTheme = c.lightAt(now);
  palette = lightTheme ? lightPalette : darkPalette;
  const uint16_t accents[] = {accent, info, violet, warn};
  accent = accents[c.accentColor % 4];
  if (!wakeOnly) {
    uint8_t level = c.backlight(now);
    if (page == Display && !locked && selected == 2) level = c.brightness;
    if (page == Display && !locked && selected == 15) level = c.nightBrightness ? c.nightBrightness : c.brightness;
    hardware.brightness(level);
  }
}
const uint16_t avatarHues[] = {rgb(0x2f7d6f), rgb(0x3f6fb5), rgb(0x8a5cc2), rgb(0xb5693f),
                               rgb(0x4f8a3a), rgb(0xa8466a), rgb(0x3a8aa0), rgb(0x8f7a2e)};

// Text: the Cyrillic fonts lack Latin-1 and punctuation such as "·", "°", "№"; other languages' letters
// come from the fallback fonts of I18n.h, and Arabic is drawn in visual order.
struct GlyphRange {
  uint16_t first, last, offset;
};
struct CoverageGlyph {
  uint16_t offset;
  int8_t left, top;
  uint8_t width, height, advance;
};
struct CoverageFace {
  const CoverageGlyph* glyphs;
  const uint8_t* pixels;
};
#include "DisplayFontData.inc"
struct Face {
  const uint8_t* main;
  const uint8_t* latin;
  const CoverageFace* coverage = nullptr;
};
const Face body{u8g2_font_6x13_t_cyrillic, u8g2_font_6x13_tf, &faceBody},
    bold{u8g2_font_6x13B_t_cyrillic, u8g2_font_6x13B_tf, &faceLabel},
    small{u8g2_font_5x8_t_cyrillic, u8g2_font_5x8_tf, &faceCaption};
const Face big{u8g2_font_10x20_t_cyrillic, u8g2_font_10x20_tf, &faceTitle}, digits{u8g2_font_logisoso42_tn, nullptr};

// Blend into the existing canvas so glyphs also work over maps, bubbles and focus fills.
void paintCoverage(int x, int y, const CoverageGlyph& glyph, const uint8_t* mask, uint16_t color) {
  auto& canvas = *hardware.canvas;
  x += glyph.left;
  y += glyph.top;
  const int stride = canvas.width();
  const int endX = std::min<int>(glyph.width, stride - x);
  const int endY = std::min<int>(glyph.height, canvas.height() - y);
  for (int row = std::max(0, -y); row < endY; row++) {
    for (int col = std::max(0, -x); col < endX; col++) {
      unsigned i = row * glyph.width + col;
      unsigned alpha = (i & 1) ? (mask[i / 2] & 15) : (mask[i / 2] >> 4);
      if (alpha) {
        uint16_t& pixel = canvas.getBuffer()[(row + y) * stride + x + col];
        pixel = alpha == 15 ? color : blend565(pixel, color, alpha);
      }
    }
  }
}
const uint8_t* activeFont = nullptr;
void useFont(const uint8_t* f) {
  if (f != activeFont) {
    hardware.font.setFont(f);
    hardware.font.setFontMode(1);
    activeFont = f;
  }
} // SetFont resets transparency
uint32_t nextCp(const String& s, unsigned& i) {
  uint8_t c = s[i];
  unsigned n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
  uint32_t cp = n == 1 ? c : n == 2 ? c & 0x1f : n == 3 ? c & 0x0f : c & 0x07;
  for (unsigned k = 1; k < n && i + k < s.length(); k++) cp = cp << 6 | (uint8_t(s[i + k]) & 0x3f);
  i += n;
  return cp;
}
const char* substitute(uint32_t cp) {
  switch (cp) {
  case 0x2026: return "...";
  case 0x2013:
  case 0x2014:
  case 0x2212: return "-";
  case 0x2116: return "No";
  case 0x2018:
  case 0x2019: return "'";
  case 0x201c:
  case 0x201d:
  case 0x201e: return "\"";
  case 0x2192: return "->";
  case 0x2190: return "<-";
  case 0x2022: return "\xc2\xb7";
  }
  return nullptr;
}
// Emoji (Emoji.h): the picture in its colours, 12 px (8 px in the small face) from 1 px under the baseline,
// 2 px apart (the pictures fill their squares).
int glyph(int x, int y, uint32_t cp, const Face& f, uint16_t color, bool paint) {
  auto& u = hardware.font;
  const uint8_t* use = nullptr;
  if (char l = emoji::flagLetter(cp)) cp = uint8_t(l);
  if (cp >= 0x203c) {
    if (emoji::modifier(cp)) return 0;
    int e = emoji::find(cp);
    if (e >= 0) {
      int s = f.main == small.main ? 8 : emoji::Size;
      uint8_t px[emoji::Size * emoji::Size];
      if (paint && emoji::decode(e, px))
        emoji::draw(px, x, y - s + 1 + s / 12, s,
                    [](int a, int b, uint8_t v) { hardware.canvas->drawPixel(a, b, rgb(emoji::palette[v - 1])); });
      return s + 2;
    }
  }
  if (f.coverage) {
    for (const auto& range : coverageRanges) {
      if (cp < range.first) break;
      if (cp <= range.last) {
        const auto& metric = f.coverage->glyphs[range.offset + cp - range.first];
        if (paint) paintCoverage(x, y, metric, f.coverage->pixels + metric.offset, color);
        return metric.advance;
      }
    }
  }
  if (cp <= 0xffff) {
    useFont(f.main);
    if (u8g2_IsGlyph(&u.u8g2, cp))
      use = f.main;
    else if (f.latin) {
      useFont(f.latin);
      if (u8g2_IsGlyph(&u.u8g2, cp)) use = f.latin;
    }
  }
  if (!use && cp <= 0xffff)
    for (const uint8_t* const* x = fallbackFonts(f.main); *x; x++)
      if (fontHasGlyph(*x, cp)) {
        use = *x;
        break;
      } // other languages' letters
  if (!use) {
    if (const char* sub = substitute(cp)) {
      String s(sub);
      int w = 0;
      for (unsigned i = 0; i < s.length();) w += glyph(x + w, y, nextCp(s, i), f, color, paint);
      return w;
    }
    useFont(f.main);
    int w = u8g2_GetGlyphWidth(&u.u8g2, '0'), a = u.getFontAscent();
    if (paint) hardware.canvas->drawRect(x + 1, y - a, max(2, w - 2), a, color);
    return w; // visible placeholder, e.g. emoji
  }
  useFont(use);
  if (!paint) return u8g2_GetGlyphWidth(&u.u8g2, cp);
  u.setForegroundColor(color);
  return u.drawGlyph(x, y, cp);
}
int measure(const String& text, const Face& f = body) {
  String s = visualText(text);
  int w = 0;
  for (unsigned i = 0; i < s.length();) w += glyph(0, 0, nextCp(s, i), f, 0, false);
  return w;
}
int text(int x, int y, const String& logical, uint16_t color = ink, const Face& f = body) {
  String s = visualText(logical);
  for (unsigned i = 0; i < s.length();) x += glyph(x, y, nextCp(s, i), f, color, true);
  return x;
}
void textRight(int x, int y, const String& s, uint16_t color = ink, const Face& f = body) {
  text(x - measure(s, f), y, s, color, f);
}
void textCenter(int x, int y, const String& s, uint16_t color = ink, const Face& f = body) {
  text(x - measure(s, f) / 2, y, s, color, f);
}
String fit(const String& s, int px, const Face& f = body) {
  if (measure(s, f) <= px) return s;
  int dots = measure("...", f), w = 0;
  unsigned i = 0, end = 0;
  while (i < s.length()) {
    int g = glyph(0, 0, nextCp(s, i), f, 0, false);
    if (w + g + dots > px) break;
    w += g;
    end = i;
  }
  return s.substring(0, end) + "...";
}
// Word wrap by pixel width; long words break anywhere.
unsigned wrap(const String& s, String* rows, unsigned maxRows, int px, const Face& f = body) {
  unsigned count = 0, i = 0;
  String row;
  int rowW = 0;
  auto push = [&] {
    if (count < maxRows) rows[count++] = row;
    row = "";
    rowW = 0;
  };
  while (i < s.length() && count < maxRows) {
    if (s[i] == '\n') {
      push();
      i++;
      continue;
    }
    unsigned start = i;
    int w = 0;
    while (i < s.length() && s[i] != '\n') {
      uint32_t cp = nextCp(s, i);
      w += glyph(0, 0, cp, f, 0, false);
      if (cp == ' ') break;
    }
    String word = s.substring(start, i);
    if (rowW + w <= px) {
      row += word;
      rowW += w;
      continue;
    }
    if (rowW) {
      push();
      if (count >= maxRows) break;
    }
    if (w <= px) {
      row = word;
      rowW = w;
      continue;
    }
    for (unsigned j = 0; j < word.length();) {
      unsigned at = j;
      int g = glyph(0, 0, nextCp(word, j), f, 0, false);
      if (rowW + g > px) {
        push();
        if (count >= maxRows) break;
      }
      row += word.substring(at, j);
      rowW += g;
    }
  }
  if (row.length() && count < maxRows) rows[count++] = row;
  return count;
}
#define t(en) String(tr(en)) // a macro: tr() keys its translation at compile time (I18n.h)
String flag(bool v) {
  return v ? t("On") : t("Off");
}
String count(unsigned n, const char* one, const char* many) {
  return plural(n, one, many);
}
void notice(const String& value, uint16_t color = accent) {
  toast = value;
  toastAt = millis();
  toastColor = color;
  dirty = true;
}

// Drawing primitives and icons (drawn, not font glyphs, so they scale and take state colours).
SmoothCanvas& g() {
  static SmoothCanvas canvas(*hardware.canvas);
  return canvas;
}
void panel(int x, int y, int w, int h, uint16_t fill, int r = 6) {
  g().fillRoundRect(x, y, w, h, r, fill);
}
void ring(int x, int y, int w, int h, int r = 6, uint16_t color = accent) {
  g().drawRoundRect(x, y, w, h, r, color, 2);
}
void tri(int x, int y, int dir, int s, uint16_t c) {
  if (dir == 0)
    g().fillTriangle(x - s, y + s / 2, x + s, y + s / 2, x, y - s / 2 - 1, c);
  else if (dir == 2)
    g().fillTriangle(x - s, y - s / 2, x + s, y - s / 2, x, y + s / 2 + 1, c);
  else if (dir == 1)
    g().fillTriangle(x - s / 2, y - s, x - s / 2, y + s, x + s / 2 + 1, y, c);
  else
    g().fillTriangle(x + s / 2, y - s, x + s / 2, y + s, x - s / 2 - 1, y, c);
}
void arc(int cx, int cy, int r, int a0, int a1, uint16_t c) {
  for (int a = a0; a <= a1; a += 3) {
    float rad = a * M_PI / 180;
    g().drawPixel(cx + roundf(r * sinf(rad)), cy - roundf(r * cosf(rad)), c);
  }
}
void thickLine(int x0, int y0, int x1, int y1, uint16_t c) {
  g().strokeLine(x0, y0, x1, y1, 2, c);
}
void icon(Icon id, int cx, int cy, int s, uint16_t c, uint16_t hole = bg) {
  drawIcon(g(), id, cx, cy, s, c, hole, faint);
  if (id == IcHelp) textCenter(cx, cy + 5, "?", c, bold);
}
void bars(int x, int y, unsigned level, uint16_t color) {
  for (unsigned k = 0; k < 4; k++) {
    int h = 3 + k * 2;
    g().fillRect(x + k * 4, y - h, 3, h, k < level ? color : line);
  }
}
unsigned snrLevel(float snr) {
  return snr >= 5 ? 4 : snr >= 0 ? 3 : snr >= -5 ? 2 : snr >= -10 ? 1 : 0;
}
// Avatars: initial of the name, colour from the ID.
String initial(const String& name) {
  if (!name.length()) return "?";
  unsigned i = 0;
  uint32_t cp = nextCp(name, i);
  if (cp >= 0x430 && cp <= 0x44f)
    cp -= 0x20;
  else if (cp == 0x451)
    cp = 0x401;
  else if (cp < 128)
    cp = toupper(cp);
  else if (cp >= 0x800)
    return "?";
  char b[4] = {};
  if (cp < 0x80)
    b[0] = cp;
  else {
    b[0] = 0xc0 | cp >> 6;
    b[1] = 0x80 | (cp & 0x3f);
  }
  return b;
}
void avatar(int cx, int cy, int r, uint64_t id, const String& name, int type = -1) {
  uint16_t hue = avatarHues[(id ^ id >> 17 ^ id >> 41) % 8];
  if (channels::isChannel(id)) {
    const channels::Channel* c = meshRadio.channel(id);
    bool priv = c && !channels::isPublic(c->secret) && !channels::isHashtag(*c);
    g().fillCircle(cx, cy, r, lightTheme ? cardHi : priv ? rgb(0x2e2450) : rgb(0x14524b));
    icon(priv ? IcLock : IcHash, cx, cy, r / 2, priv ? violet : accent);
    return;
  }
  if (type > 1) {
    uint16_t tc = type == 2 ? violet : type == 3 ? warn : info;
    g().fillCircle(cx, cy, r, card);
    g().drawCircle(cx, cy, r, tc);
    icon(type == 2 ? IcTower : type == 3 ? IcRoom : IcSensor, cx, cy, r / 2 + 1, tc, card);
    return;
  }
  g().fillCircle(cx, cy, r, hue);
  textCenter(cx, cy + (r > 12 ? 5 : 4), initial(name), rgb(0xffffff), r > 12 ? bold : small);
}
// Touch targets of the frame on screen (uiTouch): the items a tap selects (index as in "selected")
// and the footer hints, which act as their keys. draw() clears them.
struct Target {
  int16_t x, y, w, h, index;
};
Target targets[16];
unsigned targetCount = 0;
struct HintSpot {
  int16_t x0, x1;
  int key;
};
HintSpot hintSpots[6];
unsigned hintCount = 0;
// index >= 0 selects that item; a negative index is a key (-KeyRight).
void target(int x, int y, int w, int h, int index) {
  if (targetCount < 16) targets[targetCount++] = {int16_t(x), int16_t(y), int16_t(w), int16_t(h), int16_t(index)};
}
int hintKey(const char* k) {
  if (!strcmp(k, tr("hold OK"))) return KeyHold;
  static const struct {
    const char* name;
    int key;
  } keys[] = {{"OK", Enter},     {"BACK", KeyBack}, {"DEL", Erase},  {"MSG", KeyMsg},  {"MAP", KeyMap},
              {"HOME", KeyHome}, {"PIN", KeyAdv},   {"MIC", KeyMic}, {"CTRL", KeySet}, {"SPC", ' '},
              {"<>", KeyRight},  {"<", KeyLeft},    {"^v", KeyDown}, {"+/-", '+'},     {"MENU", KeyMenu}};
  for (auto& v : keys)
    if (!strcmp(k, v.name)) return v.key;
  return k[0] >= 'A' && k[0] <= 'Z' && !k[1] ? k[0] + 32 : 0; // letter keys (map)
}
// Softkey footer: [KEY] action pairs; "<>" and "^v" draw arrow pairs.
struct Hint {
  const char* key;
  String action;
};
void footer(std::initializer_list<Hint> hints) {
  auto& d = g();
  d.fillRect(0, 221, 320, 19, bar);
  d.drawFastHLine(0, 220, 320, line);
  int x = 6;
  auto isArrow = [](const char* key) { return !strcmp(key, "<>") || !strcmp(key, "^v") || !strcmp(key, "<"); };
  auto keyWidth = [&](const char* k) {
    return isArrow(k) || !strcmp(k, "MENU") || !strcmp(k, "PIN") ? 17 : measure(k, small) + 8;
  };
  int total = 0;
  for (const auto& h : hints) total += keyWidth(h.key) + 4 + measure(h.action, body) + 10;
  const Face& label = total - 10 <= 310 ? body : small;
  for (auto& h : hints) {
    int kw = keyWidth(h.key), aw = measure(h.action, label);
    if (x + kw + 4 + aw > 316) break;
    if (int key = hintKey(h.key))
      if (hintCount < 6) hintSpots[hintCount++] = {int16_t(x - 3), int16_t(x + kw + 4 + aw + 5), key};
    d.fillRoundRect(x, 224, kw, 13, 3, line);
    if (!strcmp(h.key, "MENU") || !strcmp(h.key, "PIN"))
      icon(!strcmp(h.key, "MENU") ? IcChat : IcPin, x + 8, 230, 5, ink, line);
    else if (!strcmp(h.key, "<>")) {
      tri(x + 5, 230, 3, 3, ink);
      tri(x + 12, 230, 1, 3, ink);
    } else if (!strcmp(h.key, "<"))
      tri(x + 8, 230, 3, 3, ink);
    else if (!strcmp(h.key, "^v")) {
      tri(x + 5, 231, 0, 3, ink);
      tri(x + 12, 229, 2, 3, ink);
    } else
      text(x + 4, 234, h.key, ink, small);
    text(x + kw + 4, label.main == small.main ? 234 : 235, h.action, dim, label);
    x += kw + 4 + aw + 10;
  }
}
// Conversations, read marks and drafts.
bool belongs(const ChatMessage& m, uint64_t id) {
  return channels::isChannel(id)
             ? m.destination == id
             : (m.outgoing ? m.destination == id
                           : m.source == id && (m.destination == meshRadio.nodeId || m.protocol == 1));
}
// Chats: joined channels (Public first), then direct conversations, newest first.
// A left channel's messages stay in the history but out of the list.
uint64_t focusedThread = meshmesh::Broadcast;
void threads(bool followFocus = true) {
  conversationCount = 0;
  for (unsigned i = 0; i < meshRadio.channelCount; i++)
    conversations[conversationCount++] = meshRadio.channelList[i].id;
  const unsigned channelRows = conversationCount;
  for (int i = int(meshRadio.historyCount) - 1; i >= 0; i--) {
    auto& m = meshRadio.history[i];
    if (channels::isChannel(m.destination)) continue;
    uint64_t id = m.outgoing ? m.destination : m.source;
    bool found = false;
    for (unsigned j = channelRows; j < conversationCount; j++)
      if (conversations[j] == id) found = true;
    if (!found && conversationCount < 65) conversations[conversationCount++] = id;
  }
  if (page == Threads) {
    if (followFocus) {
      for (unsigned i = 0; i < conversationCount; i++)
        if (conversations[i] == focusedThread) {
          selected = i;
          break;
        }
    }
    // If the conversation vanished, keep the nearest remaining row.
    selected = constrain(selected, 0, max(0, int(conversationCount) - 1));
    if (conversationCount) focusedThread = conversations[selected];
  }
}
Peer* peerOf(uint64_t id) {
  for (unsigned i = 0; i < meshRadio.peerCount; i++)
    if (meshRadio.peers[i].id == id) return &meshRadio.peers[i];
  return nullptr;
}
String nameOf(uint64_t id) {
  if (channels::isChannel(id)) {
    const channels::Channel* c = meshRadio.channel(id);
    return c ? String(c->name) : t("Channel");
  }
  if (Peer* p = peerOf(id)) return p->name;
  for (int i = meshRadio.historyCount - 1; i >= 0; i--)
    if (meshRadio.history[i].source == id) return meshRadio.history[i].name;
  return meshRadio.idText(id);
}
String readKey(uint64_t id) {
  if (id == meshmesh::Broadcast) return "all";
  char b[15] = {};
  unsigned n = 0;
  do {
    b[n++] = "0123456789abcdefghijklmnopqrstuvwxyz"[id % 36];
    id /= 36;
  } while (id);
  String key = "r";
  while (n) key += b[--n];
  return key;
}
// Read marks: one file in MeshMesh storage (NVS is small); a mark an older firmware left in NVS is read
// once and removed when that chat is read again. Without the storage they stay in NVS.
struct ReadFile {
  uint32_t version = 1, count = 0;
  ReadMark marks[65] = {};
  uint32_t check = 0;
};
uint32_t fnv(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
bool marksLoaded = false;
void loadMarks() {
  marksLoaded = true;
  ReadFile* f = new ReadFile;
  if (readStored("/meshmesh/read.bin", "/meshmesh/read.tmp", f, sizeof(*f))) {
    if (f->version == 1 && f->count <= 65 && f->check == fnv((const uint8_t*)f, offsetof(ReadFile, check)))
      for (unsigned i = 0; i < f->count && markCount < 65; i++) marks[markCount++] = f->marks[i];
  }
  delete f;
}
bool saveMarks() {
  ReadFile* f = new ReadFile;
  f->count = markCount;
  for (unsigned i = 0; i < markCount; i++) f->marks[i] = marks[i];
  f->check = fnv((const uint8_t*)f, offsetof(ReadFile, check));
  bool saved = writeStored("/meshmesh/read.bin", "/meshmesh/read.tmp", f, sizeof(*f));
  delete f;
  return saved;
}
uint32_t readAt(uint64_t id) {
  if (!marksLoaded) loadMarks();
  for (unsigned i = 0; i < markCount; i++)
    if (marks[i].id == id) return marks[i].at;
  uint32_t v = 0;
  Preferences p;
  if (p.begin("meshmesh-ui", true)) {
    v = p.getUInt(readKey(id).c_str(), 0);
    p.end();
  }
  if (markCount < 65) marks[markCount++] = {id, v};
  return v; // cached: status bar needs unread totals every frame
}
unsigned unread(uint64_t id) {
  uint32_t at = readAt(id);
  unsigned n = 0;
  for (unsigned i = 0; i < meshRadio.historyCount; i++) {
    auto& m = meshRadio.history[i];
    if (belongs(m, id) && !m.outgoing && !m.seen && (m.timestamp > at || (!m.timestamp && m.uptime))) n++;
  }
  return n;
}
void nextUnread() {
  threads();
  for (unsigned step = 1; step <= conversationCount; ++step) {
    unsigned row = (selected + step) % conversationCount;
    if (unread(conversations[row])) {
      selected = row;
      focusedThread = conversations[row];
      return;
    }
  }
  notice(t("No unread messages"), dim);
}
unsigned unreadTotal() {
  threads();
  unsigned n = 0;
  for (unsigned i = 0; i < conversationCount; i++)
    if (conversations[i]) n += unread(conversations[i]);
  return n;
}
void markRead() {
  uint32_t at = 0;
  for (unsigned i = 0; i < meshRadio.historyCount; i++) {
    auto& m = meshRadio.history[i];
    if (belongs(m, recipient) && !m.outgoing) {
      at = max(at, m.timestamp);
      m.seen = true;
    }
  }
  if (at <= readAt(recipient)) return;
  for (unsigned i = 0; i < markCount; i++)
    if (marks[i].id == recipient) marks[i].at = at;
  Preferences p;
  String key = readKey(recipient);
  if (saveMarks()) {
    if (p.begin("meshmesh-ui", false)) {
      if (p.isKey(key.c_str())) p.remove(key.c_str());
      p.end();
    }
  } else if (p.begin("meshmesh-ui", false)) {
    p.putUInt(key.c_str(), at);
    p.end();
  }
}
void rememberComposer() {
  int chosen = -1;
  for (int i = 0; i < 16; i++)
    if (drafts[i].recipient == recipient) {
      chosen = i;
      break;
    }
  if (chosen < 0)
    for (int i = 0; i < 16; i++)
      if (!drafts[i].recipient) {
        chosen = i;
        break;
      }
  if (chosen < 0) {
    chosen = 0;
    for (int i = 1; i < 16; i++)
      if (drafts[i].touched < drafts[chosen].touched) chosen = i;
  }
  drafts[chosen].recipient = recipient;
  drafts[chosen].text = composer;
  drafts[chosen].touched = millis();
}
String restoredComposer() {
  for (auto& d : drafts)
    if (d.recipient == recipient) return d.text;
  return "";
}
// Nodes: most recently heard first; focus follows the node, not its row.
void listRow(int y, int h, bool focus);
#include "UiContactNotes.inc"
void sortNodes() {
  nodeTotal = 0;
  for (unsigned i = 0; i < meshRadio.peerCount; i++)
    if (!tagFilter.length() || tagMatches(contactMemo(meshRadio.peers[i]).memo.tags)) nodeOrder[nodeTotal++] = i;
  uint32_t now = millis();
  auto before = [&](const Peer& a, const Peer& b) {
    if (a.heard != b.heard) return a.heard;
    return a.heard && now - a.seen < now - b.seen;
  };
  for (unsigned i = 1; i < nodeTotal; i++)
    for (unsigned j = i; j > 0 && before(meshRadio.peers[nodeOrder[j]], meshRadio.peers[nodeOrder[j - 1]]); j--)
      std::swap(nodeOrder[j], nodeOrder[j - 1]);
  if (!nodeTotal) {
    selected = 0;
    return;
  }
  for (unsigned i = 0; i < nodeTotal; i++)
    if (meshRadio.peers[nodeOrder[i]].id == focusNode) {
      selected = i;
      return;
    }
  selected = constrain(selected, 0, int(nodeTotal) - 1);
  focusNode = meshRadio.peers[nodeOrder[selected]].id;
}
Peer* focusedPeer() {
  return peerOf(focusNode);
}
String netError();
bool sharedPage(Page p) {
  return p == Radio || p == Display || p == Sensors || p == Network || p == Diagnostics || p == Help || p == Library;
}
// Leaving the radar pages keeps the radar running while the web page or the wardrive nets log holds it.
// A page opened from the menu, the settings, the map or a hotkey goes back there (NetList is Network's own).
// Back restores the cursor for the parent page. A fresh visit starts at row zero.
int lastRow[sizeof(pageNames) / sizeof(*pageNames)] = {};
int lastNodeAction = 0, lastChatOffset = 0;
uint64_t lastChatId = 0;
Page quickReturn = Home;
void resetSpectrum();
void stopSweep();
void change(Page next, bool back = false) {
  if (page == Spectrum || next == Spectrum) resetSpectrum();
  activePanel = Panel::Closed;
  clipMenu = false;
  if (next == Home && appFolder >= 0 && page != Home && page != Folder) {
    uint8_t order[AppEntriesMax];
    if (appsShown(order, appFolder)) {
      folderOpen = appFolder;
      next = Folder;
      back = true;
    }
  }
  if (next == Home) appFolder = -1;
  lastRow[page] = selected;
  if (page == Node) lastNodeAction = action;
  if (page == Chat && next != Chat) {
    rememberComposer();
    lastChatId = recipient;
    lastChatOffset = chatOffset;
  }
  if (next == QuickView) quickReturn = page == Chat ? Chat : Home;
  if (sharedPage(next) && !sharedPage(page) && page != NetList) subReturn = page;
  bool radarPage = next == Scope || next == Homing || next == Motion;
  if (radarPage && page != Scope && page != Homing && page != Motion) {
    scopeHeld = false;
    scopeWide = false;
    scopeCount = scopeRow = 0;
  }
  if (radarPage)
    radar.open();
  else if (!webRadarActive() && !wardrive::holdsRadar())
    radar.close();
  if (radarPage || (!webRadarActive() && !wardrive::holdsRadar()))
    radar.setCsi(next != Motion ? Radar::CsiOff : csiBeaconRole ? Radar::CsiBeacon : Radar::CsiSensor);
  if (next == Scope) radar.untrack();
  page = next;
  selected = back ? lastRow[next] : 0;
  action = back && next == Node ? lastNodeAction : 0;
  chatOffset = back && next == Chat && lastChatId == recipient ? lastChatOffset : 0;
  editing = false;
  deleteArmed = false;
  dirty = true;
  if (next == Radio || next == Display) draft = config;
  if (next == Threads) threads(back);
  if (next == Chat) {
    composer = restoredComposer();
    markRead();
  }
  if (next == Library) deserializeJson(library, maps.areas());
  if (next == Nodes) sortNodes();
  if (next == NetList) internet.rescan();
}

// Time, distances and short labels.
bool localTime(time_t at, tm& out) {
  if (at < 1700000000) return false;
  at += config.utcOffset * 60;
  out = *gmtime(&at);
  return true;
}
String clockText() {
  tm v;
  if (!localTime(time(nullptr), v)) return "--:--";
  char b[6];
  snprintf(b, sizeof b, "%02d:%02d", v.tm_hour, v.tm_min);
  return b;
}
String timeText(uint32_t at) {
  tm v, today;
  if (!localTime(at, v)) return "";
  char b[8];
  if (localTime(time(nullptr), today) && (v.tm_yday != today.tm_yday || v.tm_year != today.tm_year))
    snprintf(b, sizeof b, "%02d.%02d", v.tm_mday, v.tm_mon + 1);
  else
    snprintf(b, sizeof b, "%02d:%02d", v.tm_hour, v.tm_min);
  return b;
}
String dateText() {
  tm v;
  if (!localTime(time(nullptr), v)) return t("Clock not set");
  const char* days[] = {tr("Sunday"),   tr("Monday"), tr("Tuesday"), tr("Wednesday"),
                        tr("Thursday"), tr("Friday"), tr("Saturday")};
  // Month names as the date below uses them.
  const char* months[] = {tr("January"),   tr("February"), tr("March"),    tr("April"),
                          tr("May"),       tr("June"),     tr("July"),     tr("August"),
                          tr("September"), tr("October"),  tr("November"), tr("December")};
  String s = t("{weekday}, {month} {day}");
  s.replace("{weekday}", days[v.tm_wday]);
  s.replace("{month}", months[v.tm_mon]);
  s.replace("{day}", String(v.tm_mday));
  return s;
}
String ago(uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 60) return t("now");
  if (s < 3600) return String(s / 60) + t(" min");
  if (s < 86400) return String(s / 3600) + t(" h");
  return String(s / 86400) + t(" d");
}
// Wardrive signal: a path hash, SNR in quarter dB and its colour (Wardrive.h).
String hashText(const uint8_t* h, uint8_t size) {
  String s;
  char b[3];
  for (uint8_t i = 0; i < size && i < 3; i++) {
    snprintf(b, 3, "%02X", h[i]);
    s += b;
  }
  return s;
}
String snrText(int8_t q) {
  return String(q / 4.0f, 1) + t(" dB");
}
uint16_t snrColor(int8_t q) {
  return q == -128 ? bad : q >= 20 ? ok : q >= -20 ? warn : rgb(0xf08a4b);
} // quarter dB: 5 dB and over, -5 dB and over, weaker; -128 nobody
String pathText(const Peer& p) {
  if (p.pathLength == 255) return t("path unknown");
  if (!(p.pathLength & 63)) return t("direct");
  return count(p.pathLength & 63, "hop", "hops");
}
String typeText(uint8_t type) {
  switch (type) {
  case 1: return t("Chat node");
  case 2: return t("Repeater");
  case 3: return t("Room server");
  case 4: return t("Sensor");
  }
  return t("Node");
}
uint16_t typeColor(uint8_t type) {
  return type == 2 ? violet : type == 3 ? warn : type == 4 ? info : accent;
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
String distanceText(float m, float bearing) {
  const char* dirs[] = {tr("N"), tr("NE"), tr("E"), tr("SE"), tr("S"), tr("SW"), tr("W"), tr("NW")};
  int k = int((bearing + 22.5f) / 45) % 8;
  return (m < 1000 ? String(int(m / 10) * 10) + t(" m") : String(m / 1000, m < 10000 ? 1 : 0) + t(" km")) + " " +
         dirs[k];
}

// Status bar (HUD): title on the left; radio, GPS, links, clock and battery on the right.
String title() {
  if (locked) return config.name;
  switch (page) {
  case Spectrum: return "Spectrum";
  case Home: return "MeshMesh";
  case Threads: return t("Chats");
  case Chat: return nameOf(recipient);
  case Map: return t("Map");
  case Memo: return t("Contact notes");
  case Nodes: return tagFilter.length() ? t("Nodes") + " · " + tagFilter : t("Nodes");
  case Node: {
    Peer* p = focusedPeer();
    return p ? String(p->name) : t("Node");
  }
  case Sensors: return t("Navigation");
  case Settings: return t("Settings");
  case Radio: return t("Radio");
  case Display: return t("Screen & device");
  case Network: return t("Connections");
  case Diagnostics: return t("Module health");
  case Help: return t("Keys");
  case Library: return t("Saved maps");
  case Scope: return t("Radar: signals");
  case Homing: return t("Homing");
  case Motion: return t("Radar: motion (CSI)");
  case NetList: return t("Internet over Wi-Fi");
  case WardriveView: return t("Wardrive");
  case QuickView: return t("Quick send");
  case Remote: return t("Server and route");
  case ChannelAdd: return t("Add a channel");
  case ChannelInfo: return nameOf(focusChannel);
  case Folder: return appsFolderName(folderOpen);
  }
  return "";
}
void statusBar() {
  auto& d = g();
  d.fillRect(0, 0, 320, 20, bar);
  d.drawFastHLine(0, 20, 320, line);
  int x = 313;
  uint16_t mv = hardware.batteryMv;
  bool external = hardware.usbPower >= 0 ? hardware.usbPower > 0 : mv > 4250;
  unsigned pct = batteryPercent(mv);
  uint16_t level = !mv ? faint : external ? accent : pct <= 15 ? bad : pct <= 30 ? warn : ok;
  d.drawRoundRect(x - 19, 5, 19, 10, 2, dim);
  d.fillRect(x, 8, 2, 4, dim);
  if (external) {
    d.fillTriangle(x - 8, 6, x - 13, 11, x - 9, 11, accent);
    d.fillTriangle(x - 10, 9, x - 6, 9, x - 11, 14, accent);
  } else if (mv)
    d.fillRect(x - 17, 7, max(1u, 15 * pct / 100), 6, level);
  x -= 23;
  if (mv && (config.batteryVolts || !external)) {
    String p = config.batteryVolts ? String(mv / 1000.f, 2) + "V" : String(pct) + "%";
    textRight(x, 14, p, dim, small);
    x -= measure(p, small) + 6;
  }
  String clock = clockText();
  textRight(x, 15, clock, ink, bold);
  x -= measure(clock, bold) + 8;
  bool fresh = meshRadio.lastRxAt && millis() - meshRadio.lastRxAt < 600000;
  bars(x - 15, 15, meshRadio.ready && fresh ? max(1u, snrLevel(meshRadio.lastSnr)) : 0, ok);
  if (!meshRadio.ready) {
    d.drawLine(x - 15, 5, x - 4, 15, bad);
    d.drawLine(x - 4, 5, x - 15, 15, bad);
  }
  x -= 21;
  if (meshRadio.busy()) {
    tri(x + 2, 10, 0, 3, accent);
    x -= 9;
  }
  if (config.gps) {
    icon(IcPin, x - 5, 10, 6, hardware.gpsFix() ? ok : hardware.clockConflict ? bad : warn, bar);
    x -= 15;
  }
  if (portalActive()) {
    icon(IcWifi, x - 6, 10, 7, accent, bar);
    x -= 17;
  } else if (internet.state != Internet::Off && internet.state != Internet::Paused) {
    icon(IcWifi, x - 6, 10, 7, internet.online() ? ok : faint, bar);
    x -= 17;
  }
  if (bleActive()) {
    icon(IcBle, x - 3, 10, 6, info, bar);
    x -= 12;
  }
  unsigned n = page == Threads || page == Chat ? 0 : unreadTotal();
  if (n) {
    String c = String(n);
    textRight(x, 14, c, warn, small);
    x -= measure(c, small) + 2;
    icon(IcMail, x - 7, 10, 7, warn, bar);
    x -= 19;
  }
  String name = title();
  if (page == Chat || page == Node) {
    tri(10, 10, 3, 4, dim);
    text(17, 15, fit(name, x - 22, bold), ink, bold);
  } else
    text(8, 15, fit(name, x - 14, bold), page == Home ? accent : ink, bold);
}
void drawToast(int y) {
  if (!toast.length() || millis() - toastAt >= 3500) return;
  String s = fit(toast, 280);
  int w = measure(s) + 26, x = 160 - w / 2;
  panel(x, y, w, 23, cardHi, 6);
  g().drawRoundRect(x, y, w, 23, 6, line);
  g().fillRoundRect(x + 5, y + 5, 3, 13, 1, toastColor);
  text(x + 15, y + 16, s, ink);
}
void scrollbar(int first, int visible, int total, int y, int h) {
  if (total <= visible) return;
  int th = max(12, h * visible / total), ty = y + (h - th) * first / max(1, total - visible);
  g().fillRoundRect(315, y, 3, h, 1, card);
  g().fillRoundRect(315, ty, 3, th, 1, faint);
}
void listRow(int y, int h, bool focus) {
  panel(8, y, 304, h, focus ? cardHi : bg, 7);
  if (focus) g().fillRoundRect(8, y + 6, 3, h - 12, 1, accent);
}

// Module health: the same eight modules as the diagnostics page.
unsigned moduleStates(bool* state) {
  bool s[] = {meshRadio.ready,        hardware.keyboardOk, hardware.sdOk,
              hardware.fsOk,          hardware.rtcValid,   hardware.gps.passedChecksum() > 0,
              hardware.compassSample, hardware.imuSample};
  unsigned faults = 0;
  for (int i = 0; i < 8; i++) {
    if (state) state[i] = s[i];
    faults += !s[i];
  }
  return faults;
}
// Home and folders share a list or grid, the app order and the current selection.
// The tiles follow config.apps: its order and folders, without the hidden ones (Apps.cpp).
const int tileColumns = 3;
struct AppTile {
  Icon ic;
  uint16_t hue;
  String name, detail;
  unsigned badge;
};
// The tile of every app ID, in uiApps order.
void appTiles(AppTile* out) {
  unsigned unreadCount = unreadTotal(), near = 0, total = meshRadio.peerCount;
  for (unsigned i = 0; i < total; i++)
    if (meshRadio.peers[i].heard && millis() - meshRadio.peers[i].seen < 1800000) near++;
  String links = portalActive() ? "Wi-Fi" : internet.online() ? t("Internet") : "";
  if (bleActive()) links += links.length() ? " + BLE" : "Bluetooth";
  if (!links.length()) links = t("All off");
  unsigned faults = moduleStates(nullptr);
  AppTile tiles[] = {
      {IcChat, accent, t("Chats"),
       unreadCount ? count(unreadCount, "new message", "new messages") : t("No unread messages"), unreadCount},
      {IcSend, info, t("Quick send"), quickSend::targetName(), 0},
      {IcMesh, violet, t("Nodes"), String(near) + t(" of ") + String(total) + t(" nearby"), 0},
      {IcCompass, warn, t("Navigation"),
       hardware.gpsFix() ? "GPS: " + count(hardware.gps.satellites.value(), "sat", "sats")
       : config.gps      ? t("GPS: searching")
                         : t("GPS off"),
       0},
      {IcPin, ok, t("Map"),
       maps.title.length() && maps.available ? maps.title
       : internet.online()                   ? t("Online map")
       : !maps.available                     ? t("No SD card")
                                             : t("No maps yet"),
       0},
      {IcRadar, accent, t("Radar"), "Wi-Fi, BLE, LoRa", 0},
      {IcPulse, info, "Spectrum", "868 MHz band", 0},
      {IcTower, wardrive::running() ? ok : dim, t("Wardrive"),
       wardrive::running() ? count(wardrive::points(), "point", "points") : t("Off"), 0},
      {IcWifi, info, t("Connect"), links, 0},
      {IcPulse, faults ? bad : ok, t("Module health"), faults ? count(faults, "fault", "faults") : t("All OK"), 0},
      {IcGear, dim, t("Settings"), t("Radio, screen"), 0}};
  static_assert(sizeof tiles / sizeof *tiles == sizeof uiApps / sizeof *uiApps, "a tile for every app ID");
  for (unsigned i = 0; i < uiAppCount; i++) out[i] = tiles[i];
}
// A folder's tile: the names of its shown apps and the sum of their badges (new messages).
AppTile folderTile(uint8_t f, const AppTile* tiles) {
  uint8_t in[AppEntriesMax];
  unsigned n = appsShown(in, f);
  AppTile t{IcFolder, warn, appsFolderName(f), String(), 0};
  for (unsigned i = 0; i < n; i++) {
    t.badge += tiles[in[i]].badge;
    if (i < 4) t.detail += String(i ? ", " : "") + tiles[in[i]].name;
  }
  return t;
}
// Two rows of three from y 71 (home and a folder); a folder's tile has a tab, as a folder.
void drawTiles(const uint8_t* order, int shown, const AppTile* tiles) {
  auto& d = g();
  selected = max(0, min(selected, shown - 1));
  int firstRow = max(0, selected / tileColumns - 1);
  for (int i = firstRow * tileColumns; i < shown && i < (firstRow + 2) * tileColumns; i++) {
    bool folder = order[i] & AppFolder;
    AppTile tile = folder ? folderTile(order[i] & ~AppFolder, tiles) : tiles[order[i]];
    int x = 8 + (i % tileColumns) * 104, y = 71 + (i / tileColumns - firstRow) * 74;
    bool focus = selected == i;
    uint16_t surface = focus ? accent : card;
    uint16_t label = focus ? bg : ink;
    if (folder) d.fillRoundRect(x + 6, y - 3, 40, 10, 4, surface);
    panel(x, y, 96, 68, surface, 9);
    target(x, y, 96, min(68, 219 - y), i);
    icon(tile.ic, x + 18, y + 17, 9, focus ? bg : tile.hue, surface);
    if (tile.badge) {
      String count = tile.badge > 99 ? "99+" : String(tile.badge);
      int w = std::max(16, measure(count, small) + 8);
      d.fillRoundRect(x + 88 - w, y + 7, w, 15, 7, focus ? bg : line);
      textCenter(x + 88 - w / 2, y + 18, count, focus ? accent : ink, small);
    }
    String rows[2];
    if (measure(tile.name, bold) > 82 && wrap(tile.name, rows, 2, 82, bold) == 2) {
      text(x + 8, y + 38, fit(rows[0], 82, bold), label, bold);
      text(x + 8, y + 52, fit(rows[1], 82, bold), label, bold);
      text(x + 8, y + 64, fit(tile.detail, 82, small), focus ? cardHi : dim, small);
    } else {
      text(x + 8, y + 44, tile.name, label, bold);
      text(x + 8, y + 60, fit(tile.detail, 82, small), focus ? cardHi : dim, small);
    }
  }
  scrollbar(firstRow, 2, (shown + tileColumns - 1) / tileColumns, 71, 142);
}
void drawLauncher(const uint8_t* order, int shown, const AppTile* tiles) {
  if (config.launcherTiles) {
    drawTiles(order, shown, tiles);
    return;
  }
  constexpr int visible = 6, top = 69, rowHeight = 25;
  selected = constrain(selected, 0, max(0, shown - 1));
  int first = max(0, selected - visible + 1);
  for (int i = first; i < shown && i < first + visible; i++) {
    const bool folder = (order[i] & AppFolder) != 0, focus = i == selected;
    AppTile app = folder ? folderTile(order[i] & ~AppFolder, tiles) : tiles[order[i]];
    int y = top + (i - first) * rowHeight;
    uint16_t surface = focus ? accent : bg, label = focus ? bg : ink;
    if (focus) panel(8, y, 304, rowHeight - 1, surface, 6);
    target(8, y, 304, rowHeight, i);
    icon(app.ic, 23, y + 12, 7, focus ? bg : app.hue, surface);
    int right = 303;
    if (app.ic != IcChat && app.badge) {
      String badge = app.badge > 99 ? "99+" : String(app.badge);
      int w = max(16, measure(badge, small) + 8);
      panel(right - w, y + 4, w, 16, focus ? bg : cardHi, 8);
      textCenter(right - w / 2, y + 16, badge, accent, small);
      right -= w + 7;
    }
    // Reserve room for the name first. Longer localized status strings
    // take whatever space is left, including on narrow folder rows.
    String name = fit(app.name, 156, bold);
    text(40, y + 17, name, label, bold);
    textRight(right, y + 16, fit(app.detail, right - 40 - measure(name, bold) - 12, small), focus ? cardHi : dim,
              small);
  }
  scrollbar(first, visible, shown, top, visible * rowHeight);
}
void drawHome() {
  text(10, 43, fit(String(config.name), 215, big), ink, big);
  text(10, 60,
       fit(meshRadio.ready ? String(config.frequency, 3) + t(" MHz · SF") + String(config.sf) + " · BW " +
                                 String(config.bandwidth, 1) + " · CR 4/" + String(config.cr)
                           : t("Radio error ") + String(meshRadio.radioError),
           240, small),
       meshRadio.ready ? dim : bad, small);
  textRight(308, 42, "TX  " + String(meshRadio.txCount), dim, small);
  textRight(308, 58, "RX  " + String(meshRadio.rxCount), dim, small);
  AppTile tiles[sizeof uiApps / sizeof *uiApps];
  appTiles(tiles);
  uint8_t order[AppEntriesMax];
  drawLauncher(order, appsShown(order), tiles);
  footer({{"OK", t("Open")}, {"MSG", t("Chats")}, {"MAP", t("Map")}, {"MENU", t("Radio controls")}});
}
// Folders use the same layout preference as Home.
void drawFolder() {
  AppTile tiles[sizeof uiApps / sizeof *uiApps];
  appTiles(tiles);
  uint8_t order[AppEntriesMax];
  int shown = appsShown(order, folderOpen);
  unsigned badge = 0;
  for (int i = 0; i < shown; i++) badge += tiles[order[i]].badge;
  panel(8, 26, 304, 38, card, 8);
  icon(IcFolder, 26, 44, 9, warn, card);
  text(44, 41, fit(count(shown, "app", "apps") + (badge ? " · " + String(badge) + t(" waiting") : String()), 258, bold),
       ink, bold);
  text(44, 56, fit(t("Folders and order: web page or app"), 258, small), dim, small);
  if (!shown) {
    textCenter(160, 130, t("No apps in this folder"), dim);
    footer({{"BACK", t("Menu")}});
    return;
  }
  drawLauncher(order, shown, tiles);
  footer({{"OK", t("Open")}, {config.launcherTiles ? "<>" : "^v", t("Select")}, {"BACK", t("Menu")}});
}
// Opens an app of the menu (an index into uiApps) from Home or from a folder.
void openApp(uint8_t app, int folder) {
  Page pages[] = {Threads,  QuickView,    Nodes,   Sensors,     Map,     Scope,
                  Spectrum, WardriveView, Network, Diagnostics, Settings}; // as uiApps
  static_assert(sizeof pages / sizeof *pages == sizeof uiApps / sizeof *uiApps, "a page for every app ID");
  if (app >= uiAppCount) return;
  if (pages[app] == QuickView) {
    quickReplyTo = 0;
    quickTargets = false;
  }
  change(pages[app]);
  appFolder = folder;
}
bool isRoom(uint64_t id) {
  Peer* p = id && !channels::isChannel(id) ? peerOf(id) : nullptr;
  return p && p->type == 3;
} // a MeshCore room server
String bubbleText(const ChatMessage& m);
String chatLeftAction();
void drawThreads() {
  threads();
  int first = max(0, selected - 3);
  for (int i = first; i < int(conversationCount) && i < first + 4; i++) {
    uint64_t id = conversations[i];
    int y = 24 + (i - first) * 48;
    bool focus = selected == i;
    listRow(y, 45, focus);
    target(8, y, 304, 45, i);
    Peer* p = id ? peerOf(id) : nullptr;
    avatar(32, y + 22, 15, id, nameOf(id), p ? p->type : -1);
    const channels::Channel* c = meshRadio.channel(id);
    const ChatMessage* last = nullptr;
    for (int j = meshRadio.historyCount - 1; j >= 0; j--)
      if (belongs(meshRadio.history[j], id)) {
        last = &meshRadio.history[j];
        break;
      }
    String when = last ? timeText(last->timestamp) : "";
    if (when.length()) textRight(306, 19 + y, when, dim, small);
    text(56, y + 18, fit(nameOf(id), 240 - measure(when, small), bold), ink, bold);
    String preview = last ? (last->outgoing    ? t("You: ")
                             : c || isRoom(id) ? String(last->name) + ": "
                                               : String()) +
                                bubbleText(*last)
                     : !c                            ? t("No messages yet")
                     : channels::isPublic(c->secret) ? t("Open MeshCore channel")
                     : channels::isHashtag(*c)       ? t("Open channel · no messages yet")
                                                     : t("Private channel · no messages yet");
    preview.replace("\n", " ");
    unsigned n = unread(id);
    int pillW = 0;
    if (n) {
      String b = String(n);
      pillW = max(18, measure(b, small) + 10);
      g().fillRoundRect(306 - pillW, y + 26, pillW, 14, 7, accent);
      textCenter(306 - pillW / 2, y + 36, b, bg, small);
    }
    text(56, y + 36, fit(preview, 244 - pillW), n ? ink : dim);
  }
  scrollbar(first, 4, conversationCount, 24, 190);
  footer({{"OK", t("Open")}, {"MENU", t("Actions")}, {"U", t("Next unread")}});
}
void statusMark(int x, int y, const ChatMessage& m) {
  auto& d = g();
  switch (m.status) {
  case ChatMessage::Queued:
    d.drawCircle(x + 3, y - 3, 3, dim);
    d.drawFastVLine(x + 3, y - 5, 3, dim);
    d.drawFastHLine(x + 3, y - 3, 2, dim);
    break;
  case ChatMessage::Sent:
    d.drawLine(x, y - 3, x + 2, y - 1, dim);
    d.drawLine(x + 2, y - 1, x + 6, y - 6, dim);
    break;
  case ChatMessage::Delivered:
    for (int k : {0, 4}) {
      d.drawLine(x + k, y - 3, x + k + 2, y - 1, accent);
      d.drawLine(x + k + 2, y - 1, x + k + 6, y - 6, accent);
    }
    break;
  case ChatMessage::Failed:
    d.fillCircle(x + 3, y - 3, 4, bad);
    d.drawFastVLine(x + 3, y - 5, 3, ink);
    d.drawPixel(x + 3, y - 1, ink);
    break;
  default: break;
  }
}
void drawChat() {
  auto& d = g();
  Peer* p = peerOf(recipient);
  const channels::Channel* ch = meshRadio.channel(recipient);
  String sub = channels::isChannel(recipient) ? (ch && !channels::isPublic(ch->secret) && !channels::isHashtag(*ch)
                                                     ? t("Private channel · senders unverified · no ACK")
                                                     : t("Open channel · senders unverified · no ACK"))
               : p && p->type == 3            ? typeText(p->type) + " · " +
                                         (remote::find(p->id) && remote::find(p->id)->login == remote::Done
                                              ? t("logged in")
                                              : t("log in: node card > Server"))
               : p ? typeText(p->type) + " · " + pathText(*p) + (p->heard ? " · " + ago(millis() - p->seen) : String())
                   : t("Not in contacts");
  text(8, 33, fit(sub, 270, small), dim, small);
  unsigned used = composer.length(), limit = meshRadio.messageLimit(recipient);
  String counter = String(used) + "/" + String(limit);
  const int leading = 17;
  String rows[8];
  unsigned inputRows = max(1u, wrap(composer, rows, 8, 278 - measure(counter, small)));
  unsigned shown = min(inputRows, 3u);
  int boxH = shown * leading + 12, boxY = 216 - boxH;
  int indices[64], total = 0;
  for (unsigned i = 0; i < meshRadio.historyCount; i++)
    if (belongs(meshRadio.history[i], recipient)) indices[total++] = i;
  chatOffset = min(chatOffset, max(0, total - 1));
  int end = total - chatOffset, bottom = boxY - 5;
  const int top = 41;
  for (int j = end - 1; j >= 0 && bottom > top + 20; j--) {
    auto& m = meshRadio.history[indices[j]];
    String lines[7];
    unsigned n = wrap(bubbleText(m), lines, 7, 214);
    bool named = !m.outgoing && (channels::isChannel(recipient) || isRoom(recipient)); // a room post names its author
    String when = timeText(m.timestamp),
           state = m.status == ChatMessage::Failed ? t("not confirmed") : meshRadio.routeText(m);
    if (m.protocol == 1) state = "MM/1 " + state;
    int metaW = measure(when, small) + (state.length() ? measure(state, small) + 4 : 0) + (m.outgoing ? 12 : 0),
        w = metaW;
    for (unsigned k = 0; k < n; k++) w = max(w, measure(lines[k]));
    if (named) w = max(w, measure(m.name, small));
    w = max(44, w + 16);
    int h = 9 + (named ? 14 : 0) + n * leading + 13;
    if (bottom - h < top) {
      if (j != end - 1) break;
      n = max(1, (bottom - top - 9 - (named ? 14 : 0) - 13) / leading);
      h = 9 + (named ? 14 : 0) + n * leading + 13;
    }
    int x = m.outgoing ? 312 - w : 8, y = bottom - h, ty = y + 17;
    panel(x, y, w, h, m.outgoing ? outBubble : inBubble, 9);
    if (named) {
      text(x + 8, y + 14, fit(m.name, w - 16, small), info, small);
      ty += 14;
    }
    for (unsigned k = 0; k < n; k++) text(x + 8, ty + k * leading, lines[k]);
    int mx = x + w - 7 - (m.outgoing ? 11 : 0), my = y + h - 4;
    textRight(mx, my, when, dim, small);
    if (state.length())
      textRight(mx - measure(when, small) - 4, my, state, m.status == ChatMessage::Failed ? bad : faint, small);
    if (m.outgoing) statusMark(x + w - 15, my, m);
    bottom = y - 5;
  }
  if (!total) {
    icon(ch && !channels::isPublic(ch->secret) && !channels::isHashtag(*ch) ? IcLock
         : channels::isChannel(recipient)                                   ? IcHash
                                                                            : IcChat,
         160, 86, 14, faint);
    textCenter(160, 124, t("No messages yet"), dim);
    textCenter(160, 142, t("Type and press OK to send"), faint, small);
  }
  if (chatOffset) {
    String c = String(chatOffset);
    int w = measure(c, small) + 18;
    d.fillRoundRect(304 - w, boxY - 19, w, 14, 7, accent);
    tri(304 - w + 7, boxY - 12, 2, 3, bg);
    text(304 - w + 12, boxY - 8, c, bg, small);
  }
  panel(8, boxY, 304, boxH, cardHi, 8);
  if (composer.length()) {
    unsigned firstRow = inputRows - shown;
    int cx = 17;
    for (unsigned i = firstRow; i < inputRows; i++) cx = text(17, boxY + 19 + (i - firstRow) * leading, rows[i], ink);
    d.fillRect(cx + 1, boxY + 7 + (shown - 1) * leading, 1, 13, accent);
  } else {
    d.fillRect(16, boxY + 7, 2, 14, accent);
    text(24, boxY + 19, t("Message..."), dim);
  }
  textRight(305, boxY + boxH - 9, counter, used * 10 >= limit * 9 ? (used >= limit ? bad : warn) : dim, small);
  String left = chatLeftAction();
  if (left.length())
    footer({{"OK", t("Send")}, {"<", left}, {t("hold OK").c_str(), t("Clipboard")}, {"MENU", t("Actions")}});
  else
    footer({{"OK", t("Send")}, {t("hold OK").c_str(), t("Clipboard")}, {"MENU", t("Actions")}});
}
// Map: north-up tiles with HUD chips, own position, node markers and a scale bar.
void drawMap() {
  auto& d = g();
  maps.draw(0, 21, 320, 199);
  bool online = internet.online();
  if (!maps.haveCenter || (!maps.available && !online)) {
    panel(30, 70, 260, 86, card, 10);
    icon(IcPin, 160, 96, 12, faint, card);
    textCenter(160, 128,
               online            ? t("Finding location by IP...")
               : !maps.available ? t("Needs Wi-Fi internet or an SD card")
                                 : t("Connect Wi-Fi or load a map"),
               ink);
    textCenter(160, 144, online ? t("GPS will take over after a fix") : t("Connections > Internet over Wi-Fi"), dim,
               small);
  } else {
    double scale = 256.0 * (1UL << maps.zoom);
    auto yy = [&](double l) {
      double r = l * M_PI / 180;
      return (1 - log(tan(r) + 1 / cos(r)) / M_PI) / 2 * scale;
    };
    auto point = [&](double lat, double lon, int& x, int& y) {
      if (!isfinite(lat) || !isfinite(lon) || fabs(lat) > 85.05112878) return false;
      x = 160 + int((lon - maps.longitude) / 360 * scale);
      y = 120 + int(yy(lat) - yy(maps.latitude));
      return x >= 6 && x < 314 && y >= 26 && y < 216;
    };
    {
      const wardrive::Recent* pts;
      unsigned n = wardrive::recent(pts);
      for (unsigned i = 0; i < n; i++) {
        int x, y;
        if (!point(pts[i].lat / 1e6, pts[i].lon / 1e6, x, y)) continue; // the wardrive log: pings larger
        if (pts[i].kind == wardrive::Ping) {
          d.fillCircle(x, y, 5, bar);
          d.fillCircle(x, y, 3, snrColor(pts[i].snr));
        } else
          d.fillCircle(x, y, 2, snrColor(pts[i].snr));
      }
    }
    for (unsigned i = 0; i < meshRadio.peerCount; i++) {
      auto& p = meshRadio.peers[i];
      int x, y;
      if (!p.position || !point(p.latitude, p.longitude, x, y)) continue;
      d.fillCircle(x, y, 6, bar);
      d.fillCircle(x, y, 4, typeColor(p.type));
      String name = fit(p.name, 80, small);
      int w = measure(name, small) + 6, lx = constrain(x + 8, 2, 316 - w);
      d.fillRoundRect(lx, y - 6, w, 11, 3, bar);
      text(lx + 3, y + 2, name, ink, small);
    }
    if (hardware.gpsFix()) {
      int x, y;
      if (point(hardware.gps.location.lat(), hardware.gps.location.lng(), x, y)) {
        if (navigation.headingValid) {
          float a = navigation.heading * M_PI / 180;
          d.fillTriangle(x + roundf(18 * sinf(a)), y - roundf(18 * cosf(a)), x + roundf(8 * sinf(a - 0.6f)),
                         y - roundf(8 * cosf(a - 0.6f)), x + roundf(8 * sinf(a + 0.6f)), y - roundf(8 * cosf(a + 0.6f)),
                         info);
        }
        d.fillCircle(x, y, 7, ink);
        d.fillCircle(x, y, 5, info);
      }
    }
    if (!maps.follow) {
      d.fillRect(151, 119, 19, 3, bar);
      d.fillRect(159, 111, 3, 19, bar);
      d.drawFastHLine(152, 120, 17, ink);
      d.drawFastVLine(160, 112, 17, ink);
      d.drawCircle(160, 120, 5, bar);
    }
    if (!maps.visibleTiles) {
      panel(30, 96, 260, 44, card, 8);
      textCenter(160, 114,
                 maps.fetching  ? t("Downloading map...")
                 : maps.waiting ? t("Reading map from SD...")
                 : online       ? t("Tiles unavailable")
                                : t("This zoom is not downloaded"),
                 ink);
      if (!maps.waiting)
        textCenter(160, 130, online ? fit(netError(), 240, small) : t("+/- zoom · Wi-Fi internet loads maps"), dim,
                   small);
    }
    double metresPerPx = cos(maps.latitude * M_PI / 180) * 40075016.686 / scale;
    int nice[] = {5,    10,    20,    50,    100,    200,    500,    1000,    2000,
                  5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000},
        dist = nice[0];
    for (int v : nice)
      if (v / metresPerPx <= 90) dist = v;
    int px = max(8, int(dist / metresPerPx));
    String scaleText = dist >= 1000 ? String(dist / 1000) + t(" km") : String(dist) + t(" m");
    int sw = max(px, measure(scaleText, small)) + 12;
    d.fillRoundRect(312 - sw, 196, sw, 19, 4, bar);
    d.fillRect(306 - px, 209, px, 2, ink);
    d.drawFastVLine(306 - px, 206, 5, ink);
    d.drawFastVLine(305, 206, 5, ink);
    textRight(306, 204, scaleText, ink, small);
    d.fillRoundRect(4, 203, 82, 12, 3, bar);
    text(8, 212, "© OpenStreetMap", dim, small);
  }
  String area = fit(maps.available && maps.inArea() ? maps.title
                    : online                        ? "OpenStreetMap"
                                                    : t("Offline map"),
                    120, small) +
                "  z" + String(maps.zoom) + (maps.fetching ? t(" · loading") : "");
  int aw = measure(area, small) + 22;
  d.fillRoundRect(6, 26, aw, 16, 4, bar);
  icon(IcPin, 14, 34, 5, ok, bar);
  text(22, 37, area, ink, small);
  bool fix = hardware.gpsFix();
  String gps = !config.gps              ? t("GPS off")
               : fix                    ? "GPS " + String(hardware.gps.satellites.value())
               : hardware.clockConflict ? t("GPS: old date")
                                        : t("GPS: searching");
  int gw = measure(gps, small) + 10;
  d.fillRoundRect(6, 45, gw, 15, 4, bar);
  text(11, 56, gps, !config.gps ? dim : fix ? ok : warn, small);
  if (navigation.headingValid) {
    int cx = 298, cy = 42;
    d.fillCircle(cx, cy, 15, bar);
    d.drawCircle(cx, cy, 15, line);
    float a = navigation.heading * M_PI / 180;
    d.fillTriangle(cx + roundf(11 * sinf(a)), cy - roundf(11 * cosf(a)), cx + roundf(4 * cosf(a)),
                   cy + roundf(4 * sinf(a)), cx - roundf(4 * cosf(a)), cy - roundf(4 * sinf(a)), bad);
    d.fillTriangle(cx - roundf(11 * sinf(a)), cy + roundf(11 * cosf(a)), cx + roundf(4 * cosf(a)),
                   cy + roundf(4 * sinf(a)), cx - roundf(4 * cosf(a)), cy - roundf(4 * sinf(a)), dim);
    String deg = String(int(navigation.heading + .5f) % 360) + "°";
    d.fillRoundRect(cx - 15, 60, 30, 12, 3, bar);
    textCenter(cx, 69, deg, ink, small);
  }
  footer({{"<>", t("Pan")},
          {"+/-", t("Zoom")},
          {"OK", maps.follow ? t("Following") : t("To GPS")},
          {"MENU", t("Actions")}});
}
void drawLibrary() {
  unsigned total = library.size();
  if (!total) {
    icon(IcPin, 160, 90, 14, faint);
    textCenter(160, 128, t("No saved maps"), ink);
    textCenter(160, 146, t("Upload a map via Wi-Fi or USB"), dim, small);
  }
  int first = max(0, selected - 3);
  for (unsigned i = first; i < total && i < unsigned(first + 4); i++) {
    int y = 24 + (i - first) * 48;
    bool focus = selected == int(i);
    listRow(y, 45, focus);
    target(8, y, 304, 45, i);
    g().fillRoundRect(18, y + 8, 30, 30, 7, card);
    icon(IcPin, 33, y + 22, 9, ok, card);
    String name = library[i]["name"].as<String>();
    text(58, y + 19, fit(name, 200, bold), ink, bold);
    text(58, y + 36, count(library[i]["tiles"].as<unsigned>(), "tile on SD", "tiles on SD"), dim, small);
    if (name == maps.title) {
      g().drawLine(284, y + 22, 288, y + 26, accent);
      g().drawLine(288, y + 26, 296, y + 17, accent);
      g().drawLine(284, y + 23, 288, y + 27, accent);
      g().drawLine(288, y + 27, 296, y + 18, accent);
    }
  }
  scrollbar(first, 4, total, 24, 190);
  footer({{"OK", t("Open")}, {"^v", t("Select")}, {"BACK", t("Map")}});
}
void drawNodes() {
  sortNodes();
  if (!nodeTotal) {
    icon(IcMesh, 160, 86, 16, faint);
    textCenter(160, 126, tagFilter.length() ? t("No matching contacts") : t("No nodes discovered yet"), ink);
    textCenter(160, 144, tagFilter.length() ? t("F: edit tag filter") : t("Advertise my node"), dim, small);
  }
  int first = max(0, selected - 3);
  uint32_t now = millis();
  for (unsigned i = first; i < nodeTotal && i < unsigned(first + 4); i++) {
    auto& p = meshRadio.peers[nodeOrder[i]];
    int y = 24 + (i - first) * 48;
    bool focus = selected == int(i);
    listRow(y, 45, focus);
    target(8, y, 304, 45, i);
    avatar(32, y + 22, 15, p.id, p.name, p.type);
    String age = p.heard ? ago(now - p.seen) : t("saved");
    textRight(306, y + 18, age, p.heard && now - p.seen < 1800000 ? ok : dim, small);
    if (p.heard) bars(286 - measure(age, small), y + 18, snrLevel(p.snr), ok);
    text(56, y + 18, fit(p.name, 200 - measure(age, small), bold), ink, bold);
    float metres, bearing;
    String detail = typeText(p.type) + " · " + pathText(p);
    if (p.heard) detail += " · " + String(int(p.rssi)) + " dBm";
    if (distanceTo(p, metres, bearing))
      detail += " · " + distanceText(metres, bearing);
    else if (p.position)
      detail += " · GPS";
    if (contactMemo(p).memo.tags[0]) detail = String(contactMemo(p).memo.tags) + " · " + detail;
    text(56, y + 36, fit(detail, 250, small), typeColor(p.type) == accent ? dim : typeColor(p.type), small);
  }
  scrollbar(first, 4, nodeTotal, 24, 190);
  footer({{"OK", t("Details")}, {"MENU", t("Actions")}, {"F", t("Filter")}, {"BACK", t("Back")}});
}
enum NodeAction { ActWrite, ActMap, ActResetPath, ActForget, ActServer, ActTrace };
// A room's posts open as its chat; a repeater or room has its server page, a chat node behind repeaters a trace.
unsigned nodeActions(const Peer& p, NodeAction* out) {
  unsigned n = 0;
  if (p.type == 1 || p.type == 3) out[n++] = ActWrite;
  if (p.type == 2 || p.type == 3)
    out[n++] = ActServer;
  else if (remote::traceable(p.id))
    out[n++] = ActTrace;
  if (p.position) out[n++] = ActMap;
  if (p.pathLength != 255) out[n++] = ActResetPath;
  out[n++] = ActForget;
  return n;
}
void drawNode() {
  Peer* p = focusedPeer();
  if (!p) {
    textCenter(160, 110, t("Node was removed"), dim);
    footer({{"BACK", t("Nodes")}});
    return;
  }
  auto& d = g();
  avatar(36, 50, 20, p->id, p->name, p->type);
  text(66, 48, fit(p->name, 240, big), ink, big);
  text(66, 64, typeText(p->type), typeColor(p->type), small);
  float metres, bearing;
  String rows[][2] = {
      {t("Last heard"),
       p->heard ? (millis() - p->seen < 60000 ? ago(0) : ago(millis() - p->seen) + t(" ago")) : t("not since boot")},
      {t("Signal"), p->heard ? String(int(p->rssi)) + " dBm · SNR " + String(p->snr, 1) : String("-")},
      {t("Route"), pathText(*p) + (p->pathLength == 255 ? t(" · flood") : String())},
      {t("Position"), distanceTo(*p, metres, bearing) ? distanceText(metres, bearing)
                      : p->position                   ? String(p->latitude, 5) + ", " + String(p->longitude, 5)
                                                      : t("not shared")},
      {"ID", meshRadio.idText(p->id)}};
  NodeAction acts[6];
  unsigned n = nodeActions(*p, acts);
  action = constrain(action, 0, int(n) - 1);
  bool two = n > 4; // five or six actions: two rows of buttons
  for (int i = 0; i < 5; i++) {
    int y = two ? 86 + i * 16 : 90 + i * 18;
    text(14, y, rows[i][0], dim);
    textRight(306, y, fit(rows[i][1], 180), ink);
    if (i < 4) d.drawFastHLine(14, y + (two ? 4 : 5), 292, card);
  }
  const String names[] = {p->type == 3 ? t("Room") : t("Message"), t("On map"), t("Reset path"),
                          deleteArmed ? t("Sure?") : t("Forget"),  t("Server"), t("Trace")};
  unsigned per = two ? (n + 1) / 2 : n;
  for (unsigned i = 0; i < n; i++) {
    unsigned row = i / per, k = i % per, inRow = min(per, n - row * per);
    int w = (304 - (inRow - 1) * 6) / inRow, x = 8 + k * (w + 6), h = two ? 25 : 28, y = two ? 160 + row * 29 : 184;
    bool focus = action == int(i), danger = acts[i] == ActForget;
    target(x, y, w, h, i);
    panel(x, y, w, h, focus ? (danger && deleteArmed ? bad : cardHi) : card, 7);
    if (focus) ring(x, y, w, h, 7, danger ? bad : accent);
    textCenter(x + w / 2, y + h / 2 + 5, fit(names[acts[i]], w - 8), danger ? (focus ? ink : bad) : ink);
  }
  footer({{"<>", t("Action")}, {"OK", t("Run")}, {"N", t("Notes")}, {"BACK", t("Nodes")}});
}
void compassRose(int cx, int cy, int r) {
  auto& d = g();
  d.fillCircle(cx, cy, r, card);
  d.drawCircle(cx, cy, r, line);
  d.drawCircle(cx, cy, r - 1, line);
  for (int a = 0; a < 360; a += 30) {
    float rad = a * M_PI / 180;
    int l = a % 90 ? 4 : 7;
    d.drawLine(cx + roundf((r - 2) * sinf(rad)), cy - roundf((r - 2) * cosf(rad)), cx + roundf((r - 2 - l) * sinf(rad)),
               cy - roundf((r - 2 - l) * cosf(rad)), a % 90 ? faint : dim);
  }
  const char* dirs[] = {tr("N"), tr("E"), tr("S"), tr("W")};
  for (int k = 0; k < 4; k++) {
    float rad = k * M_PI / 2;
    textCenter(cx + roundf((r - 16) * sinf(rad)), cy - roundf((r - 16) * cosf(rad)) + 4, dirs[k], k ? dim : bad, bold);
  }
  if (navigation.headingValid) {
    float a = navigation.heading * M_PI / 180;
    int tx = cx + roundf((r - 8) * sinf(a)), ty = cy - roundf((r - 8) * cosf(a));
    d.fillTriangle(tx, ty, cx + roundf(5 * cosf(a)), cy + roundf(5 * sinf(a)), cx - roundf(5 * cosf(a)),
                   cy - roundf(5 * sinf(a)), accent);
    d.fillCircle(cx, cy, 3, ink);
  } else
    textCenter(cx, cy + 4, "--", faint, bold);
}
void drawSensors() {
  compassRose(62, 84, 54);
  String heading = navigation.headingValid ? String(int(navigation.heading + .5f) % 360) + "°"
                   : navigation.calibrated ? t("hold flat")
                                           : t("not calibrated");
  textCenter(62, 153, heading, navigation.headingValid ? ink : warn, navigation.headingValid ? bold : small);
  bool fix = hardware.gpsFix();
  int y = 38;
  auto row = [&](const String& name, const String& value, uint16_t color) {
    text(130, y, name, dim, small);
    textRight(306, y, fit(value, 120), color);
    y += 19;
  };
  // The GPS switch is the first item: the receiver is turned on and off where its position is used.
  {
    bool focus = selected == 0;
    target(122, 24, 192, 22, 0);
    panel(122, 24, 192, 22, focus ? cardHi : card, 6);
    if (focus) ring(122, 24, 192, 22, 6);
    text(130, y, "GPS", dim, small);
    g().fillRoundRect(280, 28, 26, 14, 7, config.gps ? ok : line);
    g().fillCircle(config.gps ? 298 : 287, 35, 5, ink);
    textRight(274, y,
              fit(!config.gps              ? t("off")
                  : fix                    ? t("fix")
                  : hardware.clockConflict ? t("old date")
                                           : t("searching"),
                  136),
              !config.gps ? dim
              : fix       ? ok
                          : warn);
    y += 19;
  }
  unsigned view = config.gps ? hardware.gpsInView() : 0;
  row(t("Satellites"),
      String(hardware.gps.satellites.value()) + (view ? t(" of ") + String(view) + t(" in view") : String()), ink);
  row(t("Latitude"), fix ? String(hardware.gps.location.lat(), 5) : String("-"), ink);
  row(t("Longitude"), fix ? String(hardware.gps.location.lng(), 5) : String("-"), ink);
  row(t("Clock"),
      (hardware.clockSource == "unset"    ? t("not set")
       : hardware.clockSource == "manual" ? t("manual")
                                          : hardware.clockSource) +
          (hardware.clockTrusted ? t(" · trusted") : String()),
      ink);
  row(t("Compass"),
      navigation.calibrating  ? count(navigation.samples, "sample", "samples")
      : navigation.calibrated ? t("calibrated")
                              : t("needs calibration"),
      navigation.calibrating ? warn : ink);
  String actions[] = {t("Share position"), navigation.calibrating ? t("Finish calibration") : t("Calibrate compass")};
  String hints[] = {fix ? t("Advert with GPS") : t("Needs a fresh GPS fix"),
                    navigation.calibrating ? t("Rotate in all directions") : t("Rotate for 20+ seconds")};
  for (int i = 0; i < 2; i++) {
    int x = 8 + i * 154;
    bool focus = selected == i + 1;
    target(x, 168, 150, 46, i + 1);
    panel(x, 168, 150, 46, focus ? cardHi : card, 7);
    if (focus) ring(x, 168, 150, 46, 7);
    text(x + 10, 186, fit(actions[i], 132, bold), ink, bold);
    text(x + 10, 203, fit(hints[i], 132, small), dim, small);
  }
  footer({{"<>", t("Select")},
          {"OK", selected     ? t("Run")
                 : config.gps ? t("GPS off")
                              : t("GPS on")},
          {"BACK", t("Back")}});
}
// Signal radar: Wi-Fi access points, Bluetooth devices and directly heard LoRa nodes. Nearer the centre means a
// stronger signal; the angle only keeps targets apart, because RSSI carries no bearing.
bool radarSound = true;
uint32_t pingAt = 0, pingedSamples = 0;
int liveScopeIndex() {
  if (scopeRow >= scopeCount) return -1;
  const RadarTarget& selectedTarget = scopeRows[scopeRow];
  for (unsigned i = 0; i < radar.count; i++) {
    if (radar.targets[i].kind == selectedTarget.kind && radar.targets[i].id == selectedTarget.id) return int(i);
  }
  return -1;
}
float signalLevel(int rssi) {
  return constrain((rssi + 100) / 65.f, 0.f, 1.f);
} // -100 .. -35 dBm
uint16_t mix(uint32_t a, uint32_t b, float f) {
  auto ch = [&](int s) { return int(((a >> s) & 255) * (1 - f) + ((b >> s) & 255) * f); };
  return rgb(ch(16) << 16 | ch(8) << 8 | ch(0));
}
uint32_t targetHue(const RadarTarget& r) {
  return r.kind == RadarTarget::Lora  ? 0xa78bfa
         : r.kind == RadarTarget::Ble ? (r.device ? 0xf472b6 : 0x8d99a6)
         : r.open                     ? 0xe9b13b
                                      : 0x4f9df7;
}
String deviceText(const RadarTarget& r) {
  switch (r.device) {
  case RadarTarget::Phone: return t("Phone");
  case RadarTarget::Watch: return t("Watch");
  case RadarTarget::Audio: return t("Headphones");
  case RadarTarget::Personal: return t("Phone/watch");
  default: return t("BLE device");
  }
}
String vendorText(uint16_t v) {
  switch (v) {
  case 0x004c: return "Apple";
  case 0x0075: return "Samsung";
  case 0x00e0: return "Google";
  case 0x027d: return "Huawei";
  case 0x038f: return "Xiaomi";
  case 0x0087: return "Garmin";
  case 0x0006: return "Microsoft";
  }
  return "";
}
String targetName(const RadarTarget& r) {
  if (r.name[0]) return r.name;
  if (r.kind == RadarTarget::Ble) {
    String v = vendorText(r.vendor);
    return deviceText(r) + (v.length() ? " " + v : String());
  }
  return r.kind == RadarTarget::Wifi ? t("Hidden network") : meshRadio.idText(r.id);
}
String macText(uint64_t id) {
  char b[18];
  snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", unsigned(id >> 40 & 255), unsigned(id >> 32 & 255),
           unsigned(id >> 24 & 255), unsigned(id >> 16 & 255), unsigned(id >> 8 & 255), unsigned(id & 255));
  return b;
}
bool radarFresh() {
  return radar.fresh();
}
String bleState() {
  switch (radar.ble) {
  case Radar::BleBusy: return t("Bluetooth busy: probe");
  case Radar::BleFailed: return t("Bluetooth error");
  default: return "";
  }
}
String wifiState() {
  switch (radar.wifi) {
  case Radar::WifiPortal: return t("Wi-Fi busy: access point");
  case Radar::WifiBusy: return t("Wi-Fi busy: probe");
  case Radar::WifiFailed: return t("Wi-Fi error");
  default: return radar.sweeps ? "" : t("Scanning Wi-Fi...");
  }
}
constexpr int ScopeX = 96, ScopeY = 116, ScopeRadius = 86;
void drawSweep() {
  auto& d = g();
  const int cx = ScopeX, cy = ScopeY, R = ScopeRadius;
  const uint32_t cardRgb = 0x131a22, gridRgb = 0x223040;
  uint32_t now = millis();
  d.fillCircle(cx, cy, R, card);
  for (int k = 1; k <= 4; k++) d.drawCircle(cx, cy, R * k / 4, k == 4 ? line : rgb(gridRgb));
  d.drawFastHLine(cx - R, cy, 2 * R + 1, rgb(gridRgb));
  d.drawFastVLine(cx, cy - R, 2 * R + 1, rgb(gridRgb));
  float sweep = (now % 3000) * 2 * M_PI / 3000; // one turn per 3 s, fading trail behind the beam
  for (int k = 24; k >= 0; k--) {
    float a = sweep - k * .03f;
    d.drawLine(cx, cy, cx + roundf(R * sinf(a)), cy - roundf(R * cosf(a)), mix(0x1fc2ae, cardRgb, .25f + k * .03f));
  }
  int sel = liveScopeIndex();
  for (int i = radar.count - 1; i >= 0; i--) {
    const RadarTarget& r = radar.targets[i];
    uint32_t h = uint32_t(r.id ^ (r.id >> 29)) * 2654435761u;
    float a = (h >> 8) % 360 * M_PI / 180;
    float dist =
        (R - 9) * (.1f + .9f * (1 - signalLevel(r.rssi))); // keep even the selection ring inside the redrawn disk
    int x = cx + roundf(dist * sinf(a)), y = cy - roundf(dist * cosf(a));
    float since = fmodf(sweep - a + 4 * M_PI, 2 * M_PI) / (2 * M_PI); // blips glow as the beam passes
    float faded = r.kind == RadarTarget::Wifi ? 0 : constrain((now - r.seen) / 30000.f, 0.f, .5f); // quiet targets fade
    d.fillCircle(x, y, i == sel ? 4 : 3, mix(targetHue(r), cardRgb, min(.85f, .15f + .6f * since + faded)));
    if (i == sel) {
      d.drawCircle(x, y, 7, ink);
      d.drawCircle(x, y, 8, accent);
    }
  }
  d.fillCircle(cx, cy, 3, ink);
  scopeFrameAt = now;
  ++scopeFrames;
}
void drawScope() {
  if (!scopeHeld) {
    scopeCount = int(radar.count);
    memcpy(scopeRows, radar.targets, scopeCount * sizeof(RadarTarget));
    scopeRow = 0;
  }
  auto& d = g();
  int left = scopeWide ? 6 : 190, right = 314, width = right - left;
  if (!scopeWide) {
    drawSweep();
    textCenter(ScopeX, 216, t("closer to centre = stronger"), faint, small);
  }
  String summary =
      scopeHeld ? t("List paused") + " · " + String(scopeCount ? scopeRow + 1 : 0) + "/" + String(scopeCount)
                : "Wi-Fi " + String(radar.counted(RadarTarget::Wifi)) + "·BLE " +
                      String(radar.counted(RadarTarget::Ble)) + "·LoRa " + String(radar.counted(RadarTarget::Lora));
  text(left + 4, 34, fit(summary, width - 6, small), scopeHeld ? accent : dim, small);
  int first = max(0, scopeRow - 6);
  for (int i = first; i < scopeCount && i < first + 7; i++) {
    const RadarTarget& r = scopeRows[i];
    int y = 40 + (i - first) * 22;
    bool focus = i == scopeRow;
    target(left, y, width, 20, i);
    panel(left, y, width, 20, focus ? cardHi : bg, 5);
    if (focus) d.fillRoundRect(left, y + 4, 3, 12, 1, accent);
    d.fillCircle(left + 9, y + 10, 3, rgb(targetHue(r)));
    text(left + 16, y + 14, fit(targetName(r), width - 48), focus ? ink : dim);
    textRight(right - 4, y + 14, String(int(r.rssi)), focus ? ink : dim, bold);
  }
  String state = wifiState();
  if (!state.length()) state = bleState();
  unsigned people = radar.personal(30000);
  if (!state.length() && people) state = count(people, "phone/watch nearby", "phones/watches nearby");
  uint16_t color =
      radar.wifi == Radar::WifiReady && radar.ble != Radar::BleFailed && radar.ble != Radar::BleBusy ? dim : warn;
  if (!scopeCount)
    text(left + 4, 110, fit(state.length() ? state : t("No signals yet"), width - 8, small), color, small);
  else if (scopeWide)
    text(left + 4, 212, fit(targetName(scopeRows[scopeRow]), width - 8, small), ink, small);
  else if (state.length())
    text(left + 4, 212, fit(state, width - 8, small), color, small);
  footer({{"L", scopeWide ? t("Radar") : t("List")},
          {"P", scopeHeld ? t("Resume") : t("Pause")},
          {"OK", t("Home in")},
          {"<>", t("Motion")}});
}
void drawHoming() {
  auto& d = g();
  const RadarTarget& f = radar.focus;
  uint16_t hue = rgb(targetHue(f));
  bool fresh = radarFresh(), lora = f.kind == RadarTarget::Lora, bt = f.kind == RadarTarget::Ble;
  panel(8, 26, 304, 34, card, 8);
  d.fillRoundRect(14, 31, 24, 24, 6, bg);
  icon(lora ? IcTower : bt ? (f.device ? IcPerson : IcBle) : IcWifi, 26, 43, 7, hue, bg);
  text(46, 40, fit(targetName(f), 258, bold), ink, bold);
  String vendor = vendorText(f.vendor);
  text(46, 54,
       fit(lora ? "LoRa · " + meshRadio.idText(f.id) + t(" · direct packets only")
           : bt ? "Bluetooth · " + deviceText(f) + (vendor.length() ? " · " + vendor : String()) +
                      t(" · address changes over time")
                : "Wi-Fi · " + t("channel ") + String(f.channel) + " · " + macText(f.id),
           258, small),
       dim, small);
  int x = text(14, 112, radar.samples ? String(int(lroundf(radar.fast))) : String("--"), fresh ? ink : faint, digits);
  text(x + 4, 112, "dBm", dim, small);
  // Homing indicator: the fast average against the slow one, as in the RSSI tracker.
  int trend = radar.trend();
  panel(186, 66, 126, 46, card, 8);
  if (f.kind == RadarTarget::Wifi && radar.wifi != Radar::WifiReady)
    text(196, 93, fit(wifiState(), 108, small), warn, small);
  else if (bt && radar.ble != Radar::BleReady)
    text(196, 93, fit(bleState(), 108, small), warn, small);
  else if (!fresh)
    text(196, 93, radar.samples > 1 ? t("Lost") : t("No signal"), warn, bold);
  else {
    uint16_t c = trend > 0 ? ok : trend < 0 ? bad : dim;
    if (trend)
      tri(206, 89, trend > 0 ? 0 : 2, 10, c);
    else {
      d.fillRect(197, 83, 18, 3, c);
      d.fillRect(197, 91, 18, 3, c);
    }
    text(224, 86, trend > 0 ? t("Stronger") : trend < 0 ? t("Weaker") : t("Steady"), c, bold);
    text(224, 101, t("keep moving"), faint, small);
  }
  // Strength meter with the peak marker.
  float level = signalLevel(lroundf(radar.fast));
  d.fillRoundRect(14, 120, 292, 8, 3, line);
  if (radar.samples)
    d.fillRoundRect(14, 120, max(3, int(292 * level)), 8, 3,
                    !fresh        ? faint
                    : level < .3f ? bad
                    : level < .6f ? warn
                                  : ok);
  if (radar.samples) {
    int px = 14 + int(291 * signalLevel(radar.peak));
    d.fillRect(px - 1, 116, 3, 16, ink);
  }
  text(14, 142, "-100", faint, small);
  textRight(306, 142, "-35 dBm", faint, small);
  textCenter(160, 142,
             t("peak ") + (radar.samples
                               ? String(int(lroundf(radar.peak))) + " dBm (" + String(radar.fast - radar.peak, 0) + ")"
                               : String("-")),
             dim, small);
  // History: the last 120 samples, oldest on the left.
  panel(8, 148, 304, 58, card, 6);
  for (int dbm : {-90, -70, -50}) {
    int y = 202 - int(50 * signalLevel(dbm));
    d.drawFastHLine(12, y, 296, rgb(0x223040));
  }
  unsigned n = radar.historyCount();
  int px = 0, py = 0;
  for (unsigned i = 0; i < n; i++) {
    int x = 12 + int(i * 296 / (Radar::HistorySize - 1)), y = 202 - int(50 * signalLevel(radar.sample(i)));
    if (i)
      d.drawLine(px, py, x, y, accent);
    else
      d.drawPixel(x, y, accent);
    px = x;
    py = y;
  }
  if (!n) textCenter(160, 181, t("Waiting for samples"), faint, small);
  uint32_t age = radar.samples ? millis() - radar.lastSample : 0;
  String stats =
      lora ? count(radar.samples, "packet", "packets") : String(radar.rate) + (bt ? t(" adverts/s") : t(" beacons/s"));
  int sx = text(12, 216, stats, dim, small);
  // Sample age: green up to 0.5 s, yellow up to 1.5 s, then red (LoRa counts in minutes).
  if (radar.samples)
    text(sx, 216, " · " + (age < 60000 ? String(age / 1000.f, 1) + t(" s ago") : ago(age)),
         lora         ? dim
         : age < 500  ? ok
         : age < 1500 ? warn
                      : bad,
         small);
  footer({{"OK", radarSound ? t("Mute") : t("Sound")}, {"<>", t("Reset peak")}, {"BACK", t("List")}});
}
// CSI motion: one board is the beacon, this one the sensor (or the other way round).
uint32_t motionBeepAt = 0;
bool wasMoving = false;
void drawMotion() {
  auto& d = g();
  bool beacon = radar.csi == Radar::CsiBeacon, heard = radar.beaconHeard();
  uint32_t now = millis();
  panel(8, 26, 304, 34, card, 8);
  d.fillRoundRect(14, 31, 24, 24, 6, bg);
  icon(beacon ? IcRadio : IcPulse, 26, 43, 7, beacon ? accent : heard ? ok : warn, bg);
  String blocked = radar.wifi == Radar::WifiBusy                  ? t("Wi-Fi busy: probe")
                   : radar.wifi == Radar::WifiFailed              ? t("Wi-Fi error")
                   : (!beacon && radar.wifi == Radar::WifiPortal) ? t("Turn the access point off")
                                                                  : "";
  text(46, 40, beacon ? t("Beacon for another board") : t("Sensor"), ink, bold);
  text(46, 54,
       fit(blocked.length() ? blocked
           : beacon         ? String(radar.csiRate) + t(" frames/s on channel 1")
           : heard ? t("beacon ") + String(radar.csiRate) + t(" frames/s · ") + String(int(radar.csiRssi)) + " dBm"
                   : t("waiting: set the other board to CSI beacon"),
           258, small),
       blocked.length() ? warn : dim, small);
  String state;
  uint16_t color;
  if (blocked.length()) {
    state = t("NO WI-FI");
    color = warn;
  } else if (beacon) {
    state = t("BEACON ON");
    color = accent;
  } else if (!heard) {
    state = t("NO BEACON");
    color = warn;
  } else if (radar.csiStale) {
    state = t("CSI FROZEN");
    color = warn;
  } else if (radar.calibrateUntil) {
    state = t("CALIBRATING ") + String((radar.calibrateUntil - now + 999) / 1000) + t(" s");
    color = info;
  } else if (radar.moving) {
    state = t("MOTION");
    color = bad;
  } else {
    state = t("STILL");
    color = ok;
  }
  textCenter(160, 92, state, color, big);
  if (!beacon) {
    float threshold = radar.motionThreshold(), scale = max(threshold * 2.5f, radar.activity * 1.1f);
    d.fillRoundRect(14, 104, 292, 8, 3, line);
    if (heard)
      d.fillRoundRect(14, 104, max(3, int(292 * min(1.f, radar.activity / scale))), 8, 3, radar.moving ? bad : ok);
    int tx = 14 + int(291 * threshold / scale);
    d.fillRect(tx - 1, 100, 3, 16, ink);
    text(14, 128, t("activity ") + String(radar.activity * 1000, 1), dim, small);
    textRight(306, 128,
              t("threshold ") + String(threshold * 1000, 1) + (radar.baseline > 0 ? String() : t(" (default)")),
              radar.baseline > 0 ? dim : warn, small);
    // History: one point per 0.5 s, threshold dotted.
    panel(8, 136, 304, 70, card, 6);
    unsigned n = radar.motionCount();
    float top = threshold * 2.5f;
    for (unsigned i = 0; i < n; i++) top = max(top, radar.motion(i));
    int ty = 202 - int(62 * threshold / top);
    for (int x = 12; x < 308; x += 6) d.drawFastHLine(x, ty, 3, faint);
    int px = 0, py = 0;
    for (unsigned i = 0; i < n; i++) {
      int x = 12 + int(i * 296 / (Radar::MotionHistory - 1)), y = 202 - int(62 * radar.motion(i) / top);
      if (i) d.drawLine(px, py, x, y, radar.motion(i) > threshold ? bad : accent);
      px = x;
      py = y;
    }
    if (!n) textCenter(160, 175, t("Walk between the boards to see the trace"), faint, small);
    text(12, 216, fit(t("Calibrate with the area empty and still (10 s)"), 296, small), faint, small);
  } else
    text(12, 130, fit(t("Place the boards 2-5 m apart; motion between them is sensed"), 296, small), dim, small);
  if (beacon)
    footer({{"<>", t("Signals")}, {"B", t("Sensor")}, {"BACK", t("Menu")}});
  else
    footer({{"OK", t("Calibrate")}, {"<>", t("Signals")}, {"B", t("Beacon")}, {"BACK", t("Menu")}});
}
const int settingsCount = 7;
// Power off (Power.cpp): the M9 has no button on a GPIO, the T-Deck wakes on a held trackball click.
String powerOnHint() {
  return powerOnWay() >= 0 ? t("To turn on, hold the trackball pressed") : t("To turn on: RESET or the power switch");
}
bool farewell = false; // the board is about to turn off: the last frame
void drawOff() {
  panel(30, 62, 260, 116, card, 10);
  icon(IcPower, 160, 96, 14, farewell ? dim : warn, card);
  textCenter(160, 138, farewell ? t("Device is off") : t("Turning off..."), ink, bold);
  textCenter(160, 160, powerOnHint(), dim, small);
}
uint32_t powerArmed = 0; // the first OK on "Turn off" arms it for 5 s
uint32_t discardArmed = 0; // the first BACK on an unsaved Radio or Display draft arms discarding it for 5 s
void drawSettings() {
  Icon icons[] = {IcRadio, IcScreen, IcCompass, IcWifi, IcPulse, IcHelp, IcPower};
  uint16_t hues[] = {accent, info, warn, info, ok, dim, bad};
  String names[] = {t("Radio"),         t("Screen & device"), t("GPS & compass"), t("Connections"),
                    t("Module health"), t("Keys & help"),     t("Turn off")};
  String details[] = {
      String(config.frequency, 3) + t(" MHz · SF") + String(config.sf) + " · " + String(config.power) + " dBm",
      t("Brightness ") + String(config.brightness) + " · " + langNames[config.lang < LangCount ? config.lang : 0],
      "GPS " + flag(config.gps) + " · " +
          (navigation.calibrated ? t("compass calibrated") : t("compass not calibrated")),
      "Wi-Fi " + flag(portalActive()) + " · BLE " + flag(bleActive()),
      "RX " + String(meshRadio.rxCount) + " · TX " + String(meshRadio.txCount) + " · " + String(meshRadio.relayed) +
          t(" relayed"),
      t("What every key does"),
      powerOnHint()};
  int first = max(0, selected - 5);
  for (int i = first; i < settingsCount && i < first + 6; i++) {
    int y = 24 + (i - first) * 32;
    bool focus = selected == i;
    listRow(y, 30, focus);
    target(8, y, 304, 30, i);
    g().fillRoundRect(16, y + 4, 22, 22, 5, card);
    icon(icons[i], 27, y + 15, 7, hues[i], card);
    text(46, y + 14, names[i], ink, bold);
    text(46, y + 26, fit(details[i], 250, small), dim, small);
    tri(302, y + 15, 1, 4, focus ? accent : faint);
  }
  scrollbar(first, 6, settingsCount, 24, 190);
  footer({{"OK", t("Open")}, {"^v", t("Select")}, {"BACK", t("Menu")}});
}
constexpr int RowLauncherLayout = 3;
constexpr int RowTheme = 13, RowAccent = 14, RowNight = 15, RowPassword = 16, RowSoundChannels = 17,
              RowSoundDirect = 18;
bool credentialEnabled();
int settingRows() {
  return page == Display ? 20 : 14;
} // Includes the final Save row. Radio row 0: the regional preset (RadioPresets.h).
// Radio rows 9 and 10: the default region of our floods (OK types one, <> the regions found) and the region search;
// row 11: the profile before the last change of frequency, BW, SF or CR (OK returns to it), or during a test of a
// network profile (radio try from the page or app) the test (OK keeps it; a restart returns the saved one).
// Row 12: the network search (NetFind.h): OK starts it near the position (or stops it), and once a network is found
// tests it (radio try); <> searches every profile of the catalogue.
const int RowPreset = 0, RowRegion = 9, RowFind = 10, RowPrev = 11, RowNet = 12;
String profileText(const RadioProfile& r) {
  return String(r.frequency, 3) + " SF" + String(r.sf) + " BW" + String(r.bandwidth, 1) + " CR" + String(r.cr);
}
String regionSearchText() {
  auto& s = regions::search;
  if (regions::searching()) return t("Searching: ") + count(s.repeaters, "repeater", "repeaters");
  return s.state == regions::Search::Done ? count(s.count, "region", "regions") : t("OK: start");
}
String netValue() {
  auto& s = netfind::state;
  if (netfind::running()) return t("Listening ") + String(s.step + 1) + "/" + String(s.count);
  if (s.phase == netfind::State::Done && s.best != 255) {
    auto& r = s.results[s.best];
    return "SF" + String(r.radio.sf) + " · " + count(r.repeaters, "repeater", "repeaters");
  }
  return s.phase == netfind::State::Done ? t("Nothing new") : t("OK: start");
}
String netHint() {
  auto& s = netfind::state;
  if (netfind::running()) {
    auto& r = s.results[s.step];
    return netfind::profileText(r.radio) + (r.place != 255 ? " · " + netfind::placeName(r.place) : String());
  }
  if (s.phase == netfind::State::Done && s.best != 255) {
    auto& r = s.results[s.best];
    return t("OK: test ") + netfind::profileText(r.radio) +
           (r.place != 255 ? " · " + netfind::placeName(r.place) : String());
  }
  if (s.phase == netfind::State::Done) {
    unsigned n = 0;
    for (unsigned i = 0; i < s.count; i++)
      if (s.results[i].current) n = s.results[i].repeaters;
    return t("Current profile: ") + count(n, "repeater", "repeaters");
  }
  return t("Try every regional preset");
}
String regionsFound() {
  String n;
  for (unsigned i = 0; i < regions::search.count; i++) n += String(i ? ", " : "") + regions::search.found[i].name;
  return n;
}
String settingName(int i) {
  if (page == Radio) {
    const char* names[] = {tr("Preset"),         tr("Frequency"), tr("Bandwidth"),       tr("Spreading factor"),
                           tr("Coding rate"),    tr("TX power"),  tr("Relay hop limit"), tr("Relay packets"),
                           tr("Path hash size"), tr("Region"),    tr("Find regions")};
    if (i == RowNet) return t("Find network");
    if (i == RowPrev) return config.trial.set() ? t("Radio test") : t("Previous radio");
    return i < 11 ? names[i] : "";
  }
  const char* names[] = {tr("Name"),
                         tr("Language"),
                         tr("Day brightness"),
                         tr("Home layout"),
                         tr("Sounds"),
                         tr("Auto lock"),
                         tr("Dim screen"),
                         tr("GPS receiver"),
                         tr("UTC offset"),
                         tr("Battery shows"),
                         tr("Lock screen"),
                         tr("Wake on message"),
                         tr("Message popup"),
                         tr("Theme"),
                         tr("Accent colour"),
                         tr("Night brightness"),
                         tr("Screen password"),
                         tr("Channel sound"),
                         tr("Direct message sound")};
  return i <= RowSoundDirect ? names[i] : "";
}
String settingValue(const Config& c, int i) {
  if (page == Radio) {
    switch (i) {
    case RowPreset: {
      int p = radiopresets::match(c);
      return p < 0 ? String(t("Custom")) : String(radiopresets::list[p].name);
    }
    case 1: return String(c.frequency, 3) + t(" MHz");
    case 2: return String(c.bandwidth, 1) + t(" kHz");
    case 3: return "SF" + String(c.sf);
    case 4: return "4/" + String(c.cr);
    case 5: return String(c.power) + " dBm";
    case 6: return String(c.hops);
    case 7: return flag(c.relay);
    case 8: return count(c.pathHash, "byte", "bytes");
    case RowRegion: return c.region[0] ? String(c.region) : String(t("None"));
    case RowFind: return regionSearchText();
    case RowNet: return netValue();
    case RowPrev:
      return config.trial.set() ? t("OK: keep") : config.previous.set() ? profileText(config.previous) : t("None");
    }
    return "";
  }
  switch (i) {
  case RowTheme: {
    const char* modes[] = {tr("Dark"), tr("Light"), tr("Auto")};
    return modes[c.theme];
  }
  case RowAccent: {
    const char* colors[] = {tr("Lime"), tr("Blue"), tr("Violet"), tr("Amber")};
    return colors[c.accentColor];
  }
  case RowNight: return c.nightBrightness ? String(c.nightBrightness) : t("Same as day");
  case RowPassword: return flag(credentialEnabled());
  case RowSoundChannels: return c.soundChannels == 0 ? t("Off") : c.soundChannels == 1 ? t("All") : t("Mentions");
  case RowSoundDirect: return flag(c.soundDirect);
  case 0: return c.name;
  case 1: return langNames[c.lang < LangCount ? c.lang : 0];
  case 2: return String(c.brightness);
  case RowLauncherLayout: return c.launcherTiles ? t("Tiles") : t("List");
  case 4: return flag(c.sound);
  case 5: return c.autoLock ? String(c.autoLock) + t(" s") : t("Off");
  case 6: return c.dimAfter ? String(c.dimAfter) + t(" s") : t("Off");
  case 7: return flag(c.gps);
  case 8: {
    int m = abs(c.utcOffset);
    char b[12];
    snprintf(b, sizeof b, "%c%02d:%02d", c.utcOffset < 0 ? '-' : '+', m / 60, m % 60);
    return b;
  }
  case 9: return c.batteryVolts ? t("Volts") : t("Percent");
  case 10: return !credentialEnabled() && c.lockDetails ? t("Details") : t("Hidden");
  case 11:
  case 12: {
    unsigned mode = i == 11 ? c.notifyWake : c.notifyPopup;
    return mode == 0 ? t("Off") : mode == 1 ? t("All") : t("Mentions");
  }
  }
  return "";
}
String settingHint(int i) {
  if (page == Radio) {
    switch (i) {
    case RowPreset: return t("MeshCore regional settings: frequency, BW, SF, CR");
    case 1: return t("1 kHz steps; must match every node");
    case 2: return t("62.5 / 125 / 250 / 500 kHz");
    case 3: return t("Higher SF: longer range, slower");
    case 4: return t("Error correction 4/5 to 4/8");
    case 5: return t("Radio chip setting, 0-22 dBm");
    case 6: return t("Relay packets with fewer hops than this");
    case 7: return t("Forward other nodes' MeshCore packets");
    case 8: return t("Per hop in our floods; older MeshCore drops 2-3");
    case RowRegion: return t("Repeaters pass on our floods by it; OK: type");
    case RowNet: return netHint();
    case RowFind:
      return regions::search.count ? t("Found: ") + regionsFound() : t("Asks repeaters that hear this device");
    case RowPrev:
      return config.trial.set()      ? t("A restart returns ") + profileText(config.trial)
             : config.previous.set() ? t("OK: return to it")
                                     : t("Kept on a change of frequency, BW, SF or CR");
    }
    return t("Applied to the radio when saved");
  }
  switch (i) {
  case RowPassword: return t("OK: set or change password");
  case RowSoundChannels: return t("Beep for channel messages; repeats stay silent");
  case RowSoundDirect: return t("Beep for direct messages");
  case RowTheme:
  case RowNight: return t("Day 07:00–20:00, local time");
  case 0: return t("OK: edit, 1-24 UTF-8 bytes");
  case 1: return t("Menu language");
  case 2: return "10-255";
  case RowLauncherLayout: return t("Applies to Home and all folders");
  case 4: return t("Tone for messages and ACKs");
  case 5: return t("0 = off, 30-600 s");
  case 6: return t("0 = off, 10-600 s");
  case 7: return t("Receiver power");
  case 8: return t("Local time, 15 minute steps");
  case 9: return t("Status bar: charge estimate or measured voltage");
  case 10: return t("Message senders while locked");
  }
  return t("Stored in device memory");
}
bool draftChanged() {
  for (int i = 0; i < settingRows() - 1; i++)
    if (settingValue(draft, i) != settingValue(config, i)) return true;
  return false;
}
// Wardrive (Wardrive.h): what it logs, the position it uses, the last ping and the repeaters heard.
String wardriveMode() {
  if (!wardrive::running()) return t("Off");
  String s;
  if (wardrive::settings.passive) s = t("Listening");
  if (wardrive::settings.ping) s += String(s.length() ? " + " : "") + t("pings");
  if (wardrive::settings.nets) s += String(s.length() ? " + " : "") + "Wi-Fi/BLE";
  return s;
}
void drawWardrive() {
  int32_t lat, lon;
  bool phone, fix = wardrive::position(lat, lon, phone), on = wardrive::running();
  panel(8, 26, 304, 44, card, 8);
  icon(IcTower, 26, 48, 9, on ? ok : faint, card);
  text(44, 44, fit(wardriveMode(), 256, bold), on ? ink : dim, bold);
  text(44, 61,
       fit(fix ? (phone ? t("Phone: ") : "GPS: ") + String(lat / 1e6, 5) + ", " + String(lon / 1e6, 5)
               : t("No position: GPS fix or phone app"),
           256, small),
       fix ? dim : warn, small);
  panel(8, 74, 304, 42, card, 8);
  String counts = count(wardrive::points(), "point", "points");
  if (wardrive::settings.nets || wardrive::nets()) counts += " · Wi-Fi/BLE " + String(wardrive::nets());
  text(18, 90, fit(counts, 288, small), ink, small);
  const auto& r = wardrive::last();
  String last = wardrive::pinging() ? t("Ping sent, listening for repeaters...")
                : !r.done           ? t("No pings yet: P sends one")
                : r.count           ? t("Last ping: ") + count(r.count, "repeater", "repeaters") + ", " +
                                hashText(r.hash, r.size) + " " + snrText(r.snr) + " · " + ago(millis() - r.at)
                          : t("Last ping: no repeater heard it") + String(" · ") + ago(millis() - r.at);
  text(18, 107, fit(last, 288, small), wardrive::pinging() ? accent : r.done && !r.count ? bad : dim, small);
  const wardrive::Repeater* list;
  unsigned n = wardrive::repeaters(list), order[24];
  for (unsigned i = 0; i < n; i++) order[i] = i;
  std::sort(order, order + n, [&](unsigned a, unsigned b) {
    unsigned x = list[a].echoes * 4 + list[a].rx, y = list[b].echoes * 4 + list[b].rx;
    return x != y ? x > y : list[a].snr > list[b].snr;
  });
  if (!n) textCenter(160, 150, on ? t("No repeaters heard yet") : t("OK starts listening and pings"), faint, small);
  for (unsigned i = 0; i < n && i < 5; i++) {
    auto& e = list[order[i]];
    int y = 132 + i * 17;
    text(18, y, hashText(e.hash, e.size), ink, bold);
    text(80, y, snrText(e.snr), snrColor(e.snr), small);
    text(150, y, t("rx ") + String(e.rx) + t(" · echo ") + String(e.echoes), dim, small);
    textRight(306, y, ago(millis() - e.at), faint, small);
  }
  footer({{"OK", on ? t("Stop") : t("Start")}, {"P", t("Ping")}, {"N", "Wi-Fi/BLE"}, {"BACK", t("Back")}});
}
// OK starts (listening and pings) or stops; P pings now; N switches the Wi-Fi/BLE log.
bool wardriveKey(int key) {
  StaticJsonDocument<128> d;
  d["action"] = "settings";
  if (key == Enter) {
    if (wardrive::running()) {
      d["action"] = "stop";
    } else {
      d["passive"] = true;
      d["ping"] = true;
    }
    String r = wardrive::command(d.as<JsonObjectConst>());
    if (r.startsWith("ERR"))
      notice(r, bad);
    else
      notice(wardrive::running() ? t("Wardrive on") : t("Wardrive off"), wardrive::running() ? ok : dim);
    return true;
  }
  if (key == 'p' || key == 'P') {
    int32_t lat, lon;
    bool phone;
    notice(wardrive::ping()                       ? t("Ping queued")
           : !wardrive::position(lat, lon, phone) ? t("No position yet")
                                                  : t("Wait: a ping is under way"),
           wardrive::pinging() ? accent : warn);
    return true;
  }
  if (key == 'n' || key == 'N') {
    d["nets"] = !wardrive::settings.nets;
    String r = wardrive::command(d.as<JsonObjectConst>());
    notice(r.startsWith("ERR") ? r : "Wi-Fi/BLE: " + flag(wardrive::settings.nets), r.startsWith("ERR") ? bad : ok);
    return true;
  }
  return false;
}
String quickName(uint64_t id) {
  if (auto* c = meshRadio.channel(id)) return c->name;
  if (const Peer* p = meshRadio.findContact(id)) return p->name;
  return t("Choose recipient");
}
void drawQuick() {
  unsigned n = quickTargets ? quickSend::targetCount() : quickSend::Count + 1;
  selected = constrain(selected, 0, max(0, int(n) - 1));
  if (quickTargets) {
    text(12, 44, fit(t("Choose recipient"), 296, bold), accent, bold);
    int first = max(0, selected - 5);
    for (unsigned i = first; i < n && i < unsigned(first + 6); i++) {
      int y = 52 + (i - first) * 26;
      listRow(y, 24, int(i) == selected);
      target(8, y, 304, 24, i);
      text(18, y + 17, fit(quickName(quickSend::target(i)), 284), int(i) == selected ? ink : dim);
    }
    scrollbar(first, 6, n, 52, 156);
  } else {
    bool focus = selected == 0;
    target(8, 28, 304, 40, 0);
    panel(8, 28, 304, 40, focus ? cardHi : card, 7);
    if (focus) ring(8, 28, 304, 40, 7);
    text(18, 42, fit(t("Choose recipient"), 260, small), dim, small);
    text(18, 59, fit(quickName(quickReplyTo ? quickReplyTo : quickSend::state.to), 260, bold), accent, bold);
    tri(299, 49, 1, 4, accent);
    int first = max(1, selected - 4);
    for (unsigned i = first; i < n && i < unsigned(first + 5); i++) {
      int y = 75 + (i - first) * 26;
      listRow(y, 24, int(i) == selected);
      target(8, y, 304, 24, i);
      text(18, y + 17, fit(String(quickSend::state.text[i - 1]), 284), int(i) == selected ? ink : dim);
    }
    scrollbar(first - 1, 5, quickSend::Count, 75, 130);
  }
  footer({{"^v", t("choose")}, {"OK", quickTargets || selected == 0 ? t("select") : t("send")}, {"BACK", t("exit")}});
}
bool quickKey(int key) {
  unsigned n = quickTargets ? quickSend::targetCount() : quickSend::Count + 1;
  if (key == KeyBack) {
    if (quickTargets) {
      quickTargets = false;
      selected = 0;
    } else {
      quickReplyTo = 0;
      change(quickReturn, true);
    }
    return true;
  }
  if (key == KeyUp || key == KeyDown) {
    if (n) selected = (selected + (key == KeyUp ? int(n) - 1 : 1)) % n;
    return true;
  }
  if (key == Enter) {
    if (quickTargets) {
      if (!n) return true;
      StaticJsonDocument<128> d;
      d["action"] = "target";
      d["to"] = meshRadio.idText(quickSend::target(selected % n));
      String r = quickSend::command(d.as<JsonObjectConst>());
      notice(r, r.startsWith("OK") ? ok : bad);
      if (r.startsWith("OK")) {
        quickTargets = false;
        quickReplyTo = 0;
        selected = 1;
      }
    } else if (selected == 0) {
      quickTargets = true;
      selected = 0;
    } else {
      String r = quickSend::send(selected - 1, quickReplyTo);
      notice(r, r.startsWith("OK") ? ok : bad);
    }
    return true;
  }
  return true;
}
void drawEditor() {
  int rows = settingRows(), first = max(0, selected - 6);
  bool changed = draftChanged();
  for (int i = first; i < rows && i < first + 7; i++) {
    int y = 25 + (i - first) * 23;
    bool focus = selected == i;
    target(8, y, 304, 21, i);
    if (i == rows - 1) {
      panel(8, y, 304, 21, changed ? (focus ? accent : cardHi) : card, 6);
      if (focus && !changed) ring(8, y, 304, 21, 6, line);
      textCenter(160, y + 15, changed ? t("Save changes") : t("No changes"), changed ? (focus ? bg : ink) : dim, bold);
      continue;
    }
    listRow(y, 21, focus);
    String value = settingValue(draft, i);
    bool diff = value != settingValue(config, i);
    if (diff) g().fillCircle(17, y + 10, 2, accent);
    text(24, y + 15, settingName(i), focus ? ink : dim);
    if (focus && !(page == Display && (i == 0 || i == RowPassword)) &&
        !(page == Radio && (i == RowFind || i == RowPrev || i == RowNet))) {
      int w = measure(value, bold);
      tri(300, y + 10, 1, 4, accent);
      text(292 - w, y + 15, value, accent, bold);
      tri(284 - w, y + 10, 3, 4, accent);
    } else
      textRight((page == Display && i == 0) || (page == Radio && (i == RowFind || i == RowPrev || i == RowNet)) ? 306
                                                                                                                : 300,
                y + 15, fit(value, 150), diff ? accent : ink, focus ? bold : body);
  }
  scrollbar(first, 7, rows, 25, 160);
  int hintWidth = 300;
  if (changed) {
    String label = t("Save");
    int w = measure(label, bold) + 34, x = 312 - w;
    panel(x, 190, w, 26, accent, 6);
    panel(x + 5, 195, 16, 16, bg, 3);
    textCenter(x + 13, 207, "S", accent, small);
    text(x + 26, 208, label, bg, bold);
    target(x, 190, w, 26, -'s');
    hintWidth = x - 18;
  }
  text(10, 207, fit(settingHint(selected), hintWidth, small), faint, small);
  if (page == Display && selected == RowPassword)
    footer({{"OK", t("Edit")}, {"BACK", t("Cancel")}});
  else if (page == Display && selected == 0)
    footer({{"OK", t("Edit name")}, {"^v", t("Select")}, {"BACK", t("Cancel")}});
  else if (page == Radio && selected == RowPrev)
    footer({{"OK", config.trial.set() ? t("Keep") : t("Return")}, {"^v", t("Select")}, {"BACK", t("Cancel")}});
  else if (page == Radio && selected == RowNet) {
    auto& s = netfind::state;
    footer({{"OK", netfind::running()                                 ? t("Stop")
                   : s.phase == netfind::State::Done && s.best != 255 ? t("Test")
                                                                      : t("Search")},
            {"BACK", t("Cancel")}});
  } else if (page == Radio && selected == RowFind)
    footer({{"OK", t("Search")}, {"^v", t("Select")}, {"BACK", t("Cancel")}});
  else if (page == Radio && selected == RowRegion)
    footer({{"<>", t("Found")}, {"OK", t("Type")}, {"BACK", t("Cancel")}});
  else
    footer({{"<>", t("Change")}, {"OK", selected == rows - 1 ? t("Save") : t("Next value")}, {"BACK", t("Cancel")}});
}
void toggle(int x, int y, bool on) {
  panel(x, y, 30, 16, on ? accent : line, 8);
  g().fillCircle(on ? x + 22 : x + 8, y + 8, 6, on ? bg : dim);
}
void drawNetwork() {
  auto& d = g();
  bool wifi = portalActive(), ble = bleActive(), web = internet.enabled;
  int y = 25;
  auto cardAt = [&](int i, int h, Icon ic, const String& name, const String& state, bool on) {
    bool focus = selected == i;
    target(8, y, 304, h, i);
    panel(8, y, 304, h, focus ? cardHi : card, 8);
    if (focus) ring(8, y, 304, h, 8);
    d.fillRoundRect(16, y + 7, 24, 24, 6, bg);
    icon(ic, 28, y + 19, 7, on ? info : faint, bg);
    text(48, y + 17, name, ink, bold);
    text(48, y + 30, fit(state, 220, small), dim, small);
  };
  int wh = wifi ? 56 : 38;
  cardAt(0, wh, IcWifi, t("Wi-Fi access point"),
         wifi ? "192.168.4.1 · " + String(t("Web chat and map upload")) : t("Off"), wifi);
  toggle(274, y + 11, wifi);
  // The network and its password, whole, as on the home network card below (the address is in the line above).
  if (wifi) {
    text(48, y + 48, "MM-" + meshRadio.idText(meshRadio.nodeId).substring(6), accent, bold);
    int pw = measure(portalPassword(), bold);
    textRight(302, y + 48, portalPassword(), ink, bold);
    icon(IcKey, 291 - pw, y + 44, 6, dim, selected == 0 ? cardHi : card);
  }
  // On the home network the device page answers at the client's address, with the same password.
  y += wh + 6;
  int ih = internet.online() ? 56 : 38;
  cardAt(1, ih, IcWifi, t("Internet over Wi-Fi"), internet.stateText(), internet.online());
  toggle(274, y + 11, web);
  if (internet.online()) {
    text(48, y + 48, "http://" + internet.address(), accent, bold);
    int pw = measure(portalPassword(), bold);
    textRight(302, y + 48, portalPassword(), ink, bold);
    icon(IcKey, 291 - pw, y + 44, 6, dim, selected == 1 ? cardHi : card);
  } // the password, whole
  y += ih + 6;
  cardAt(2, 38, IcBle, "Bluetooth LE", ble ? bleName() : t("Off"), ble);
  toggle(274, y + 11, ble);
  if (ble) textRight(266, y + 17, "PIN " + String(blePin()), accent, bold);
  y += 44;
  if (y + 38 <= 216)
    cardAt(3, 38, IcKey, t("MeshCore identity"), meshRadio.publicKeyText().substring(0, 24) + "...", true);
  footer({{"OK", selected == 1 ? t("Networks") : t("Toggle")}, {"^v", t("Select")}, {"BACK", t("Back")}});
}
// Internet over Wi-Fi: the client switch, networks in range, then saved networks out of range.
String pendingSsid;
struct NetRow {
  String ssid;
  int rssi;
  bool open, saved, visible;
};
unsigned netRows(NetRow* rows, unsigned max) {
  unsigned n = 0;
  for (unsigned i = 0; i < internet.seenCount && n < max; i++) {
    auto& v = internet.seen[i];
    rows[n++] = {v.ssid, v.rssi, v.open, v.saved, true};
  }
  for (unsigned i = 0; i < Internet::MaxSaved && n < max; i++) {
    String name = internet.savedName(i);
    if (!name.length()) continue;
    bool seen = false;
    for (unsigned k = 0; k < internet.seenCount; k++)
      if (name == internet.seen[k].ssid) seen = true;
    if (!seen) rows[n++] = {name, -127, false, true, false};
  }
  return n;
}
// Internet errors come in English from Internet.cpp (also shown over USB).
String netError() {
  String e = internet.error;
  if (config.lang == LangEn) return e;
  const char* const map[] = {N_("Wrong password or refused: "),
                             N_("No answer from "),
                             N_("No saved network in range"),
                             N_("Connection lost"),
                             N_("Wi-Fi scan failed"),
                             N_("Wi-Fi scan timed out"),
                             N_("Clock not set"),
                             N_("SSID 1-32 bytes, password empty or 8-63"),
                             N_("Tile is not 256 or 512 px"),
                             N_("Not a PNG image"),
                             N_("No location"),
                             N_("SD full: web tiles not cached")};
  for (const char* m : map)
    if (e.startsWith(m)) return String(trText(m)) + e.substring(strlen(m));
  return e;
}
unsigned netCount() {
  NetRow rows[Internet::MaxSeen + Internet::MaxSaved];
  return netRows(rows, Internet::MaxSeen + Internet::MaxSaved);
}
uint32_t netScanAt = 0;
void netListEnter() {
  if (selected == 0) {
    internet.setEnabled(!internet.enabled);
    notice(internet.enabled ? t("Wi-Fi client on") : t("Wi-Fi client off"), internet.enabled ? ok : dim);
    return;
  }
  NetRow rows[Internet::MaxSeen + Internet::MaxSaved];
  unsigned total = netRows(rows, Internet::MaxSeen + Internet::MaxSaved);
  if (unsigned(selected - 1) >= total) return;
  auto& r = rows[selected - 1];
  if (r.saved || r.open) {
    if (!r.saved && !internet.save(r.ssid, "")) {
      notice(netError(), bad);
      return;
    }
    internet.connectTo(r.ssid);
    notice(t("Connecting to ") + r.ssid);
    return;
  }
  pendingSsid = r.ssid;
  edit = "";
  editing = true;
}
void netListErase() {
  NetRow rows[Internet::MaxSeen + Internet::MaxSaved];
  unsigned total = netRows(rows, Internet::MaxSeen + Internet::MaxSaved);
  if (selected > 0 && unsigned(selected - 1) < total && rows[selected - 1].saved) {
    String name = rows[selected - 1].ssid;
    if (internet.forget(name)) notice(t("Forgotten: ") + name, dim);
    selected = min(selected, int(netCount()));
    return;
  }
  internet.rescan();
  notice(t("Searching..."), dim);
}
void drawNetList() {
  auto& d = g();
  bool focus = selected == 0;
  target(8, 25, 304, 40, 0);
  panel(8, 25, 304, 40, focus ? cardHi : card, 8);
  if (focus) ring(8, 25, 304, 40, 8);
  d.fillRoundRect(16, 33, 24, 24, 6, bg);
  icon(IcWifi, 28, 45, 7, internet.online() ? ok : internet.enabled ? info : faint, bg);
  // The last failure (wrong password, no answer) stays visible until the next attempt starts.
  bool failed = internet.enabled && !internet.online() && internet.error.length() &&
                internet.state != Internet::Connecting && internet.state != Internet::Scanning;
  text(48, 42, t("Wi-Fi client"), ink, bold);
  text(48, 56, fit(failed ? netError() : internet.stateText(), 220, small),
       internet.online() ? ok
       : failed          ? warn
                         : dim,
       small);
  toggle(274, 37, internet.enabled);
  NetRow rows[Internet::MaxSeen + Internet::MaxSaved];
  unsigned total = netRows(rows, Internet::MaxSeen + Internet::MaxSaved);
  if (!internet.enabled)
    textCenter(160, 120, t("Turn the client on to find networks"), dim);
  else if (!total)
    textCenter(160, 120, internet.state == Internet::Paused ? internet.stateText() : t("Searching for networks..."),
               dim);
  int first = max(0, selected - 1 - 5);
  for (unsigned i = first; i < total && i < unsigned(first + 6); i++) {
    int y = 70 + (i - first) * 24;
    auto& r = rows[i];
    bool f = selected == int(i) + 1;
    listRow(y, 22, f);
    target(8, y, 304, 22, i + 1);
    bool current = internet.online() && internet.ssid == r.ssid;
    if (r.visible)
      bars(18, y + 16, r.rssi >= -55 ? 4 : r.rssi >= -67 ? 3 : r.rssi >= -78 ? 2 : 1, current ? ok : f ? ink : dim);
    else
      text(18, y + 15, "--", faint, small);
    text(40, y + 15, fit(r.ssid, 150), current ? ok : ink, f ? bold : body);
    String tag = current   ? t("connected")
                 : r.saved ? (r.visible ? t("saved") : t("saved, away"))
                 : r.open  ? t("open")
                           : "";
    if (!r.open && !current) icon(IcLock, 297, y + 11, 6, dim);
    if (tag.length()) textRight(286, y + 15, tag, current ? ok : r.saved ? accent : dim, small);
  }
  scrollbar(first, 6, total, 70, 144);
  bool saved = selected > 0 && unsigned(selected - 1) < total && rows[selected - 1].saved;
  footer({{"OK", selected == 0 ? t("On/off") : t("Connect")},
          {"DEL", saved ? t("Forget") : t("Rescan")},
          {"BACK", t("Back")}});
}
String keyName(uint32_t k) {
  switch (k) {
  case 0: return "-";
  case KeyMsg: return "MSG";
  case KeyHome: return "HOME";
  case KeyMenu: return t("Menu");
  case KeyAdv: return t("Map pin");
  case KeyGps: return t("Map pin") + " (GPS)";
  case KeyMap: return "MAP";
  case KeyBack: return "BACK";
  case KeyMic: return "MIC";
  case KeySet: return "CTRL";
  case KeyHold: return t("hold OK");
  case KeyLeft: return t("Left");
  case KeyUp: return t("Up");
  case KeyDown: return t("Down");
  case KeyRight: return t("Right");
  case Enter: return "OK";
  case Erase: return "DEL";
  case ' ': return t("space");
  }
  char b[16];
  if (k > 32 && k < 127)
    snprintf(b, sizeof b, "'%c'", char(k));
  else
    snprintf(b, sizeof b, "0x%02X", unsigned(k));
  return b;
}
void drawDiagnostics() {
  const char* names[] = {"LoRa", nullptr, "SD", "LittleFS", "RTC", "GPS / NMEA", nullptr, "IMU"};
  String label[8];
  for (int i = 0; i < 8; i++) label[i] = names[i] ? String(names[i]) : i == 1 ? t("Keyboard") : t("Compass");
  bool state[8];
  moduleStates(state);
  for (int i = 0; i < 8; i++) {
    int x = 8 + (i % 2) * 154, y = 25 + (i / 2) * 22;
    panel(x, y, 150, 19, card, 5);
    g().fillCircle(x + 11, y + 9, 4, state[i] ? ok : bad);
    text(x + 21, y + 14, label[i], ink);
    textRight(x + 144, y + 14, state[i] ? "OK" : t("n/a"), state[i] ? ok : bad, small);
  }
  panel(8, 116, 304, 74, card, 8);
  uint32_t up = millis() / 1000;
  char uptime[16];
  snprintf(uptime, sizeof uptime, "%lu:%02lu:%02lu", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60),
           (unsigned long)(up % 60));
  String cells[][2] = {{"RX", String(meshRadio.rxCount)},
                       {"TX", String(meshRadio.txCount)},
                       {t("Relayed"), String(meshRadio.relayed)},
                       {t("Rejected"), String(meshRadio.rejected)},
                       {"RSSI", String(int(meshRadio.lastRssi)) + " dBm"},
                       {"SNR", String(meshRadio.lastSnr, 1) + " dB"},
                       {"RAM", String(ESP.getFreeHeap() / 1024) + " K"},
                       {t("Uptime"), uptime}};
  for (int i = 0; i < 8; i++) {
    int x = 16 + (i % 4) * 75, y = 134 + (i / 4) * 32;
    text(x, y, cells[i][0], dim, small);
    text(x, y + 15, cells[i][1], ink, bold);
  }
  text(10, 207, t("Boot ") + String(config.bootCounter) + t(" · last key: ") + keyName(hardware.lastKey), faint, small);
  footer({{"OK", hardware.fsOk ? t("Crypto test + sound") : t("Create storage...")}, {"BACK", t("Back")}});
}
void drawHelp() {
  Hint keys[] = {{"MSG", t("Chats")},
                 {"MAP", t("Map; L - saved maps")},
                 {"HOME", t("Menu")},
                 {"BACK", t("Previous screen")},
                 {"CTRL", t("Settings")},
                 {tr("Map pin"), t("Announce this node; hold: GPS on/off")},
                 {tr("Menu"), t("App actions and radio controls")},
                 hardware.micKey ? Hint{"MIC", t("Lock screen (there is no microphone)")}
                                 : Hint{tr("hold OK"), t("Home screen: lock")},
                 {"OK", t("Open / send; twice or hold: unlock")},
                 {tr("Right"), t("In text: capital letter")}};
  for (int i = 0; i < 10; i++) {
    int y = 24 + i * 19;
    int kw = max(30, measure(keys[i].key, small) + 10);
    g().fillRoundRect(10, y, kw, 15, 3, line);
    textCenter(10 + kw / 2, y + 11, keys[i].key, ink, small);
    text(max(50, kw + 16), y + 12, fit(keys[i].action, 258), i % 2 ? dim : ink);
  }
  footer({{"BACK", t("Settings")}});
}
#include "UiScreenLock.inc"
void drawLocked() {
  if (credentialEnabled()) {
    drawCredential();
    return;
  }
  unsigned n = unreadTotal();
  int top = 0;
  textCenter(160, 106 + top, clockText(), ink, digits);
  textCenter(160, 130 + top, dateText(), dim);
  int y = 144 + top;
  if (n) {
    const ChatMessage* last = nullptr;
    for (int i = meshRadio.historyCount - 1; i >= 0; i--)
      if (!meshRadio.history[i].outgoing) {
        last = &meshRadio.history[i];
        break;
      }
    panel(40, y, 240, 40, card, 8);
    icon(IcMail, 62, y + 20, 9, warn, card);
    text(80, y + 16, count(n, "new message", "new messages"), ink, bold);
    if (!config.lockDetails)
      text(80, y + 32, t("Unlock to read"), dim, small);
    else if (last)
      text(80, y + 32, fit(t("Last from ") + String(last->name), 190, small), dim, small);
  } else
    textCenter(160, 166, meshRadio.ready ? t("Radio and GPS keep working") : t("Radio error"),
               meshRadio.ready ? faint : bad, small);
  icon(IcLock, 160, 200, 7, dim);
  String unlock = t("Twice or hold: unlock");
  if (hardware.micKey)
    footer({{"OK", unlock}, {"MIC", t("Lock")}});
  else
    footer({{"OK", unlock}});
}
#include "UiChannels.inc"
#include "UiRemote.inc"
#include "UiClipboard.inc"
void drawEditing() {
  targetCount = hintCount = 0; // the page under the dialog does not take taps
  uint16_t* px = g().getBuffer();
  for (int i = 0; i < 320 * 240; i++) px[i] = (px[i] >> 1) & 0x7bef; // dim the page under the dialog
  bool key = page == Network, pass = page == NetList;
  panel(12, 60, 296, 122, cardHi, 10);
  g().drawRoundRect(12, 60, 296, 122, 10, line);
  text(26, 82,
       page == Memo          ? memoLabel()
       : page == Nodes       ? t("Filter by tag")
       : key                 ? t("Network key · 64 hex")
       : page == ChannelAdd  ? channelEditTitle()
       : page == Remote      ? remoteEditTitle()
       : page == Radio       ? String(t("Region"))
       : page == ChannelInfo ? String(t("Region of the channel"))
       : pass                ? fit(t("Wi-Fi password: ") + pendingSsid, 268, bold)
                             : t("Device name"),
       ink, bold);
  panel(24, 92, 272, 52, bg, 6);
  g().drawRoundRect(24, 92, 272, 52, 6, accent);
  String rows[10];
  unsigned n = wrap(edit, rows, 10, 258), first = n > 3 ? n - 3 : 0;
  int cx = 32;
  for (unsigned i = first; i < n; i++) cx = text(32, 108 + (i - first) * 15, rows[i], ink);
  g().fillRect(cx + 1, 98 + max(0, int(n - first) - 1) * 15, 2, 12,
               accent); // a long text shows its end, where the cursor is
  textRight(294, 160, String(edit.length()) + "/" + String(textByteLimit()) + t(" bytes"), faint, small);
  if (page == ChannelAdd) {
    text(26, 160, fit(channelEditHint(), 230, small), faint, small);
  } else if (page == Remote) {
    text(26, 160,
         remoteEdit == EditPassword ? t("Saved after a successful login")
                                    : t("As in the MeshCore app: ver, get name, neighbors..."),
         faint, small);
  } else if (page == Radio || page == ChannelInfo) {
    text(26, 160,
         page == Radio ? t("As on repeaters: msk, sao...; empty: none")
                       : t("As on repeaters: msk, sao...; empty: default"),
         faint, small);
  } else if (page == Memo && selected)
    text(26, 160, t("Separate tags with commas"), dim, small);
  else if (page == Nodes)
    text(26, 160, t("Empty shows all contacts"), dim, small);
  else if (pass)
    text(26, 160, t("8-63 characters; -> after a letter: capital"), faint, small);
  text(26, 175,
       page == ChannelAdd ? channelEditOk()
       : page == Remote   ? (remoteEdit == EditPassword ? t("OK: log in   BACK: cancel") : t("OK: send   BACK: cancel"))
                          : t("OK: save   BACK: cancel"),
       dim, small);
  footer({{t("hold OK").c_str(), t("Clipboard")}, {"BACK", t("Cancel")}});
}
#include "UiActions.inc"
void draw() {
  applyAppearance();
  auto& c = g();
  c.fillScreen(bg);
  targetCount = hintCount = 0;
  if (powerOffPending()) {
    drawOff();
    hardware.flush();
    return;
  }
  if (locked)
    drawLocked();
  else if (activePanel != Panel::Closed)
    drawActionPanel();
  else
    switch (page) {
    case Spectrum: drawSpectrum(); break;
    case Memo: drawMemo(); break;
    case Home: drawHome(); break;
    case Threads: drawThreads(); break;
    case Chat: drawChat(); break;
    case Map: drawMap(); break;
    case Library: drawLibrary(); break;
    case Nodes: drawNodes(); break;
    case Node: drawNode(); break;
    case Sensors: drawSensors(); break;
    case Settings: drawSettings(); break;
    case Radio:
    case Display: drawEditor(); break;
    case Network: drawNetwork(); break;
    case NetList: drawNetList(); break;
    case Diagnostics: drawDiagnostics(); break;
    case Help: drawHelp(); break;
    case Scope: drawScope(); break;
    case Homing: drawHoming(); break;
    case Motion: drawMotion(); break;
    case QuickView: drawQuick(); break;
    case WardriveView: drawWardrive(); break;
    case ChannelAdd: drawChannelAdd(); break;
    case ChannelInfo: drawChannelInfo(); break;
    case Remote: drawRemote(); break;
    case Folder: drawFolder(); break;
    }
  statusBar();
  if (credentialStep != CredentialStep::Idle && !locked)
    drawCredential();
  else if (editing && !locked)
    drawEditing();
  if (clipMenu && !locked) drawClipboard();
  if (credentialStep == CredentialStep::Idle) drawToast(24); // under the status bar: clear of the text box while typing
  hardware.flush();
}
// Right after a letter toggles its case.
bool toggleLastCase(String& text) {
  if (!text.length()) return false;
  unsigned at = meshmesh::previousCharacter(text.c_str(), text.length()), i = at;
  uint32_t cp = nextCp(text, i), to = cp;
  if (cp >= 'a' && cp <= 'z')
    to = cp - 32;
  else if (cp >= 'A' && cp <= 'Z')
    to = cp + 32;
  if (to == cp) return false;
  char b[3] = {};
  if (to < 0x80)
    b[0] = to;
  else {
    b[0] = 0xc0 | to >> 6;
    b[1] = 0x80 | (to & 0x3f);
  }
  text.remove(at);
  text += b;
  return true;
}

// The network search row: stop a search, test the network found, or search (all: every profile).
void netAction() {
  auto& s = netfind::state;
  if (netfind::running()) {
    netfind::stop();
    return;
  }
  if (s.phase == netfind::State::Done && s.best != 255 && !config.sameRadio(s.results[s.best].radio)) {
    String r = netfind::use(s.best);
    bool done = r.startsWith("OK");
    if (done) draft = config;
    notice(done                             ? t("Radio test: a restart returns the saved one")
           : r.startsWith("ERR radio busy") ? t("Radio busy, retry")
                                            : r.substring(4),
           done ? ok : bad);
    return;
  }
  if (!netfind::start())
    notice(meshRadio.busy() || regions::searching() ? t("Radio busy, retry") : t("Radio offline"), bad);
}
void alter(int dir) {
  if (page == Radio) {
    switch (selected) {
    case RowPreset: {
      int p = radiopresets::step(draft, dir);
      if (p < 0) {
        notice(t("No preset for this radio"), warn);
        return;
      }
      radiopresets::apply(draft, p);
      break;
    }
    case 1: draft.frequency = constrain(roundf((draft.frequency + dir * .001f) * 1000) / 1000, 863.f, 870.f); break;
    case 2: {
      float bw[] = {62.5, 125, 250, 500};
      int i = 0;
      while (i < 3 && draft.bandwidth != bw[i]) i++;
      draft.bandwidth = bw[(i + dir + 4) % 4];
      break;
    }
    case 3: draft.sf = constrain(int(draft.sf) + dir, 7, 12); break;
    case 4: draft.cr = constrain(int(draft.cr) + dir, 5, 8); break;
    case 5: draft.power = constrain(int(draft.power) + dir, 0, MM_MAX_POWER); break;
    case 6: draft.hops = constrain(int(draft.hops) + dir, 0, 7); break;
    case 7: draft.relay = !draft.relay; break;
    case 8: draft.pathHash = constrain(int(draft.pathHash) + dir, 1, 3); break;
    case RowRegion: strlcpy(draft.region, regions::cycle(draft.region, dir, false).c_str(), sizeof draft.region); break;
    case RowNet: return;
    }
  } else {
    switch (selected) {
    case RowTheme: draft.theme = (draft.theme + dir + 3) % 3; break;
    case RowAccent: draft.accentColor = (draft.accentColor + dir + 4) % 4; break;
    case RowNight: {
      int n = draft.nightBrightness ? draft.nightBrightness : draft.brightness;
      n += dir * 15;
      draft.nightBrightness = n < 10 ? 0 : constrain(n, 10, 255);
      break;
    }
    case 1: draft.lang = langStep(draft.lang, dir); break;
    case 2: draft.brightness = constrain(int(draft.brightness) + dir * 15, 10, 255); break;
    case RowLauncherLayout: draft.launcherTiles = !draft.launcherTiles; break;
    case 4: draft.sound = !draft.sound; break;
    case 5: draft.autoLock = constrain(int(draft.autoLock) + dir * 30, 0, 600); break;
    case 6: draft.dimAfter = constrain(int(draft.dimAfter) + dir * 10, 0, 600); break;
    case 7: draft.gps = !draft.gps; break;
    case 8: draft.utcOffset = constrain(int(draft.utcOffset) + dir * 15, -720, 840); break;
    case 9: draft.batteryVolts = !draft.batteryVolts; break;
    case 10:
      if (!credentialEnabled()) draft.lockDetails = !draft.lockDetails;
      break;
    case 11: draft.notifyWake = (draft.notifyWake + dir + 3) % 3; break;
    case 12: draft.notifyPopup = (draft.notifyPopup + dir + 3) % 3; break;
    case RowSoundChannels: draft.soundChannels = (draft.soundChannels + dir + 3) % 3; break;
    case RowSoundDirect: draft.soundDirect = !draft.soundDirect; break;
    }
  }
  dirty = true;
}
// Keep the settings JSON off uiKey's stack: the saved-map library also needs SD/JSON stack space.
void saveDraft() {
  DynamicJsonDocument d(4096);
  auto error = deserializeJson(d, configJson());
  if (error) {
    notice(error.c_str(), bad);
    return;
  }
  if (page == Radio) {
    d["frequency"] = draft.frequency;
    d["bandwidth"] = draft.bandwidth;
    d["sf"] = int(draft.sf);
    d["cr"] = int(draft.cr);
    d["power"] = int(draft.power);
    d["hops"] = int(draft.hops);
    d["relay"] = draft.relay;
    d["path_hash"] = int(draft.pathHash);
    d["region"] = draft.region;
    d["preset"] = draft.preset;
  } else {
    d["theme"] = draft.theme;
    d["accent_color"] = draft.accentColor;
    d["night_light"] = draft.nightBrightness;
    d["launcher_tiles"] = draft.launcherTiles;
    d["name"] = draft.name;
    d["lang"] = langCodes[draft.lang];
    d["brightness"] = int(draft.brightness);
    d["sound"] = draft.sound;
    d["gps"] = draft.gps;
    d["auto_lock"] = int(draft.autoLock);
    d["dim_after"] = int(draft.dimAfter);
    d["utc_offset"] = int(draft.utcOffset);
    d["battery_volts"] = draft.batteryVolts;
    d["lock_details"] = draft.lockDetails;
    d["notify_wake"] = draft.notifyWake;
    d["notify_popup"] = draft.notifyPopup;
    d["sound_channels"] = draft.soundChannels;
    d["sound_direct"] = draft.soundDirect;
  }
  String reply = applySettings(d.as<JsonObjectConst>());
  bool saved = reply.startsWith("OK");
  notice(saved ? t("Settings saved") : reply, saved ? ok : bad);
  if (saved) change(Settings, true);
}
// Navigation, hold ADV and the screen settings turn the GPS on and off; an open settings draft follows.
void gpsToggle() {
  config.gps = !config.gps;
  config.save();
  hardware.setGps(config.gps);
  draft.gps = config.gps;
  notice("GPS: " + flag(config.gps), config.gps ? ok : dim);
}
void runNodeAction() {
  Peer* p = focusedPeer();
  if (!p) return;
  NodeAction acts[6];
  unsigned n = nodeActions(*p, acts);
  NodeAction a = acts[constrain(action, 0, int(n) - 1)];
  if (a != ActForget) deleteArmed = false;
  switch (a) {
  case ActWrite:
    recipient = p->id;
    chatReturn = Node;
    composer = "";
    change(Chat);
    break;
  case ActServer:
    remoteAction = 0;
    change(Remote);
    break;
  case ActTrace:
    remoteAction = 0;
    change(Remote);
    if (!remote::traceTo(p->id)) notice(t("Not sent: radio or route"), bad);
    break;
  case ActMap:
    maps.follow = false;
    maps.center(p->latitude, p->longitude);
    change(Map);
    break;
  case ActResetPath: {
    if (meshRadio.busy()) {
      notice(t("Radio busy, try again"), warn);
      break;
    }
    bool reset = meshRadio.resetPath(p->id);
    notice(reset ? t("Path reset: next message floods") : t("Path reset failed"), reset ? ok : bad);
    break;
  }
  case ActForget:
    if (!deleteArmed) {
      deleteArmed = true;
      notice(t("OK again removes the contact"), warn);
      break;
    }
    deleteArmed = false;
    if (meshRadio.removeContact(p->id)) {
      notice(t("Contact removed; its next advert adds it again"), ok);
      change(Nodes);
    } else
      notice(t("Cannot remove while sending"), bad);
    break;
  }
}
} // namespace
static bool realErase = false; // DEL tapped in the footer: a real delete, also on the T-Deck
bool uiRadarPage() {
  return page == Scope || page == Homing || page == Motion;
}
bool uiScreenOff() {
  return wakeOnly;
}
void uiFarewell() {
  farewell = true;
  wakeOnly = locked = false;
  hardware.brightness(config.backlight(time(nullptr)));
  draw();
}
void uiUseDefaultInput() {}
void uiBegin() {
  uiUseDefaultInput();
  lastPhysicalInput = lastInput = millis();
  page = Home;
  if (!handheldRead("lock", &credential, sizeof credential)) credential.version = UINT32_MAX;
  if (credentialEnabled()) lockScreen();
  draw();
}
String uiStatus() {
  StaticJsonDocument<1024> d;
  if (locked && credentialEnabled()) return "{\"locked\":true,\"page\":\"locked\"}";
  d["panel"] = activePanel == Panel::Actions ? "actions" : activePanel == Panel::Radios ? "radios" : "closed";
  if (page == Spectrum) spectrumStatus(d);
  if (page == Home) d["unread"] = unreadTotal();
  if (page == Scope) {
    d["radar_list"] = scopeWide;
    d["radar_paused"] = scopeHeld;
    d["radar_row"] = scopeRow;
    d["radar_rows"] = scopeCount;
    d["sweep_frames"] = scopeFrames;
  }
  d["page"] = pageNames[page];
  if (page == Nodes) d["visible_nodes"] = nodeTotal;
  if (page == Home || page == Folder) d["layout"] = config.launcherTiles ? "tiles" : "list";
  if (page == Radio) {
    int p = radiopresets::match(draft);
    d["radio_preset"] = p < 0 ? "custom" : radiopresets::list[p].name;
  }
  d["locked"] = locked;
  d["selected"] = selected;
  if (page == QuickView) {
    d["quick_targets"] = quickTargets;
    d["quick_to"] = meshRadio.idText(quickReplyTo ? quickReplyTo : quickSend::state.to);
    if (!quickTargets && selected > 0) d["quick_index"] = selected - 1;
  }
  d["recipient"] = recipient == meshmesh::Broadcast ? "ALL" : meshRadio.idText(recipient);
  d["composer"] = composer;
  d["composer_bytes"] = composer.length();
  d["editing"] = editing;
  d["chat_offset"] = chatOffset;
  d["idle_seconds"] = (millis() - lastInput) / 1000;
  if (page == Folder) {
    d["folder"] = folderOpen;
    d["folder_name"] = appsFolderName(folderOpen);
  }
  if (appFolder >= 0) d["from_folder"] = appFolder;
  if ((page == Nodes || page == Node) && focusNode) d["selected_node"] = meshRadio.idText(focusNode);
  if (page == Node) {
    d["action"] = action;
  }
  if (page == ChannelInfo) d["channel"] = meshRadio.idText(focusChannel);
  if (page == ChannelAdd) d["add_step"] = int(addStep);
  d["channels"] = meshRadio.channelCount;
  if (page == Scope || page == Homing || page == Motion) {
    d["csi_role"] = radar.csi;
    d["radar_targets"] = radar.count;
    d["radar_selected"] = liveScopeIndex();
    d["radar_sound"] = radarSound;
  }
  String s;
  serializeJson(d, s);
  return s;
}
void uiKey(int key) {
  if (powerOffPending()) return;
  bool asleep = wakeOnly;
  lastPhysicalInput = lastInput = millis();
  hardware.brightness(config.backlight(time(nullptr)));
  wakeOnly = false;
  dirty = true;
  keyed = true; // drawn on the next pass, without the 150 ms gap
  if (locked) { // a hold (timed by the M9 keyboard controller, not by us) or two OK presses within a second unlock
    static uint32_t firstOk = 0;
    bool twice = key == Enter && firstOk && millis() - firstOk < 1000;
    firstOk = key == Enter && !twice ? millis() : 0;
    if (credentialEnabled()) {
      hardware.lastKey = 0;
      if (!asleep) credentialKey(key);
      return;
    }
    if (key == KeyHold || twice) locked = false;
    return;
  }
  if (asleep) return;
  if (key == KeyMic) {
    activePanel = Panel::Closed;
    lockScreen();
    return;
  }
  if (credentialStep != CredentialStep::Idle) {
    credentialKey(key);
    return;
  }
  if (clipMenu) {
    clipboardKey(key);
    return;
  }
  if (key == KeyHold && (editing || page == Chat)) {
    openClipboard();
    return;
  }
  // M9 V1.1 has no MIC key: hold OK locks, but only on the home screen and folders, where a hold is otherwise just OK.
  if (key == KeyHold && !hardware.micKey && (page == Home || page == Folder)) {
    activePanel = Panel::Closed;
    lockScreen();
    return;
  }

  if (editing) {
    if (key == KeyBack) {
      editing = false;
      return;
    }
    if (key == Erase) {
      edit.remove(meshmesh::previousCharacter(edit.c_str(), edit.length()));
      return;
    }
    if (key == KeyRight && page != Network) {
      toggleLastCase(edit);
      return;
    }
    if (key == Enter) {
      if (page == Memo || page == Nodes) {
        saveMemo();
        return;
      }
      if (page == ChannelAdd) {
        channelEditSave();
        return;
      }
      if (page == Remote) {
        remoteSaveEdit();
        return;
      }
      if (page == ChannelInfo) {
        channelRegionSave();
        return;
      }
      if (page == Radio) {
        String r = regions::normalize(edit);
        if (edit.length() && !r.length()) {
          notice(t("Region: letters, digits, '-'"), bad);
          return;
        }
        strlcpy(draft.region, r.c_str(), sizeof draft.region);
        editing = false;
        return;
      }
      if (page == NetList) {
        if (edit.length() < 8) {
          notice(t("Password: 8-63 characters"), bad);
          return;
        }
        if (internet.save(pendingSsid, edit)) {
          editing = false;
          edit = "";
          internet.connectTo(pendingSsid);
          notice(t("Connecting to ") + pendingSsid);
        } else
          notice(netError(), bad);
        return;
      }
      if (page == Network) {
        StaticJsonDocument<128> d;
        d["key"] = edit;
        String r = applySettings(d.as<JsonObjectConst>());
        if (r.startsWith("OK")) editing = false;
        notice(r);
      } else if (edit.length() && edit.length() <= 24 &&
                 meshmesh::validUtf8((const uint8_t*)edit.c_str(), edit.length())) {
        strlcpy(draft.name, edit.c_str(), sizeof draft.name);
        editing = false;
      } else
        notice(t("Name: 1-24 UTF-8 bytes"), bad);
      return;
    }
    if (key >= 32 && key < 127) {
      String ch = String(char(key));
      if (edit.length() + ch.length() <= textByteLimit()) edit += ch;
    }
    return;
  }
  if (activePanel != Panel::Closed && actionPanelKey(key)) return;
  if (key == KeyMenu) {
    openActionPanel();
    return;
  }
  if (page == Spectrum && spectrumKey(key)) return;
  if (page == Node && key != Enter && key != KeyHold) deleteArmed = false;
  if (key == KeyHome || key == KeyMsg || key == KeyMap || key == KeySet)
    appFolder = -1; // straight there, not back to a folder
  if (key == KeyHome) {
    change(Home);
    return;
  }
  if (key == KeyMsg) {
    change(Threads, page == Threads || (page == Chat && chatReturn == Threads) || page == ChannelInfo);
    return;
  }
  if (key == KeyMap) {
    change(Map);
    return;
  }
  if (key == KeySet) {
    change(Settings);
    return;
  }
  if (key == KeyAdv) {
    bool sent = meshRadio.sendHello();
    notice(sent ? t("Node announced") : t("Announcement failed"), sent ? ok : bad);
    return;
  }
  if (key == KeyGps) {
    gpsToggle();
    return;
  }
  if (page == Folder && key == KeyBack) {
    change(Home, true);
    return;
  } // the folder's tile stays selected
  if (page == QuickView && quickKey(key)) return;
  if (page == Remote && remoteKey(key)) return;
  if (page == Scope && ((key == 'l' || key == 'L') || (key == KeyBack && scopeWide))) {
    scopeWide = !scopeWide;
    if (scopeWide && scopeCount) scopeHeld = true;
    return;
  }
  if (page == Scope && (key == 'p' || key == 'P')) {
    scopeHeld = scopeCount > 0 && !scopeHeld;
    return;
  }
  // Unsaved Radio or Display changes: BACK first jumps to Save, a second BACK within 5 s discards them.
  if (key == KeyBack && (page == Radio || page == Display) && draftChanged() &&
      (!discardArmed || millis() - discardArmed > 5000)) {
    discardArmed = millis();
    selected = settingRows() - 1;
    notice(t("Unsaved: OK saves, BACK discards"), warn);
    return;
  }
  if (key == KeyBack) discardArmed = 0;
  if (key == KeyBack) {
    change(page == Memo          ? Node
           : page == ChannelAdd  ? Threads
           : page == ChannelInfo ? infoReturn
           : page == NetList     ? Network
           : page == Homing      ? Scope
           : page == Chat        ? chatReturn
           : page == Node        ? Nodes
           : sharedPage(page)    ? subReturn
                                 : Home,
           true);
    return;
  }

  if (page == ChannelInfo) {
    channelInfoKey(key);
    return;
  }
  if (page == Node && (key == 'n' || key == 'N') && focusedPeer()) {
    change(Memo);
    return;
  }
  if (page == Nodes && (key == 'f' || key == 'F')) {
    edit = tagFilter;
    editing = true;
    return;
  }
  if (page == Memo) {
    if (key == KeyUp || key == KeyDown) selected = 1 - selected;
    if (key == Enter && focusedPeer()) {
      const auto& m = contactMemo(*focusedPeer()).memo;
      edit = selected ? m.tags : m.note;
      editing = true;
    }
    return;
  }
  if (page == Threads && (key == 'u' || key == 'U')) {
    nextUnread();
    return;
  }
  if ((page == Radio || page == Display) && (key == 's' || key == 'S')) {
    if (draftChanged()) saveDraft();
    return;
  }
  if (page == Threads && (key == KeyLeft || key == KeyRight)) {
    threads();
    if (selected < int(conversationCount) && channels::isChannel(conversations[selected]))
      openChannelInfo(conversations[selected], Threads);
    return;
  }
  if (page == Chat) {
    if (key == Enter) {
      if (meshRadio.sendMessage(composer, recipient)) {
        composer = "";
        chatOffset = 0;
        notice(t("Message queued"));
      } else
        notice(t("Cannot send: empty, full queue or radio"), bad);
      return;
    }
    if (key == Erase) {
      composer.remove(meshmesh::previousCharacter(composer.c_str(), composer.length()));
      return;
    }
    if (key == KeyUp) {
      unsigned total = 0;
      for (unsigned i = 0; i < meshRadio.historyCount; i++)
        if (belongs(meshRadio.history[i], recipient)) total++;
      chatOffset = min(chatOffset + 1, max(0, int(total) - 1));
      return;
    }
    if (key == KeyDown) {
      chatOffset = max(0, chatOffset - 1);
      return;
    }
    if (key == KeyRight) {
      if (!composer.length()) {
        quickReplyTo = recipient;
        quickTargets = false;
        change(QuickView);
      } else
        toggleLastCase(composer);
      return;
    }
    if (key == KeyLeft) {
      chatLeft();
      return;
    }
    if (key >= 32 && key < 127) {
      String ch = String(char(key));
      if (composer.length() + ch.length() <= meshRadio.messageLimit(recipient))
        composer += ch;
      else
        notice(t("UTF-8 byte limit: ") + String(meshRadio.messageLimit(recipient)), warn);
    }
    return;
  }
  if (page == NetList && key == Erase) {
    netListErase();
    return;
  }
  if (page == Map) {
    if (key == 'l' || key == 'L') {
      change(Library);
      return;
    }
    if (key == KeyLeft) maps.pan(-70, 0);
    if (key == KeyRight) maps.pan(70, 0);
    if (key == KeyUp) maps.pan(0, -70);
    if (key == KeyDown) maps.pan(0, 70);
    if (key == '+' || key == '=') maps.changeZoom(1);
    if (key == '-' || key == '_') maps.changeZoom(-1);
    if (key == Enter) {
      maps.follow = true;
      if (hardware.gpsFix())
        maps.center(hardware.gps.location.lat(), hardware.gps.location.lng());
      else
        notice(t("Waiting for GPS fix"), warn);
    }
    return;
  }
  if ((page == Nodes || page == Node) && (key == 'p' || key == 'P')) {
    if (Peer* p = focusedPeer()) {
      if (p->position) {
        maps.follow = false;
        maps.center(p->latitude, p->longitude);
        change(Map);
      } else
        notice(t("This node has not shared GPS"), warn);
    }
    return;
  }
  if ((page == Home || page == Folder) && (key == KeyLeft || key == KeyRight || key == KeyUp || key == KeyDown)) {
    uint8_t order[AppEntriesMax];
    int total = page == Folder ? appsShown(order, folderOpen) : appsShown(order);
    if (!total) return;
    selected = constrain(selected, 0, total - 1);
    if (!config.launcherTiles) {
      if (key == KeyUp || key == KeyDown) selected = (selected + (key == KeyUp ? total - 1 : 1)) % total;
      return;
    }
    int rows = (total + tileColumns - 1) / tileColumns, col = selected % tileColumns, row = selected / tileColumns;
    int n = min(tileColumns, total - row * tileColumns);
    if (key == KeyLeft) col = (col + n - 1) % n;
    if (key == KeyRight) col = (col + 1) % n;
    if (key == KeyUp) row = (row + rows - 1) % rows;
    if (key == KeyDown) row = (row + 1) % rows;
    selected = min(row * tileColumns + col, total - 1);
    return;
  }
  if (page == Node && (key == KeyLeft || key == KeyRight || key == KeyUp || key == KeyDown)) {
    Peer* p = focusedPeer();
    if (!p) return;
    NodeAction acts[6];
    int n = nodeActions(*p, acts);
    action = (action + (key == KeyLeft || key == KeyUp ? -1 : 1) + n) % n;
    return;
  }
  if (page == Sensors && (key == KeyLeft || key == KeyRight)) {
    selected = (selected + (key == KeyLeft ? 2 : 1)) % 3;
    return;
  }
  if (page == WardriveView && wardriveKey(key)) return;
  if (page == Scope && (key == KeyUp || key == KeyDown)) {
    if (scopeCount) {
      scopeHeld = true;
      scopeRow = (scopeRow + (key == KeyUp ? -1 : 1) + scopeCount) % scopeCount;
    }
    return;
  }
  if (page == Scope && (key == KeyLeft || key == KeyRight)) {
    change(Motion);
    return;
  }
  if (page == Motion && (key == KeyLeft || key == KeyRight)) {
    change(Scope);
    return;
  }
  if (page == Motion && (key == 'b' || key == 'B')) {
    csiBeaconRole = !csiBeaconRole;
    radar.setCsi(csiBeaconRole ? Radar::CsiBeacon : Radar::CsiSensor);
    notice(csiBeaconRole ? t("This board is now the beacon") : t("This board is now the sensor"));
    return;
  }
  if (page == Homing && (key == KeyLeft || key == KeyRight)) {
    radar.resetPeak();
    notice(t("Peak reset"), dim);
    return;
  }
  if (key == KeyUp || key == KeyDown) {
    if (page == Threads) threads();
    if (page == Nodes) sortNodes();
    int total = page == Threads                    ? conversationCount
                : page == ChannelAdd               ? addRows()
                : page == Nodes                    ? nodeTotal
                : page == Settings                 ? settingsCount
                : page == Radio || page == Display ? settingRows()
                : page == Network                  ? 4
                : page == NetList                  ? 1 + int(netCount())
                : page == Sensors                  ? 3
                : page == Library                  ? int(library.size())
                                                   : 1;
    selected = total ? (selected + (key == KeyUp ? -1 : 1) + total) % total : 0;
    if (page == Threads && conversationCount) focusedThread = conversations[selected];
    if (page == Nodes && nodeTotal) focusNode = meshRadio.peers[nodeOrder[selected]].id;
    return;
  }
  if ((page == Radio || page == Display) && (key == KeyLeft || key == KeyRight)) {
    alter(key == KeyLeft ? -1 : 1);
    return;
  }
  if (key != Enter && key != KeyHold) return;
  if (page == Home || page == Folder) {
    uint8_t order[AppEntriesMax];
    int n = page == Folder ? appsShown(order, folderOpen) : appsShown(order);
    if (selected < 0 || selected >= n) return;
    uint8_t e = order[selected];
    if (e & AppFolder) {
      folderOpen = e & ~AppFolder;
      change(Folder);
    } else
      openApp(e, page == Folder ? folderOpen : -1);
  } else if (page == Library && library.size()) {
    if (maps.selectArea(library[selected]["id"].as<String>()))
      change(Map);
    else
      notice(t("Map unavailable"), bad);
  } else if (page == Threads) {
    threads();
    if (!conversationCount) return;
    recipient = conversations[selected];
    chatReturn = Threads;
    composer = "";
    change(Chat);
  } else if (page == ChannelAdd)
    channelAddEnter();
  else if (page == Nodes && nodeTotal) {
    focusNode = meshRadio.peers[nodeOrder[selected]].id;
    change(Node);
  } else if (page == Node)
    runNodeAction();
  else if (page == Settings && selected == settingsCount - 1) {
    if (!powerArmed || millis() - powerArmed > 5000) {
      powerArmed = millis();
      notice(t("OK again: turn off"), warn);
      return;
    }
    powerArmed = 0;
    powerOff();
  } else if (page == Settings) {
    Page pages[] = {Radio, Display, Sensors, Network, Diagnostics, Help};
    change(pages[selected]);
  } else if (page == Radio || page == Display) {
    if (selected == settingRows() - 1) {
      if (draftChanged())
        saveDraft();
      else
        notice(t("Nothing to save"), dim);
    } else if (page == Display && selected == RowPassword) {
      credentialPrompt(credentialEnabled() ? CredentialStep::Current : CredentialStep::New);
    } else if (page == Display && selected == 0) {
      editing = true;
      edit = draft.name;
    } else if (page == Radio && selected == RowRegion) {
      editing = true;
      edit = draft.region;
    } else if (page == Radio && selected == RowPrev) {
      if (!config.trial.set() && !config.previous.set()) {
        notice(t("No previous settings"), dim);
        return;
      }
      String r = executeCommand(config.trial.set() ? "radio keep" : "radio previous");
      bool done = r.startsWith("OK");
      if (done) draft = config;
      notice(done                             ? t("Radio settings changed")
             : r.startsWith("ERR radio busy") ? t("Radio busy, retry")
                                              : r.substring(4),
             done ? ok : bad);
    } else if (page == Radio && selected == RowNet)
      netAction();
    else if (page == Radio && selected == RowFind) {
      if (!regions::find()) notice(t("Radio offline"), bad);
    } else
      alter(1);
  } else if (page == Network) {
    if (selected == 0) portalToggle();
    if (selected == 1) {
      change(NetList);
    }
    if (selected == 2) bleToggle();
    if (selected == 3) notice(t("Public key: ") + meshRadio.publicKeyText().substring(0, 16));
  } else if (page == Sensors) {
    if (selected == 0)
      gpsToggle();
    else if (selected == 1) {
      bool sent = meshRadio.sendPosition();
      notice(sent ? t("Position shared") : t("Waiting for GPS fix"), sent ? ok : warn);
    } else if (navigation.calibrating) {
      bool saved = navigation.finish();
      notice(saved ? t("Calibration saved") : t("Rotate wider, at least 20 seconds"), saved ? ok : warn);
    } else {
      navigation.start();
      notice(t("Rotate in all directions"));
    }
  } else if (page == NetList)
    netListEnter();
  else if (page == Scope) {
    int i = liveScopeIndex();
    if (i >= 0 && radar.track(i)) {
      scopeHeld = true;
      pingedSamples = radar.samples;
      change(Homing);
    } else
      notice(scopeCount ? t("Signal no longer listed") : t("No signal selected"), warn);
  } else if (page == Motion) {
    if (radar.csi == Radar::CsiSensor && radar.beaconHeard()) {
      radar.calibrate();
      notice(t("Calibrating: keep the area still for 10 s"), info);
    } else
      notice(t("Needs a heard beacon"), warn);
  } else if (page == Homing) {
    radarSound = !radarSound;
    notice(radarSound ? (config.sound ? t("Ping on") : t("Device sound is off in settings")) : t("Ping off"),
           radarSound && !config.sound ? warn : dim);
  } else if (page == Diagnostics && !hardware.fsOk) { // storage left by another firmware: create ours after a second OK
    static uint32_t armed = 0;
    if (!armed || millis() - armed > 5000) {
      armed = millis();
      notice(t("OK again: erase old data"), warn);
      return;
    }
    armed = 0;
    String r = executeCommand("fsformat");
    bool done = r.startsWith("OK");
    if (done) executeCommand("restart");
    notice(done ? t("Storage created, restart") : r, done ? ok : bad);
  } else if (page == Diagnostics) {
    hardware.beep();
    bool passed = meshRadio.selfTest();
    notice(passed ? t("Encryption test passed") : t("Encryption test failed"), passed ? ok : bad);
  }
}
// Touch: the title bar goes back, footer hints are their keys, a tap on a list item or tile opens it
// (settings rows, the mode choice: the first tap selects)
// takes a tap as the cursor and OK; swipes are the arrows and a hold is "hold OK", as on the ball.
void uiTouch(char gesture, int x, int y) {
  if (locked || wakeOnly) {
    uiKey(gesture == 'h' ? KeyHold : 0);
    return;
  } // a tap only wakes; a hold unlocks
  if (credentialStep != CredentialStep::Idle) {
    if (gesture == 't' && y < 21)
      uiKey(KeyBack);
    else if (gesture == 't' && y >= 221) {
      for (unsigned i = 0; i < hintCount; i++)
        if (x >= hintSpots[i].x0 && x < hintSpots[i].x1) uiKey(hintSpots[i].key);
    }
    return;
  }
  if (gesture == 'h') {
    uiKey(KeyHold);
    return;
  }
  if (gesture != 't') {
    uiKey(gesture == 'u' ? KeyUp : gesture == 'd' ? KeyDown : gesture == 'l' ? KeyLeft : KeyRight);
    return;
  }
  if (dirty) return; // the screen has not caught up with the last action: the tap was aimed at the old one
  if (y < 21) {
    uiKey(KeyBack);
    return;
  }
  if (y >= 221) {
    for (unsigned i = 0; i < hintCount; i++)
      if (x >= hintSpots[i].x0 && x < hintSpots[i].x1) {
        realErase = hintSpots[i].key == Erase;
        uiKey(hintSpots[i].key);
        return;
      }
    return;
  }
  if (activePanel != Panel::Closed) {
    for (unsigned i = 0; i < targetCount; i++) {
      const auto& v = targets[i];
      if (x >= v.x && x < v.x + v.w && y >= v.y && y < v.y + v.h) {
        panelRow = v.index;
        uiKey(Enter);
        break;
      }
    }
    return;
  }
  if (clipMenu) {
    for (unsigned i = 0; i < targetCount; i++) {
      const auto& v = targets[i];
      if (x >= v.x && x < v.x + v.w && y >= v.y && y < v.y + v.h) {
        clipAction = ClipAction(v.index);
        uiKey(Enter);
        break;
      }
    }
    return;
  }
  for (unsigned i = 0; i < targetCount; i++) {
    auto& v = targets[i];
    if (x < v.x || x >= v.x + v.w || y < v.y || y >= v.y + v.h) continue;
    if (v.index < 0) {
      uiKey(-v.index);
      return;
    }
    if (page == Scope && v.index < scopeCount) {
      bool open = scopeHeld && scopeRow == v.index;
      scopeHeld = true;
      scopeRow = v.index;
      if (open)
        uiKey(Enter);
      else {
        dirty = true;
        lastInput = lastPhysicalInput = millis();
      }
      return;
    }
    if (page == Node) {
      action = v.index;
      uiKey(Enter);
      return;
    }
    bool confirm = page == Radio || page == Display;
    if (confirm && selected != v.index) {
      selected = v.index;
      dirty = true;
      lastInput = millis();
      return;
    }
    selected = v.index;
    if (page == Threads && conversationCount) focusedThread = conversations[selected];
    if (page == Nodes && nodeTotal) focusNode = meshRadio.peers[nodeOrder[selected]].id;
    uiKey(Enter);
    return;
  }
}
String eventLabel(const String& value) {
  if (value.startsWith("New message from ")) return tr("New message from ") + value.substring(17);
  if (value.startsWith("Delivered to ")) return tr("Delivered to ") + value.substring(13);
  if (value == "Queued: waiting for delivery") return tr("Queued: waiting for delivery");
  if (value == "Queued: broadcast") return tr("Queued: broadcast");
  if (value == "No delivery ACK") return tr("No delivery ACK");
  if (config.lang != LangEn && (value.startsWith("Radio TX error") || value.startsWith("TX failed")))
    return tr("Radio TX error");
  return value;
}
void uiTick() {
  uint32_t now = millis();
  notifications::Event notification;
  while (notifications::take(notification)) {
    if (notification.kind == notifications::Incoming) {
      if (notifications::sound(notification.message, notification.mention)) hardware.beep();
      bool muted = notifications::muted(notification.message);
      if (!muted && notifications::enabled(notifications::mode(notification.message, config.notifyWake, 1),
                                           notification.mention)) {
        wakeOnly = false;
        hardware.brightness(config.backlight(time(nullptr)));
        lastInput = now;
      }
      if (!muted && notifications::enabled(notifications::mode(notification.message, config.notifyPopup, 2),
                                           notification.mention))
        notice(t("New message from ") + notification.message.name, accent);
      dirty = true;
    } else if (config.notifyFailed &&
               (notification.kind == notifications::TxError || notification.kind == notifications::Unconfirmed)) {
      notice(notification.kind == notifications::TxError             ? t("Radio TX error")
             : channels::isChannel(notification.message.destination) ? t("No repeat heard")
                                                                     : t("No delivery ACK"),
             warn);
    }
  }

  if (powerOffPending()) {
    static bool shown = false;
    if (!shown) {
      shown = true;
      wakeOnly = locked = false;
      hardware.brightness(config.backlight(time(nullptr)));
      draw();
    }
    return;
  } // drawn once: the board turns off
  // Homing is used while walking without pressing keys: the screen stays on and unlocked.
  if ((page == Homing || page == Motion) && !locked) lastInput = now;
  // Keep a user-started sweep visible. A configured credential still locks on physical inactivity.
  if (page == Spectrum && spectrum::running && !locked && activePanel == Panel::Closed) lastInput = now;
  // Motion start beeps once (at most every 5 s).
  if (page == Motion && radar.moving && !wasMoving && now - motionBeepAt > 5000) {
    motionBeepAt = now;
    hardware.ping(1800, 120);
  }
  wasMoving = page == Motion && radar.moving;
  if (config.autoLock && now - (credentialEnabled() ? lastPhysicalInput : lastInput) >= config.autoLock * 1000UL &&
      !locked) {
    lockScreen();
    dirty = true;
  }
  if (config.dimAfter && now - lastInput >= config.dimAfter * 1000UL) {
    hardware.brightness(0);
    wakeOnly = true;
  }
  // Ping: faster and higher as the signal strengthens (-85..-30 dBm, steeper when close); LoRa pings
  // once per new packet. A lost target gets a quiet low tick every 2 s instead of a stale ping.
  if (page == Homing && !locked && radarSound && radar.tracking) {
    float x = constrain((radar.fast + 85) / 55.f, 0.f, 1.f);
    if (!radarFresh()) {
      if (radar.samples && now - pingAt >= 2000) {
        pingAt = now;
        hardware.ping(300, 30);
      }
    } else if (radar.focus.kind == RadarTarget::Lora) {
      if (radar.samples != pingedSamples) {
        pingedSamples = radar.samples;
        hardware.ping(600 + 1000 * x, 60);
      }
    } else if (now - pingAt >= uint32_t(1200 - 1140 * x * x)) {
      pingAt = now;
      hardware.ping(600 + 1000 * x, 40);
    }
  }
  if (meshRadio.event != eventSeen) {
    eventSeen = meshRadio.event;
    bool incoming = eventSeen.startsWith("New message from ");
    if (page == Chat && !incoming) {
      markRead();
      notice(eventLabel(eventSeen), eventSeen.startsWith("Delivered")                                         ? ok
                                    : eventSeen.startsWith("No delivery") || eventSeen.startsWith("Radio TX") ? bad
                                                                                                              : accent);
    }
  }
  if (toast.length() && now - toastAt >= 3500) {
    toast = "";
    dirty = true;
  }
  // The HUD clock, signal and battery change on every page.
  bool animate = !locked && (page == Homing || page == Motion); // the sweep beam moves every frame
  // The network list refreshes itself while open; a scan briefly pauses traffic.
  if (page == NetList && !locked && internet.enabled && (!internet.scannedAt || now - internet.scannedAt > 20000) &&
      now - netScanAt > 20000) {
    netScanAt = now;
    internet.rescan();
  }
  // A dark screen is not drawn: a key lights it and redraws (and light sleep stays short).
  bool scope = page == Scope && !locked && activePanel == Panel::Closed;
  bool freshList = scope && !scopeHeld && radar.dirty && now - lastDraw >= 500;
  if (page == Spectrum) {
    if (locked || wakeOnly || activePanel != Panel::Closed)
      stopSweep();
    else if (advanceSpectrum(now))
      dirty = true;
  }
  bool update =
      dirty || meshRadio.dirty || maps.dirty || internet.dirty || animate || freshList || now - lastDraw >= 1000;
  if (wakeOnly) return;
  // A key or touch draws at once; animations run at up to 20 fps; radio, map and network updates every 150 ms.
  uint32_t gap = keyed && dirty ? 0 : animate ? 50 : 150;
  if (update && now - lastDraw >= gap) {
    draw();
    lastDraw = now;
    dirty = false;
    keyed = false;
    meshRadio.dirty = false;
    maps.dirty = false;
    radar.dirty = false;
    internet.dirty = false;
  } else if (scope && !scopeWide && !dirty && !toast.length() && now - scopeFrameAt >= 60) {
    drawSweep();
    hardware.flushRect(ScopeX - ScopeRadius, ScopeY - ScopeRadius, 2 * ScopeRadius + 1, 2 * ScopeRadius + 1);
  }
}
