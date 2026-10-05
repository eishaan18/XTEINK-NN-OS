#include "NeuronDraw.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace neuron {

namespace {

// Segment bits: a b c d e f g (top, top-right, bottom-right, bottom,
// bottom-left, top-left, middle).
constexpr uint8_t SEGMENTS[10] = {
    0b1111110,  // 0
    0b0110000,  // 1
    0b1101101,  // 2
    0b1111001,  // 3
    0b0110011,  // 4
    0b1011011,  // 5
    0b1011111,  // 6
    0b1110000,  // 7
    0b1111111,  // 8
    0b1111011,  // 9
};

int digitWidth(const int h) { return h * 11 / 20; }
int thickness(const int h) { return std::max(3, h / 7); }
int gap(const int h) { return std::max(4, h / 6); }
int colonWidth(const int h) { return thickness(h) + gap(h) / 2; }

int glyphWidth(const char c, const int h) {
  if (c >= '0' && c <= '9') return digitWidth(h);
  if (c == ':') return colonWidth(h);
  if (c == ' ') return digitWidth(h) / 2;
  if (c == '-') return digitWidth(h);
  return 0;
}

void drawDigit(const GfxRenderer& r, const int x, const int y, const int h, const uint8_t mask, const bool black) {
  const int w = digitWidth(h);
  const int t = thickness(h);
  const int half = h / 2;
  const int inset = std::max(1, t / 4);  // tiny bevel between segments
  const auto on = [mask](int bit) { return (mask >> (6 - bit)) & 1; };
  // a
  if (on(0)) r.fillRect(x + inset, y, w - 2 * inset, t, black);
  // b
  if (on(1)) r.fillRect(x + w - t, y + inset, t, half - inset, black);
  // c
  if (on(2)) r.fillRect(x + w - t, y + half + inset / 2, t, h - half - inset, black);
  // d
  if (on(3)) r.fillRect(x + inset, y + h - t, w - 2 * inset, t, black);
  // e
  if (on(4)) r.fillRect(x, y + half + inset / 2, t, h - half - inset, black);
  // f
  if (on(5)) r.fillRect(x, y + inset, t, half - inset, black);
  // g
  if (on(6)) r.fillRect(x + inset, y + half - t / 2, w - 2 * inset, t, black);
}

// GfxRenderer's thick drawLine stacks copies vertically, which leaves steep
// lines one pixel wide. Offset across the line's minor axis instead.
void thickLine(const GfxRenderer& r, const int x1, const int y1, const int x2, const int y2, const int lw,
               const bool black = true) {
  const bool steep = std::abs(y2 - y1) > std::abs(x2 - x1);
  const int start = -(lw / 2);
  for (int i = start; i < start + lw; ++i) {
    if (steep) {
      r.drawLine(x1 + i, y1, x2 + i, y2, black);
    } else {
      r.drawLine(x1, y1 + i, x2, y2 + i, black);
    }
  }
}

void cloud(const GfxRenderer& r, const int x, const int y, const int w, const int h, const bool black) {
  // Base pill plus two puffs.
  const int baseH = h * 11 / 20;
  r.fillRoundedRect(x, y + h - baseH, w, baseH, baseH / 2, black ? Color::Black : Color::White);
  fillCircle(r, x + w / 6, y + h / 4, w * 9 / 20, black);
  fillCircle(r, x + w / 2, y + h / 2 - h / 8, w * 3 / 10, black);
}

void sun(const GfxRenderer& r, const int cx, const int cy, const int radius) {
  const int lw = std::max(2, radius / 5);
  fillCircle(r, cx - radius, cy - radius, radius * 2, true);
  fillCircle(r, cx - radius + lw, cy - radius + lw, radius * 2 - 2 * lw, false);
  for (int i = 0; i < 8; ++i) {
    const float a = static_cast<float>(i) * 0.785398f;
    const float c = cosf(a);
    const float s = sinf(a);
    const int r1 = radius + lw + 2;
    const int r2 = radius + lw + radius * 2 / 3;
    thickLine(r, cx + static_cast<int>(c * r1), cy + static_cast<int>(s * r1), cx + static_cast<int>(c * r2),
              cy + static_cast<int>(s * r2), lw, true);
  }
}

}  // namespace

int bigTextWidth(const char* text, const int h) {
  int w = 0;
  int n = 0;
  for (const char* p = text; *p; ++p) {
    w += glyphWidth(*p, h);
    ++n;
  }
  if (n > 1) w += (n - 1) * gap(h);
  return w;
}

int drawBigText(const GfxRenderer& r, const int x, const int y, const int h, const char* text, const bool black) {
  int cx = x;
  const int t = thickness(h);
  for (const char* p = text; *p; ++p) {
    const char c = *p;
    if (c >= '0' && c <= '9') {
      drawDigit(r, cx, y, h, SEGMENTS[c - '0'], black);
    } else if (c == ':') {
      r.fillRect(cx, y + h * 3 / 10 - t / 2, t, t, black);
      r.fillRect(cx, y + h * 7 / 10 - t / 2, t, t, black);
    } else if (c == '-') {
      drawDigit(r, cx, y, h, 0b0000001, black);
    }
    cx += glyphWidth(c, h) + (p[1] ? gap(h) : 0);
  }
  return cx - x;
}

void drawBigTextCentered(const GfxRenderer& r, const int y, const int h, const char* text, const bool black) {
  const int w = bigTextWidth(text, h);
  drawBigText(r, (r.getScreenWidth() - w) / 2, y, h, text, black);
}

void fillCircle(const GfxRenderer& r, const int x, const int y, const int d, const bool black) {
  if (d <= 0) return;
  const float rad = d / 2.0f;
  const float cx = x + rad;
  const float cy = y + rad;
  for (int row = 0; row < d; ++row) {
    const float dy = (y + row + 0.5f) - cy;
    const float half = sqrtf(std::max(0.0f, rad * rad - dy * dy));
    const int x0 = static_cast<int>(lroundf(cx - half));
    const int x1 = static_cast<int>(lroundf(cx + half));
    if (x1 > x0) r.fillRect(x0, y + row, x1 - x0, 1, black);
  }
}

void drawWeatherIcon(const GfxRenderer& r, const int x, const int y, const int size, const int code) {
  const int s = size;
  const int lw = std::max(2, s / 16);
  if (code == 0) {
    sun(r, x + s / 2, y + s / 2, s / 5);
    return;
  }
  if (code == 1 || code == 2) {
    sun(r, x + s * 2 / 3, y + s / 3, s / 7);
    cloud(r, x + s / 12, y + s * 3 / 8, s * 3 / 4, s * 9 / 20, true);
    return;
  }
  if (code == 45 || code == 48) {
    for (int i = 0; i < 4; ++i) {
      const int ly = y + s / 4 + i * s / 7;
      const int indent = (i % 2) ? s / 8 : 0;
      r.fillRect(x + s / 8 + indent, ly, s * 3 / 4 - indent, lw, true);
    }
    return;
  }
  // Everything else sits under a cloud.
  cloud(r, x + s / 10, y + s / 8, s * 4 / 5, s / 2, true);
  const int baseY = y + s / 8 + s / 2 + lw * 2;
  const bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  const bool storm = code >= 95;
  const bool drizzle = code >= 51 && code <= 57;
  const bool rain = (code >= 61 && code <= 67) || (code >= 80 && code <= 82);
  if (storm) {
    const int bx = x + s / 2;
    const int px[] = {bx + s / 10, bx - s / 10, bx, bx - s / 6, bx + s / 20, bx - s / 20};
    const int py[] = {baseY, baseY + s / 6, baseY + s / 6, baseY + s * 3 / 8, baseY + s / 7, baseY + s / 7};
    r.fillPolygon(px, py, 6, true);
  } else if (snow) {
    for (int i = 0; i < 3; ++i) {
      const int fx = x + s / 4 + i * s / 4;
      const int fy = baseY + (i % 2) * s / 10 + s / 12;
      const int a = s / 14;
      thickLine(r, fx - a, fy, fx + a, fy, lw, true);
      thickLine(r, fx, fy - a, fx, fy + a, lw, true);
      thickLine(r, fx - a * 2 / 3, fy - a * 2 / 3, fx + a * 2 / 3, fy + a * 2 / 3, lw, true);
      thickLine(r, fx - a * 2 / 3, fy + a * 2 / 3, fx + a * 2 / 3, fy - a * 2 / 3, lw, true);
    }
  } else if (drizzle) {
    for (int i = 0; i < 4; ++i) {
      fillCircle(r, x + s / 5 + i * s / 6, baseY + (i % 2) * s / 10, lw * 2, true);
    }
  } else if (rain) {
    for (int i = 0; i < 4; ++i) {
      const int rx = x + s / 4 + i * s / 6;
      thickLine(r, rx, baseY, rx - s / 12, baseY + s / 5, lw, true);
    }
  }
}

const char* weatherLabel(const int code) {
  if (code == 0) return tr(STR_NN_WX_CLEAR);
  if (code == 1) return tr(STR_NN_WX_MAINLY_CLEAR);
  if (code == 2) return tr(STR_NN_WX_PARTLY_CLOUDY);
  if (code == 3) return tr(STR_NN_WX_OVERCAST);
  if (code == 45 || code == 48) return tr(STR_NN_WX_FOG);
  if (code >= 51 && code <= 57) return tr(STR_NN_WX_DRIZZLE);
  if (code >= 61 && code <= 67) return code >= 65 ? tr(STR_NN_WX_HEAVY_RAIN) : tr(STR_NN_WX_RAIN);
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return tr(STR_NN_WX_SNOW);
  if (code >= 80 && code <= 82) return tr(STR_NN_WX_SHOWERS);
  if (code >= 95) return tr(STR_NN_WX_THUNDER);
  return tr(STR_NN_WX_UNKNOWN);
}

void drawSparkline(const GfxRenderer& r, const int x, const int y, const int w, const int h, const float* values,
                   const int count) {
  if (count < 2 || w < 4 || h < 4) return;
  float lo = values[0];
  float hi = values[0];
  for (int i = 1; i < count; ++i) {
    lo = std::min(lo, values[i]);
    hi = std::max(hi, values[i]);
  }
  const float span = hi - lo > 0.0001f ? hi - lo : 1.0f;
  int px = x;
  int py = y + h - 1 - static_cast<int>((values[0] - lo) / span * (h - 1));
  for (int i = 1; i < count; ++i) {
    const int nx = x + (w - 1) * i / (count - 1);
    const int ny = y + h - 1 - static_cast<int>((values[i] - lo) / span * (h - 1));
    thickLine(r, px, py, nx, ny, 2, true);
    px = nx;
    py = ny;
  }
  fillCircle(r, px - 3, py - 3, 7, true);
}

}  // namespace neuron

namespace neuron {

void formatPrice(const float value, char* out, const int cap) {
  const bool neg = value < 0;
  // Double: in float, value * 100 loses the paise above ~1.6 lakh.
  const long long cents = llround(std::fabs(static_cast<double>(value)) * 100.0);
  long long whole = cents / 100;
  const int frac = static_cast<int>(cents % 100);
  // Indian grouping: last three digits, then pairs.
  char digits[24];
  snprintf(digits, sizeof(digits), "%lld", whole);
  const int len = static_cast<int>(strlen(digits));
  char grouped[40];
  int g = 0;
  for (int i = 0; i < len; ++i) {
    const int fromRight = len - i;
    grouped[g++] = digits[i];
    if (fromRight > 1 && fromRight != len + 1) {
      const int remaining = fromRight - 1;
      if (remaining == 3 || (remaining > 3 && (remaining - 3) % 2 == 0)) grouped[g++] = ',';
    }
  }
  grouped[g] = '\0';
  snprintf(out, cap, "%s%s.%02d", neg ? "-" : "", grouped, frac);
}

void formatChange(const float price, const float prevClose, char* out, const int cap) {
  if (prevClose <= 0) {
    snprintf(out, cap, "--");
    return;
  }
  const float diff = price - prevClose;
  const float pct = diff / prevClose * 100.0f;
  char abs[32];
  formatPrice(fabsf(diff), abs, sizeof(abs));
  snprintf(out, cap, "%c%s (%c%.2f%%)", diff >= 0 ? '+' : '-', abs, pct >= 0 ? '+' : '-', fabsf(pct));
}

void formatAgo(const long long epoch, char* out, const int cap) {
  const long long now = static_cast<long long>(time(nullptr));
  if (epoch <= 0 || now < 1735689600LL) {
    snprintf(out, cap, "--");
    return;
  }
  const long long d = now > epoch ? now - epoch : 0;
  if (d < 90) {
    snprintf(out, cap, "%s", tr(STR_NN_JUST_NOW));
  } else if (d < 3600) {
    snprintf(out, cap, "%lldm %s", d / 60, tr(STR_NN_AGO));
  } else if (d < 86400) {
    snprintf(out, cap, "%lldh %s", d / 3600, tr(STR_NN_AGO));
  } else {
    snprintf(out, cap, "%lldd %s", d / 86400, tr(STR_NN_AGO));
  }
}

void drawTrend(const GfxRenderer& r, const int x, const int y, const int size, const bool up) {
  const int xs[] = {x, x + size, x + size / 2};
  const int ysUp[] = {y + size, y + size, y};
  const int ysDown[] = {y, y, y + size};
  r.fillPolygon(xs, up ? ysUp : ysDown, 3, true);
}

}  // namespace neuron
