#include "NeuronSyncActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <iterator>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/neuron/NeuronSyncLauncher.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "neuron/NeuronClock.h"
#include "neuron/NeuronData.h"
#include "neuron/NeuronNet.h"

void launchNeuronSync(Activity& from, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Normally reboots straight into the sync screen; only returns on boards
  // that skip the fresh-heap reboot.
  silentRestartToNeuronSync();
  auto sync = makeUniqueNoThrow<NeuronSyncActivity>(renderer, mappedInput);
  if (!sync) {
    LOG_ERR("NNSYNC", "OOM: sync activity");
    return;
  }
  from.startActivityForResult(std::move(sync), [](const ActivityResult&) {});
}

void NeuronSyncActivity::onEnter() {
  Activity::onEnter();
  neuron::ensureDirs();
  usedWifi = true;
  if (WiFi.status() == WL_CONNECTED) {
    state = State::Syncing;
    requestUpdate();
    return;
  }
  state = State::Connecting;
  auto wifi = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!wifi) {
    LOG_ERR("NNSYNC", "OOM: WiFi selection");
    finish();
    return;
  }
  startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    state = State::Syncing;
    step = 0;
    requestUpdate();
  });
}

void NeuronSyncActivity::onExit() {
  Activity::onExit();
  if (usedWifi && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
  }
  // WiFi + TLS leave the heap fragmented; come back up clean on the Bridge.
  if (usedWifi) silentRestartToBridge();
}

void NeuronSyncActivity::runStep(const int index) {
  results[index] = Result::Running;
  requestUpdateAndWait();

  bool ok = false;
  switch (index) {
    case 0:
      ok = neuron::syncClockFromNtp();
      break;
    case 1: {
      // Config (~450 B) and Weather (~300 B) stay off the loop task's stack.
      auto cfg = makeUniqueNoThrow<neuron::Config>();
      auto w = makeUniqueNoThrow<neuron::Weather>();
      if (!cfg || !w) {
        LOG_ERR("NNSYNC", "OOM: weather sync");
        break;
      }
      neuron::loadConfig(*cfg);
      ok = neuron::fetchWeather(*cfg, *w) && neuron::saveWeather(*w);
      break;
    }
    case 2: {
      auto cfg = makeUniqueNoThrow<neuron::Config>();
      auto m = makeUniqueNoThrow<neuron::Markets>();
      if (!cfg || !m) {
        LOG_ERR("NNSYNC", "OOM: markets sync");
        break;
      }
      neuron::loadConfig(*cfg);
      marketsTotal = cfg->symbolCount;
      marketsOk = neuron::fetchMarkets(*cfg, *m);
      ok = marketsOk > 0 && neuron::saveMarkets(*m);
      break;
    }
    default:
      break;
  }
  results[index] = ok ? Result::Ok : Result::Failed;
}

void NeuronSyncActivity::loop() {
  if (state == State::Syncing) {
    if (WiFi.status() != WL_CONNECTED) {
      std::fill(std::begin(results), std::end(results), Result::Failed);
      state = State::Done;
      requestUpdate();
      return;
    }
    if (step < STEP_COUNT) {
      runStep(step++);
      return;
    }
    state = State::Done;
    requestUpdate();
    return;
  }
  if (state == State::Done && (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
                               mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
    finish();
  }
}

void NeuronSyncActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  constexpr int SIDE = 30;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_NN_SYNC_TITLE));

  int y = metrics.topPadding + metrics.headerHeight + height / 10;
  const char* heading = state == State::Done ? tr(STR_NN_SYNC_DONE) : tr(STR_NN_SYNCING);
  renderer.drawCenteredText(UI_12_FONT_ID, y, heading, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 40;

  const char* names[STEP_COUNT] = {tr(STR_NN_STEP_CLOCK), tr(STR_NN_WEATHER), tr(STR_NN_MARKETS)};
  const int lh = renderer.getLineHeight(UI_12_FONT_ID);
  for (int i = 0; i < STEP_COUNT; ++i) {
    // Status box: empty = pending, half = running, filled = ok, X = failed.
    const int box = lh - 6;
    const int bx = SIDE;
    const int by = y + 3;
    switch (results[i]) {
      case Result::Pending:
        renderer.drawRect(bx, by, box, box, 2, true);
        break;
      case Result::Running:
        renderer.drawRect(bx, by, box, box, 2, true);
        renderer.fillRect(bx, by + box / 2, box, box / 2, true);
        break;
      case Result::Ok:
        renderer.fillRect(bx, by, box, box, true);
        break;
      case Result::Failed:
        renderer.drawRect(bx, by, box, box, 2, true);
        renderer.drawLine(bx + 3, by + 3, bx + box - 4, by + box - 4, 2, true);
        renderer.drawLine(bx + box - 4, by + 3, bx + 3, by + box - 4, 2, true);
        break;
    }
    renderer.drawText(UI_12_FONT_ID, SIDE + box + 16, y, names[i], true, EpdFontFamily::BOLD);

    char detail[48] = {0};
    if (results[i] == Result::Ok && i == 2 && marketsOk < marketsTotal) {
      snprintf(detail, sizeof(detail), "%d/%d", marketsOk, marketsTotal);
    } else if (results[i] == Result::Ok) {
      snprintf(detail, sizeof(detail), "%s", tr(STR_NN_OK));
    } else if (results[i] == Result::Failed) {
      snprintf(detail, sizeof(detail), "%s", tr(STR_NN_FAILED));
    }
    if (detail[0]) {
      const int w = renderer.getTextWidth(UI_10_FONT_ID, detail);
      renderer.drawText(UI_10_FONT_ID, width - SIDE - w, y + 2, detail);
    }
    y += lh + 28;
  }

  if (state == State::Done) {
    y += 10;
    const auto lines = renderer.wrappedText(SMALL_FONT_ID, tr(STR_NN_SYNC_EXIT_HINT), width - 2 * SIDE, 3);
    for (const auto& l : lines) {
      renderer.drawCenteredText(SMALL_FONT_ID, y, l.c_str());
      y += renderer.getLineHeight(SMALL_FONT_ID);
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NN_DONE), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
