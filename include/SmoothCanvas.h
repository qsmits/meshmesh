// SPDX-License-Identifier: MIT
#pragma once
#include <Adafruit_GFX.h>
#include <algorithm>
#include <cmath>

// Coverage is composited directly into the unrotated M9/T-Deck framebuffer.
inline uint16_t blend565(uint16_t dst, uint16_t src, unsigned alpha) {
  unsigned inverse = 15 - alpha;
  unsigned red = ((dst >> 11) * inverse + (src >> 11) * alpha + 7) / 15;
  unsigned green = (((dst >> 5) & 63) * inverse + ((src >> 5) & 63) * alpha + 7) / 15;
  unsigned blue = ((dst & 31) * inverse + (src & 31) * alpha + 7) / 15;
  return (red << 11) | (green << 5) | blue;
}

class SmoothCanvas {
  GFXcanvas16& canvas;

  void pixel(int x, int y, uint16_t color, int alpha) {
    if (!alpha || x < 0 || y < 0 || x >= canvas.width() || y >= canvas.height()) return;
    auto& dst = getBuffer()[y * canvas.width() + x];
    dst = alpha == 15 ? color : blend565(dst, color, alpha);
  }
  static int coverage(float distanceInside) {
    if (distanceInside >= 0.5f) return 15;
    if (distanceInside <= -0.5f) return 0;
    return int(8 + 15 * distanceInside);
  }

  // Draw horizontal spans in the interior. Only pixels near a curved edge
  // need a distance calculation. Outlines preserve the surface inside them.
  void rounded(int x, int y, int w, int h, float radius, int stroke, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    float r = std::max(0.f, std::min(radius, 0.5f * std::min(w, h)));
    int corners = int(ceilf(r - 0.5f));
    for (int row = std::max(0, -y); row < std::min<int>(h, canvas.height() - y); ++row) {
      float top = std::min(row, h - 1 - row) + 0.5f;
      bool solidMiddle = !stroke || top < stroke;
      float dy = std::max(0.f, r - top);
      int col = 0;
      if (dy > 0) {
        float outside = r + 0.5f;
        col = std::max(0, int(r - sqrtf(outside * outside - dy * dy) - 0.5f));
      }
      for (; col < corners; ++col) {
        float dx = r - col - 0.5f;
        float inside = r - sqrtf(dx * dx + dy * dy);
        int outer = coverage(inside);
        int alpha = outer - (stroke ? coverage(inside - stroke) : 0);
        if ((solidMiddle && alpha == 15) || (!solidMiddle && outer == 15 && !alpha)) break;
        pixel(x + col, y + row, c, alpha);
        pixel(x + w - 1 - col, y + row, c, alpha);
      }
      if (solidMiddle && w > 2 * col) canvas.drawFastHLine(x + col, y + row, w - 2 * col, c);
    }
  }

public:
  explicit SmoothCanvas(GFXcanvas16& target) : canvas(target) {}
  void coveredPixel(int x, int y, uint16_t c, int alpha) { pixel(x, y, c, alpha); }
  uint16_t* getBuffer() { return canvas.getBuffer(); }
  void fillScreen(uint16_t c) { canvas.fillScreen(c); }
  void drawPixel(int x, int y, uint16_t c) { canvas.drawPixel(x, y, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) { canvas.fillRect(x, y, w, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) { canvas.drawRect(x, y, w, h, c); }
  void drawFastVLine(int x, int y, int h, uint16_t c) { canvas.drawFastVLine(x, y, h, c); }
  void drawFastHLine(int x, int y, int w, uint16_t c) { canvas.drawFastHLine(x, y, w, c); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { rounded(x, y, w, h, r, 0, c); }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t c, int stroke = 1) {
    // Straight sides can extend past the radius when the outline is thick.
    if (r < stroke) r = stroke;
    rounded(x, y, w, h, r, stroke, c);
  }
  void fillCircle(int x, int y, int r, uint16_t c) { rounded(x - r, y - r, 2 * r + 1, 2 * r + 1, r + 0.5f, 0, c); }
  void drawCircle(int x, int y, int r, uint16_t c) { rounded(x - r, y - r, 2 * r + 1, 2 * r + 1, r + 0.5f, 1, c); }

  // Walk the major axis of a stroke. Its bounding box can be much larger
  // than the few pixels which actually intersect the line on each step.
  void strokeLine(float x0, float y0, float x1, float y1, float width, uint16_t c) {
    bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);
    if (steep) {
      std::swap(x0, y0);
      std::swap(x1, y1);
    }
    if (x1 < x0) {
      std::swap(x0, x1);
      std::swap(y0, y1);
    }
    float dx = x1 - x0, dy = y1 - y0, length = sqrtf(dx * dx + dy * dy);
    float slope = dx ? dy / dx : 0, radius = width * 0.5f, bound = (radius + 0.5f) * 1.415f;
    float ux = length ? dx / length : 0, uy = length ? dy / length : 0;
    for (int x = int(ceilf(x0 - radius)); x <= int(floorf(x1 + radius)); ++x) {
      float mid = y0 + slope * (std::max(x0, std::min(x1, float(x))) - x0);
      for (int y = int(ceilf(mid - bound)); y <= int(floorf(mid + bound)); ++y) {
        float px = x - x0, py = y - y0, along = px * ux + py * uy;
        float distance;
        if (along <= 0)
          distance = sqrtf(px * px + py * py);
        else if (along >= length)
          distance = sqrtf((x - x1) * (x - x1) + (y - y1) * (y - y1));
        else
          distance = fabsf(py * ux - px * uy);
        pixel(steep ? y : x, steep ? x : y, c, coverage(radius - distance));
      }
    }
  }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    if (x0 == x1 || y0 == y1)
      canvas.drawLine(x0, y0, x1, y1, c);
    else
      strokeLine(x0, y0, x1, y1, 1, c);
  }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    drawLine(x0, y0, x1, y1, c);
    drawLine(x1, y1, x2, y2, c);
    drawLine(x2, y2, x0, y0, c);
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    // Four horizontal slices measure coverage at steep as well as shallow edges.
    int xs[] = {x0, x1, x2}, ys[] = {y0, y1, y2};
    int top = std::max(0, std::min({y0, y1, y2})), bottom = std::min<int>(canvas.height() - 1, std::max({y0, y1, y2}));
    for (int y = top; y <= bottom; ++y) {
      float left[4], right[4];
      float first = canvas.width(), last = -1, innerLeft = -1, innerRight = canvas.width();
      for (int s = 0; s < 4; ++s) {
        float slice = y + (s - 1.5f) * 0.25f;
        left[s] = canvas.width();
        right[s] = -1;
        for (int i = 0; i < 3; ++i) {
          int j = (i + 1) % 3;
          if ((ys[i] > slice) == (ys[j] > slice)) continue;
          float hit = xs[i] + (slice - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
          left[s] = std::min(left[s], hit);
          right[s] = std::max(right[s], hit);
        }
        first = std::min(first, left[s]);
        last = std::max(last, right[s]);
        innerLeft = std::max(innerLeft, left[s]);
        innerRight = std::min(innerRight, right[s]);
      }
      for (int x = std::max(0, int(ceilf(first - 0.5f))); x <= std::min<int>(canvas.width() - 1, floorf(last + 0.5f));
           ++x) {
        if (x - 0.5f >= innerLeft && x + 0.5f <= innerRight) {
          int end = std::min<int>(canvas.width() - 1, floorf(innerRight - 0.5f));
          drawFastHLine(x, y, end - x + 1, c);
          x = end;
        } else {
          float sum = 0;
          for (int s = 0; s < 4; ++s) sum += std::max(0.f, std::min(x + 0.5f, right[s]) - std::max(x - 0.5f, left[s]));
          pixel(x, y, c, int(sum * 3.75f + 0.5f));
        }
      }
    }
  }
};
