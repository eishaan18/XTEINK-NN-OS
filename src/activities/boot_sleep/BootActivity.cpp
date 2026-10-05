#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/NeuronLogo.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  constexpr int S = NEURON_LOGO_SIZE;
  const int logoY = (pageHeight - S) / 2 - 30;
  renderer.drawImage(NeuronLogo, (pageWidth - S) / 2, logoY, S, S);
  // The crest already carries the name; only the status line goes underneath.
  renderer.drawCenteredText(SMALL_FONT_ID, logoY + S + 30, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
