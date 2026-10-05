#pragma once
// Host preview stand-in for the firmware GfxRenderer: records draw calls as
// text ops (replayed by render.py) and measures text with real font advances.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "HalDisplay.h"

enum Color : uint8_t { Clear = 0x00, White = 0x01, LightGray = 0x05, DarkGray = 0x0A, Black = 0x10 };

namespace EpdFontFamily {
enum Style : uint8_t { REGULAR = 0, BOLD = 1, ITALIC = 2, BOLD_ITALIC = 3 };
}

class GfxRenderer {
 public:
  static std::vector<std::string>& ops();
  static void op(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

  int getScreenWidth() const { return 480; }
  int getScreenHeight() const { return 800; }
  void clearScreen(uint8_t color = 0xFF) const { op("clear %d", color); }
  void invertScreen() const { op("invert"); }
  void displayBuffer(HalDisplay::RefreshMode mode = HalDisplay::FAST_REFRESH) const { op("display %d", (int)mode); }

  void drawPixel(int x, int y, bool state = true) const { op("frect %d %d 1 1 %d", x, y, state); }
  void drawLine(int x1, int y1, int x2, int y2, bool state = true) const {
    op("line %d %d %d %d 1 %d", x1, y1, x2, y2, state);
  }
  // Same as the firmware: a thick line is lw one-pixel copies stacked downward.
  void drawLine(int x1, int y1, int x2, int y2, int lw, bool state) const {
    for (int i = 0; i < lw; ++i) drawLine(x1, y1 + i, x2, y2 + i, state);
  }
  void drawRect(int x, int y, int w, int h, bool state = true) const { drawRect(x, y, w, h, 1, state); }
  void drawRect(int x, int y, int w, int h, int lw, bool state) const {
    op("rect %d %d %d %d %d %d", x, y, w, h, lw, state);
  }
  void drawRoundedRect(int x, int y, int w, int h, int lw, int r, bool state) const {
    op("rrect %d %d %d %d %d %d %d", x, y, w, h, lw, r, state);
  }
  void fillRect(int x, int y, int w, int h, bool state = true) const { op("frect %d %d %d %d %d", x, y, w, h, state); }
  void fillRoundedRect(int x, int y, int w, int h, int r, Color c) const {
    op("frrect %d %d %d %d %d %d", x, y, w, h, r, c == Color::White ? 0 : 1);
  }
  void fillPolygon(const int* xs, const int* ys, int n, bool state = true) const;
  void drawImage(const uint8_t*, int x, int y, int w, int h) const { op("image %d %d %d %d", x, y, w, h); }

  int getTextWidth(int fontId, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
  int getLineHeight(int fontId) const;
  int getTextHeight(int fontId) const { return getLineHeight(fontId); }
  void drawText(int fontId, int x, int y, const char* text, bool black = true,
                EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
  void drawCenteredText(int fontId, int y, const char* text, bool black = true,
                        EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    drawText(fontId, (getScreenWidth() - getTextWidth(fontId, text, style)) / 2, y, text, black, style);
  }
  std::string truncatedText(int fontId, const char* text, int maxWidth,
                            EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
  std::vector<std::string> wrappedText(int fontId, const char* text, int maxWidth, int maxLines,
                                       EpdFontFamily::Style style = EpdFontFamily::REGULAR) const;
};
