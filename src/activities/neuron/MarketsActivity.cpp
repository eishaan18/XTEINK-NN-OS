#include "MarketsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "activities/neuron/NeuronSyncLauncher.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "neuron/NeuronDraw.h"

namespace {
constexpr int SIDE = 16;
constexpr int ROW_H = 92;
}  // namespace

void MarketsActivity::onEnter() {
  Activity::onEnter();
  markets = makeUniqueNoThrow<neuron::Markets>();
  if (!markets) {
    LOG_ERR("NNMKT", "OOM: markets");
  } else {
    neuron::loadMarkets(*markets);
  }
  requestUpdate();
}

void MarketsActivity::loop() {
  using B = MappedInputManager::Button;
  if (mappedInput.wasReleased(B::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    launchNeuronSync(*this, renderer, mappedInput);
    return;
  }
  const int count = markets ? markets->count : 0;
  const int pages = std::max(1, (count + perPage - 1) / perPage);
  int next = page;
  if (mappedInput.wasPressed(B::Down) || mappedInput.wasPressed(B::Right)) next = (page + 1) % pages;
  if (mappedInput.wasPressed(B::Up) || mappedInput.wasPressed(B::Left)) next = (page + pages - 1) % pages;
  if (next != page) {
    page = next;
    requestUpdate();
  }
}

void MarketsActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  renderer.clearScreen();
  const int top = metrics.topPadding + metrics.headerHeight + 8;
  const int footerY = height - metrics.buttonHintsHeight - 34;
  perPage = std::max(1, (footerY - top - 8) / ROW_H);
  const int count = markets ? markets->count : 0;
  const int pages = std::max(1, (count + perPage - 1) / perPage);
  if (page >= pages) page = pages - 1;

  char subtitle[16];
  snprintf(subtitle, sizeof(subtitle), "%d/%d", page + 1, pages);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_NN_MARKETS),
                 pages > 1 ? subtitle : nullptr);

  if (!markets || !markets->valid) {
    renderer.drawCenteredText(UI_12_FONT_ID, height / 3, tr(STR_NN_NO_MARKETS), true, EpdFontFamily::BOLD);
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, tr(STR_NN_NO_DATA_SYNC), width - 2 * SIDE, 3);
    int ly = height / 3 + renderer.getLineHeight(UI_12_FONT_ID) + 10;
    for (const auto& l : lines) {
      renderer.drawCenteredText(UI_10_FONT_ID, ly, l.c_str());
      ly += renderer.getLineHeight(UI_10_FONT_ID);
    }
  } else {
    const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
    const int lh12 = renderer.getLineHeight(UI_12_FONT_ID);
    const int sparkW = width / 3;
    int y = top;
    for (int i = page * perPage; i < count && i < (page + 1) * perPage; ++i) {
      const auto& q = markets->quotes[i];
      const int textW = width - 2 * SIDE - sparkW - 12;

      const std::string name = renderer.truncatedText(UI_10_FONT_ID, q.label, textW, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, SIDE, y + 4, name.c_str(), true, EpdFontFamily::BOLD);

      char price[24];
      if (q.ok) {
        neuron::formatPrice(q.price, price, sizeof(price));
      } else {
        snprintf(price, sizeof(price), "--");
      }
      renderer.drawText(UI_12_FONT_ID, SIDE, y + 6 + lh10, price, true, EpdFontFamily::BOLD);

      if (q.ok && q.prevClose > 0) {
        char chg[40];
        neuron::formatChange(q.price, q.prevClose, chg, sizeof(chg));
        const bool up = q.price >= q.prevClose;
        const int cy = y + 6 + lh10 + lh12;
        neuron::drawTrend(renderer, SIDE, cy + 3, 10, up);
        renderer.drawText(SMALL_FONT_ID, SIDE + 16, cy, chg);
      }

      if (q.closeCount >= 2) {
        neuron::drawSparkline(renderer, width - SIDE - sparkW, y + 12, sparkW, ROW_H - 30, q.closes, q.closeCount);
      }
      y += ROW_H;
      renderer.drawLine(SIDE, y - 4, width - SIDE, y - 4, 1, true);
    }

    char ago[24];
    neuron::formatAgo(static_cast<long long>(markets->updated), ago, sizeof(ago));
    char footer[96];
    snprintf(footer, sizeof(footer), "%s %s  |  %s", tr(STR_NN_UPDATED), ago, tr(STR_NN_MARKETS_SOURCE));
    renderer.drawCenteredText(SMALL_FONT_ID, footerY, footer);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NN_SYNC), pages > 1 ? tr(STR_DIR_UP) : "",
                                            pages > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
