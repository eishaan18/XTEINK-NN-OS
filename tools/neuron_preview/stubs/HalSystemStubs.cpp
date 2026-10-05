// Definitions backing the host preview stubs.
#include <cstdarg>
#include <cstring>

#include "GfxRenderer.h"
#include "PreviewMetrics.h"
#include "components/UITheme.h"

std::vector<std::string>& GfxRenderer::ops() {
  static std::vector<std::string> v;
  return v;
}

void GfxRenderer::op(const char* fmt, ...) {
  char buf[1024];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  ops().emplace_back(buf);
}

namespace {
const PreviewFont& font(int id) {
  for (const auto& f : kPreviewFonts)
    if (f.id == id) return f;
  return kPreviewFonts[0];
}
int styleIndex(EpdFontFamily::Style s) { return s == EpdFontFamily::BOLD ? 1 : (s == EpdFontFamily::ITALIC ? 2 : 0); }

// Advance of one UTF-8 character starting at p; advances p.
int advance(const PreviewFont& f, int style, const char*& p) {
  const unsigned char c = static_cast<unsigned char>(*p);
  if (c < 0x80) {
    ++p;
    return (c >= 32 && c < 127) ? f.adv[style][c - 32] : 0;
  }
  const int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
  uint32_t cp = c & (len == 2 ? 0x1F : len == 3 ? 0x0F : 0x07);
  for (int i = 1; i < len && p[i]; ++i) cp = (cp << 6) | (static_cast<unsigned char>(p[i]) & 0x3F);
  p += len;
  for (size_t i = 0; i < sizeof(kPreviewExtra) / sizeof(kPreviewExtra[0]); ++i) {
    if (kPreviewExtra[i] == cp) return f.adv[style][95 + i];
  }
  return f.adv[style]['?' - 32];
}
}  // namespace

int GfxRenderer::getTextWidth(int fontId, const char* text, EpdFontFamily::Style style) const {
  const auto& f = font(fontId);
  int w = 0;
  for (const char* p = text; *p;) w += advance(f, styleIndex(style), p);
  return w;
}

int GfxRenderer::getLineHeight(int fontId) const { return font(fontId).lineHeight; }

void GfxRenderer::drawText(int fontId, int x, int y, const char* text, bool black, EpdFontFamily::Style style) const {
  if (!text || !*text) return;
  const char* st = style == EpdFontFamily::BOLD ? "B" : (style == EpdFontFamily::ITALIC ? "I" : "R");
  op("text %s %s %d %d %d %d %s", font(fontId).name, st, x, y + font(fontId).ascender, black ? 1 : 0, 0, text);
}

void GfxRenderer::fillPolygon(const int* xs, const int* ys, int n, bool state) const {
  std::string s = "poly " + std::to_string(state ? 1 : 0) + " " + std::to_string(n);
  for (int i = 0; i < n; ++i) s += " " + std::to_string(xs[i]) + " " + std::to_string(ys[i]);
  ops().push_back(s);
}

std::string GfxRenderer::truncatedText(int fontId, const char* text, int maxWidth, EpdFontFamily::Style style) const {
  if (getTextWidth(fontId, text, style) <= maxWidth) return text;
  std::string s = text;
  while (!s.empty() && getTextWidth(fontId, (s + "\xE2\x80\xA6").c_str(), style) > maxWidth) s.pop_back();
  return s + "\xE2\x80\xA6";
}

std::vector<std::string> GfxRenderer::wrappedText(int fontId, const char* text, int maxWidth, int maxLines,
                                                  EpdFontFamily::Style style) const {
  std::vector<std::string> lines;
  std::string cur, word;
  std::string t = text;
  t += ' ';
  for (char c : t) {
    if (c != ' ') {
      word += c;
      continue;
    }
    if (word.empty()) continue;
    const std::string cand = cur.empty() ? word : cur + " " + word;
    if (getTextWidth(fontId, cand.c_str(), style) <= maxWidth || cur.empty()) {
      cur = cand;
    } else {
      lines.push_back(cur);
      cur = word;
    }
    word.clear();
  }
  if (!cur.empty()) lines.push_back(cur);
  if (static_cast<int>(lines.size()) > maxLines) {
    lines.resize(maxLines);
    lines.back() = truncatedText(fontId, (lines.back() + " \xE2\x80\xA6\xE2\x80\xA6").c_str(), maxWidth, style);
  }
  return lines;
}

// Approximation of the Lyra header: title bottom-left, battery top-right, rule.
void UITheme::drawHeader(const GfxRenderer& r, Rect rect, const char* title, const char* subtitle, bool) const {
  const int lh = r.getLineHeight(1831230762);  // UI_12
  r.drawRect(r.getScreenWidth() - 46, rect.y + 6, 26, 13, 2, true);
  r.fillRect(r.getScreenWidth() - 20, rect.y + 10, 3, 5, true);
  r.fillRect(r.getScreenWidth() - 43, rect.y + 9, 16, 7, true);
  r.drawText(1465627787, r.getScreenWidth() - 90, rect.y + 2, "82%");
  const bool shortBand = rect.height < 84;
  const int ty = shortBand ? rect.y + (rect.height - lh) / 2 : rect.y + rect.height - lh - 12;
  if (title) r.drawText(1831230762, 20, ty, title, true, EpdFontFamily::BOLD);
  if (subtitle) {
    const int w = r.getTextWidth(1322569422, subtitle);
    r.drawText(1322569422, r.getScreenWidth() - 20 - w, rect.y + rect.height - lh - 8, subtitle);
  }
  r.fillRect(20, rect.y + rect.height - 4, r.getScreenWidth() - 40, 2, true);
}

void UITheme::drawButtonHints(GfxRenderer& r, const char* b1, const char* b2, const char* b3, const char* b4) const {
  const char* labels[4] = {b1, b2, b3, b4};
  const int w = r.getScreenWidth() / 4;
  const int y = r.getScreenHeight() - 40;
  for (int i = 0; i < 4; ++i) {
    if (!labels[i] || !*labels[i]) continue;
    r.drawRoundedRect(i * w + 6, y + 4, w - 12, 34, 1, 6, true);
    const int tw = r.getTextWidth(1465627787, labels[i]);
    r.drawText(1465627787, i * w + (w - tw) / 2, y + 9, labels[i]);
  }
}

Rect UITheme::drawPopup(const GfxRenderer&, const char*) const { return {}; }

void silentRestart() {}
void silentRestartToReader() {}
void silentRestartToSettings() {}
void silentRestartToBridge() {}
void silentRestartToNeuronSync() {}
void silentRestartToJoinNetwork() {}
void restartToHomeAfterStorageHandoff() {}
