#include "UiIcons.h"
#include "SmoothCanvas.h"
#include <Arduino.h>
#include <math.h>
#include <initializer_list>
namespace {
void arc(Adafruit_GFX& d, int cx, int cy, int r, int a0, int a1, uint16_t c) {
  for (int a = a0; a <= a1; a += 3) {
    float rad = a * M_PI / 180;
    d.drawPixel(cx + roundf(r * sinf(rad)), cy - roundf(r * cosf(rad)), c);
  }
}
void thick(Adafruit_GFX& d, int x0, int y0, int x1, int y1, uint16_t c) {
  d.drawLine(x0, y0, x1, y1, c);
  d.drawLine(x0 + 1, y0, x1 + 1, y1, c);
}
// Distance to a circular arc, with round caps at the two ends. Pixel centres
// on the included sweep use radial distance, including sweeps over 180°.
void arc(SmoothCanvas& d, int cx, int cy, int r, int a0, int a1, uint16_t c) {
  if (r <= 0) return;
  float start = a0 * float(M_PI) / 180, end = a1 * float(M_PI) / 180;
  float sx = sinf(start), sy = -cosf(start), ex = sinf(end), ey = -cosf(end);
  for (int y = -r - 1; y <= r + 1; ++y)
    for (int x = -r - 1; x <= r + 1; ++x) {
      int squared = x * x + y * y;
      if (squared <= (r - 1) * (r - 1) || squared >= (r + 1) * (r + 1)) continue;
      bool after = sx * y - sy * x >= 0, before = x * ey - y * ex >= 0;
      bool inSweep = a1 - a0 > 180 ? after || before : after && before;
      float distance = inSweep ? fabsf(sqrtf(float(squared)) - r)
                               : std::min(hypotf(x - r * sx, y - r * sy), hypotf(x - r * ex, y - r * ey));
      if (distance < 1) d.coveredPixel(cx + x, cy + y, c, int((1 - distance) * 15 + 0.5f));
    }
}
void thick(SmoothCanvas& d, int x0, int y0, int x1, int y1, uint16_t c) {
  d.strokeLine(x0, y0, x1, y1, 1.75f, c);
}
} // namespace
template <class Canvas>
static void paintIcon(Canvas& d, Icon id, int cx, int cy, int s, uint16_t c, uint16_t hole, uint16_t shade) {
  int h = s / 2, q = max(1, s / 4), w = max(1, s / 8);
  switch (id) {
  case IcChat:
    d.fillRoundRect(cx - s, cy - s * 3 / 4, 2 * s, s * 3 / 2, max(2, s / 3), c);
    d.fillTriangle(cx - h, cy + s * 3 / 4 - 1, cx, cy + s * 3 / 4 - 1, cx - s * 3 / 4, cy + s, c);
    for (int k = -1; k <= 1; k++) d.fillCircle(cx + k * h, cy, w, hole);
    break;
  case IcPin:
    d.fillCircle(cx, cy - q, s * 5 / 8, c);
    d.fillTriangle(cx - s * 9 / 16, cy, cx + s * 9 / 16, cy, cx, cy + s, c);
    d.fillCircle(cx, cy - q, max(1, s / 4), hole);
    break;
  case IcMesh: {
    int ax = cx, ay = cy - s * 3 / 4, bx = cx - s * 3 / 4, by = cy + s * 5 / 8, ex = cx + s * 3 / 4;
    thick(d, ax, ay, bx, by, c);
    thick(d, ax, ay, ex, by, c);
    thick(d, bx, by, ex, by, c);
    for (int p : {0, 1, 2}) d.fillCircle(p == 0 ? ax : p == 1 ? bx : ex, p == 0 ? ay : by, max(2, s / 4), c);
    break;
  }
  case IcCompass:
    d.drawCircle(cx, cy, s, c);
    if (s > 7) d.drawCircle(cx, cy, s - 1, c);
    d.fillTriangle(cx, cy - s * 3 / 4, cx - max(2, s / 4), cy, cx + max(2, s / 4), cy, c);
    d.fillTriangle(cx, cy + s * 3 / 4, cx - max(2, s / 4), cy, cx + max(2, s / 4), cy, shade);
    break;
  case IcWifi: {
    int oy = cy + s * 5 / 8;
    for (int r : {s, s * 2 / 3, s / 3}) {
      arc(d, cx, oy, r, -45, 45, c);
      arc(d, cx, oy, r - 1, -45, 45, c);
    }
    d.fillCircle(cx, oy, max(1, s / 6), c);
    break;
  }
  case IcBle:
    d.drawLine(cx, cy - s, cx, cy + s, c);
    d.drawLine(cx, cy - s, cx + h, cy - h, c);
    d.drawLine(cx + h, cy - h, cx - h, cy + h, c);
    d.drawLine(cx, cy + s, cx + h, cy + h, c);
    d.drawLine(cx + h, cy + h, cx - h, cy - h, c);
    if (s > 6) {
      d.drawLine(cx + 1, cy - s, cx + 1, cy + s, c);
    }
    break;
  case IcGear:
    d.fillCircle(cx, cy, s * 5 / 8, c);
    for (int k = 0; k < 8; k++) {
      float a = k * M_PI / 4;
      d.fillCircle(cx + roundf(s * .8f * cosf(a)), cy + roundf(s * .8f * sinf(a)), max(1, s / 4), c);
    }
    d.fillCircle(cx, cy, max(1, s / 4), hole);
    break;
  case IcTower:
    d.drawLine(cx, cy - q, cx - h, cy + s, c);
    d.drawLine(cx, cy - q, cx + h, cy + s, c);
    d.drawLine(cx, cy - q, cx, cy + s, c);
    d.fillCircle(cx, cy - h, max(1, s / 5), c);
    arc(d, cx, cy - h, h + 1, -75, -30, c);
    arc(d, cx, cy - h, h + 1, 30, 75, c);
    arc(d, cx, cy - h, s - 1, -75, -35, c);
    arc(d, cx, cy - h, s - 1, 35, 75, c);
    break;
  case IcRoom:
    d.fillTriangle(cx - s, cy, cx + s, cy, cx, cy - s, c);
    d.fillRect(cx - s * 3 / 4, cy, s * 3 / 2, s, c);
    d.fillRect(cx - max(1, s / 5), cy + h - 1, max(2, s * 2 / 5), s - h + 1, hole);
    break;
  case IcSensor:
    d.fillRoundRect(cx - max(1, s / 5), cy - s, max(3, s * 2 / 5), s * 3 / 2, max(1, s / 5), c);
    d.fillCircle(cx, cy + h + q / 2, max(2, s * 2 / 5), c);
    break;
  case IcPerson:
    d.fillCircle(cx, cy - h + 1, max(2, s * 3 / 8), c);
    d.fillRoundRect(cx - s * 5 / 8, cy + 1, s * 5 / 4, s, max(2, s / 2), c);
    break;
  case IcLock:
    arc(d, cx, cy - q, max(2, s * 3 / 8), -90, 90, c);
    arc(d, cx, cy - q, max(2, s * 3 / 8) - 1, -90, 90, c);
    d.drawFastVLine(cx - max(2, s * 3 / 8), cy - q, q + 1, c);
    d.drawFastVLine(cx + max(2, s * 3 / 8), cy - q, q + 1, c);
    d.fillRoundRect(cx - s * 5 / 8, cy, s * 5 / 4, s * 7 / 8, max(1, s / 5), c);
    break;
  case IcMail:
    d.drawRect(cx - s, cy - s * 5 / 8, 2 * s, s * 5 / 4, c);
    d.drawLine(cx - s, cy - s * 5 / 8, cx, cy + q / 2, c);
    d.drawLine(cx + s - 1, cy - s * 5 / 8, cx, cy + q / 2, c);
    break;
  case IcHash:
    thick(d, cx - q, cy - s, cx - h, cy + s, c);
    thick(d, cx + h, cy - s, cx + q, cy + s, c);
    d.fillRect(cx - s, cy - q - 1, 2 * s, 2, c);
    d.fillRect(cx - s, cy + q - 1, 2 * s, 2, c);
    break;
  case IcPulse: {
    int p[][2] = {{-s, 0}, {-h, 0}, {-q, -s * 3 / 4}, {q, s * 3 / 4}, {h, 0}, {s, 0}};
    for (int k = 0; k < 5; k++) thick(d, cx + p[k][0], cy + p[k][1], cx + p[k + 1][0], cy + p[k + 1][1], c);
    break;
  }
  case IcHelp: d.drawCircle(cx, cy, s, c); break; // the caller writes "?" in its font
  case IcRadio:
    d.drawFastVLine(cx, cy - h, s + h, c);
    d.fillCircle(cx, cy - h, max(1, s / 5), c);
    arc(d, cx, cy - h, h + 1, -120, -60, c);
    arc(d, cx, cy - h, h + 1, 60, 120, c);
    arc(d, cx, cy - h, s, -125, -55, c);
    arc(d, cx, cy - h, s, 55, 125, c);
    break;
  case IcRadar:
    d.drawCircle(cx, cy, s, c);
    d.drawCircle(cx, cy, max(2, s / 2), c);
    thick(d, cx, cy, cx + roundf(s * .7f) - 1, cy - roundf(s * .7f) + 1, c);
    d.fillCircle(cx, cy, max(1, s / 6), c);
    d.fillCircle(cx - h, cy + q + 1, max(1, s / 6), c);
    break;
  case IcScreen:
    d.drawRoundRect(cx - s, cy - s * 5 / 8, 2 * s, s * 5 / 4, 2, c);
    d.fillRect(cx - s + 3, cy - s * 5 / 8 + 3, 2 * s - 6, s * 5 / 4 - 6, c);
    break;
  case IcSend: {
    int ax = cx - s, ay = cy - s / 8, bx = cx + s, by = cy - s * 15 / 16, ex = cx + s / 5, ey = cy + s, fx = cx - s / 6,
        fy = cy + s / 4; // a paper plane, its fold cut in
    d.fillTriangle(ax, ay, bx, by, fx, fy, c);
    d.fillTriangle(bx, by, ex, ey, fx, fy, c);
    if (s > 5) d.drawLine(fx, fy, bx - 1, by + 1, hole);
    break;
  }
  case IcFolder:
    d.fillRoundRect(cx - s, cy - s * 5 / 8, s * 3 / 4 + 1, s / 2, max(1, s / 6), c);
    d.fillRoundRect(cx - s, cy - s * 3 / 8, 2 * s, s * 9 / 8, max(1, s / 5), c);
    d.drawFastHLine(cx - s + 1, cy - s * 3 / 8 + max(1, s / 6), 2 * s - 2, hole);
    break;
  case IcKey:
    d.drawCircle(cx - h, cy, max(2, s * 3 / 8), c);
    d.drawFastHLine(cx - h + s * 3 / 8, cy, s + 1, c);
    d.drawFastVLine(cx + h, cy, max(2, s / 3), c);
    d.drawFastVLine(cx + s - 1, cy, max(2, s / 3), c);
    break;
  default: break;
  }
}
namespace {
template <class Canvas> void polygon(Canvas& d, int cx, int cy, int r, int n, uint16_t c) {
  int xp = 0, yp = 0;
  for (int k = 0; k <= n; k++) {
    float a = -M_PI / 2 + k * 2 * M_PI / n;
    int x = cx + roundf(r * cosf(a)), y = cy + roundf(r * sinf(a));
    if (k) d.fillTriangle(cx, cy, xp, yp, x, y, c);
    xp = x;
    yp = y;
  }
}
} // namespace
void drawIcon(Adafruit_GFX& d, Icon id, int cx, int cy, int s, uint16_t c, uint16_t hole, uint16_t shade) {
  paintIcon(d, id, cx, cy, s, c, hole, shade);
}

void drawIcon(SmoothCanvas& d, Icon id, int cx, int cy, int s, uint16_t c, uint16_t hole, uint16_t shade) {
  // Optical adjustments for the small LCD icons. Join the tail well inside
  // the bubble and leave enough room between cutouts at status-bar sizes.
  int half = s / 2;
  switch (id) {
  case IcChat: {
    int left = cx - s, bottom = cy + half;
    d.fillTriangle(left + 3, bottom - 3, left + s + 1, bottom - 3, left + 3, bottom + std::max(2, s / 2), c);
    d.fillRoundRect(left, cy - s * 3 / 4, 2 * s + 1, bottom - (cy - s * 3 / 4) + 1, std::max(2, s / 3), c);
    for (int k = -1; k <= 1; ++k) d.fillCircle(cx + k * half, cy, std::max(1, s / 9), hole);
    break;
  }
  case IcMesh: {
    int top = cy - s * 2 / 3, bottom = cy + s * 2 / 3, left = cx - s * 3 / 4, right = cx + s * 3 / 4;
    d.strokeLine(cx, top, left, bottom, 1.5f, c);
    d.strokeLine(cx, top, right, bottom, 1.5f, c);
    d.strokeLine(left, bottom, right, bottom, 1.5f, c);
    int radius = std::max(2, s / 4);
    for (int i = 0; i < 3; ++i) {
      int x = i == 0 ? cx : i == 1 ? left : right, y = i == 0 ? top : bottom;
      d.fillCircle(x, y, radius, c);
      if (s >= 9) d.fillCircle(x, y, std::max(0, radius - 2), hole);
    }
    break;
  }
  case IcCompass: {
    d.drawCircle(cx, cy, s, c);
    int tip = std::max(2, s / 2), waist = std::max(1, s / 5);
    d.fillTriangle(cx - tip, cy + tip, cx - waist, cy - waist, cx + waist, cy + waist, shade);
    d.fillTriangle(cx + tip, cy - tip, cx - waist, cy - waist, cx + waist, cy + waist, c);
    break;
  }
  default: paintIcon(d, id, cx, cy, s, c, hole, shade); break;
  }
}
