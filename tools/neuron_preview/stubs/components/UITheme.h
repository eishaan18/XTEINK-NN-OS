#pragma once
// Host preview stand-in for UITheme (Lyra metrics).
#include "GfxRenderer.h"

struct Rect {
  int x = 0, y = 0, width = 0, height = 0;
};

struct ThemeMetrics {
  int topPadding = 10;
  int headerHeight = 84;
  int verticalSpacing = 16;
  int buttonHintsHeight = 40;
  int contentSidePadding = 20;
};

class UITheme {
 public:
  static UITheme& getInstance() { static UITheme t; return t; }
  const ThemeMetrics& getMetrics() const { static ThemeMetrics m; return m; }
  void drawHeader(const GfxRenderer& r, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool backButton = true) const;
  void drawButtonHints(GfxRenderer& r, const char* b1, const char* b2, const char* b3, const char* b4) const;
  Rect drawPopup(const GfxRenderer& r, const char* msg) const;
};
#define GUI UITheme::getInstance()
