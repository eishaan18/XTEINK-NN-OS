#pragma once

#include <cstdint>

class GfxRenderer;

// Drawing helpers shared by the Nautical Neuron screens: large segment
// digits for the clock, simple vector weather glyphs, and sparklines.
namespace neuron {

// Width a segment-digit string ("12:45", "25:00") occupies at digit height h.
int bigTextWidth(const char* text, int h);
// Draws digits, ':' and ' ' as thick seven-segment glyphs; returns the width.
int drawBigText(const GfxRenderer& r, int x, int y, int h, const char* text, bool black = true);
// Same, horizontally centered on the screen.
void drawBigTextCentered(const GfxRenderer& r, int y, int h, const char* text, bool black = true);

// WMO weather code glyph in a size x size box.
void drawWeatherIcon(const GfxRenderer& r, int x, int y, int size, int wmoCode);
// Short localized description of a WMO weather code.
const char* weatherLabel(int wmoCode);

// Polyline through `values`, scaled to the box; a short tick marks the last point.
void drawSparkline(const GfxRenderer& r, int x, int y, int w, int h, const float* values, int count);

// Filled circle (diameter d) at top-left x,y.
void fillCircle(const GfxRenderer& r, int x, int y, int d, bool black = true);

}  // namespace neuron

namespace neuron {

// 2480531.5 -> "24,80,531.50" (Indian digit grouping, 2 decimals).
void formatPrice(float value, char* out, int cap);
// Signed change with percentage: "+182.40 (+0.74%)".
void formatChange(float price, float prevClose, char* out, int cap);
// Relative age of a timestamp: "just now", "12m ago", "3h ago", "2d ago".
void formatAgo(long long epoch, char* out, int cap);
// Small filled up/down triangle (size px) for market moves.
void drawTrend(const GfxRenderer& r, int x, int y, int size, bool up);

}  // namespace neuron
