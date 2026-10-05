#include "FocusActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "neuron/NeuronClock.h"
#include "neuron/NeuronData.h"
#include "neuron/NeuronDraw.h"

namespace {
constexpr int SHORT_BREAK_MIN = 5;
constexpr int LONG_BREAK_MIN = 15;
constexpr int WATCHES_PER_LONG_BREAK = 4;
constexpr int MIN_FOCUS = 5;
constexpr int MAX_FOCUS = 90;
}  // namespace

void FocusActivity::onEnter() {
  Activity::onEnter();
  refreshToday();
  requestUpdate();
}

void FocusActivity::refreshToday() {
  struct tm now;
  todayKey = neuron::localNow(now) ? neuron::dayKey(now) : 0;
  const auto day = neuron::loadFocusDay(todayKey);
  todaySessions = day.sessions;
  todayMinutes = day.minutes;
}

int FocusActivity::phaseMinutes() const {
  switch (phase) {
    case Phase::ShortBreak:
      return SHORT_BREAK_MIN;
    case Phase::LongBreak:
      return LONG_BREAK_MIN;
    default:
      return focusMinutes;
  }
}

unsigned long FocusActivity::elapsedMs() const {
  if (state == State::Running) return elapsedBeforePause + (millis() - startedAt);
  return elapsedBeforePause;
}

void FocusActivity::startPhase(const Phase p) {
  phase = p;
  phaseMs = static_cast<unsigned long>(phaseMinutes()) * 60000UL;
  elapsedBeforePause = 0;
  startedAt = millis();
  state = State::Running;
  lastShownMinute = -1;
  requestUpdate();
}

void FocusActivity::completePhase() {
  state = State::Done;
  if (phase == Phase::Focus) {
    ++completedInRow;
    neuron::addFocusSession(todayKey, focusMinutes);
    refreshToday();
  } else if (phase == Phase::LongBreak) {
    completedInRow = 0;
  }
  // No buzzer on the X4: flash the panel so the end of a watch is noticeable.
  {
    RenderLock lock(*this);
    for (int i = 0; i < 2; ++i) {
      renderer.clearScreen(0x00);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      renderer.clearScreen();
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    }
  }
  requestUpdate();
}

void FocusActivity::loop() {
  using B = MappedInputManager::Button;

  if (state == State::Running) {
    const unsigned long elapsed = elapsedMs();
    if (elapsed >= phaseMs) {
      completePhase();
      return;
    }
    const int remainingMin = static_cast<int>((phaseMs - elapsed + 59999UL) / 60000UL);
    if (remainingMin != lastShownMinute) {
      lastShownMinute = remainingMin;
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(B::Back)) {
    if (state == State::Idle) {
      finish();
      return;
    }
    state = State::Idle;
    phase = Phase::Focus;
    elapsedBeforePause = 0;
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(B::Confirm)) {
    switch (state) {
      case State::Idle:
        startPhase(Phase::Focus);
        break;
      case State::Running:
        elapsedBeforePause = elapsedMs();
        state = State::Paused;
        requestUpdate();
        break;
      case State::Paused:
        startedAt = millis();
        state = State::Running;
        requestUpdate();
        break;
      case State::Done:
        if (phase == Phase::Focus) {
          startPhase(completedInRow >= WATCHES_PER_LONG_BREAK ? Phase::LongBreak : Phase::ShortBreak);
        } else {
          startPhase(Phase::Focus);
        }
        break;
    }
    return;
  }

  if (state == State::Idle) {
    int next = focusMinutes;
    if (mappedInput.wasPressed(B::Right) || mappedInput.wasPressed(B::Up)) next += 5;
    if (mappedInput.wasPressed(B::Left) || mappedInput.wasPressed(B::Down)) next -= 5;
    next = std::max(MIN_FOCUS, std::min(MAX_FOCUS, next));
    if (next != focusMinutes) {
      focusMinutes = next;
      requestUpdate();
    }
  }
}

void FocusActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_NN_FOCUS_WATCH));

  const char* phaseLabel = phase == Phase::Focus        ? tr(STR_NN_PHASE_FOCUS)
                           : phase == Phase::ShortBreak ? tr(STR_NN_PHASE_BREAK)
                                                        : tr(STR_NN_PHASE_LONG_BREAK);
  int y = metrics.topPadding + metrics.headerHeight + height / 12;
  renderer.drawCenteredText(UI_12_FONT_ID, y, phaseLabel, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 24;

  // Remaining minutes, big. Idle shows the configured length.
  int minutes = phaseMinutes();
  if (state == State::Running || state == State::Paused) {
    const unsigned long elapsed = elapsedMs();
    minutes = elapsed >= phaseMs ? 0 : static_cast<int>((phaseMs - elapsed + 59999UL) / 60000UL);
  } else if (state == State::Done) {
    minutes = 0;
  }
  char big[8];
  snprintf(big, sizeof(big), "%d", minutes);
  const int digitH = height / 4;
  neuron::drawBigTextCentered(renderer, y, digitH, big);
  y += digitH + 12;
  renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_NN_MINUTES));
  y += renderer.getLineHeight(UI_10_FONT_ID) + 24;

  // Progress: one block per minute (max 30 blocks, scaled for longer watches).
  const int total = phaseMinutes();
  const int blocks = std::min(30, total);
  const unsigned long elapsed = state == State::Idle ? 0 : (state == State::Done ? phaseMs : elapsedMs());
  const int filled = phaseMs ? static_cast<int>((static_cast<unsigned long long>(elapsed) * blocks) / phaseMs) : 0;
  const int barX = 30;
  const int barW = width - 60;
  const int cell = barW / blocks;
  for (int i = 0; i < blocks; ++i) {
    const int bx = barX + i * cell;
    if (i < filled) {
      renderer.fillRect(bx + 1, y, cell - 3, 22, true);
    } else {
      renderer.drawRect(bx + 1, y, cell - 3, 22, 1, true);
    }
  }
  y += 22 + 30;

  const char* status = nullptr;
  switch (state) {
    case State::Idle:
      status = tr(STR_NN_FOCUS_IDLE_HINT);
      break;
    case State::Running:
      status = tr(STR_NN_FOCUS_RUNNING);
      break;
    case State::Paused:
      status = tr(STR_NN_FOCUS_PAUSED);
      break;
    case State::Done:
      status = phase == Phase::Focus ? tr(STR_NN_FOCUS_DONE) : tr(STR_NN_BREAK_DONE);
      break;
  }
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, status, width - 60, 2);
  for (const auto& l : lines) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, l.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }

  char today[64];
  snprintf(today, sizeof(today), "%s: %d  |  %d %s", tr(STR_NN_WATCHES_TODAY), todaySessions, todayMinutes,
           tr(STR_NN_MIN_SHORT));
  renderer.drawCenteredText(UI_10_FONT_ID, height - metrics.buttonHintsHeight - 50, today, true, EpdFontFamily::BOLD);

  const char* confirm = state == State::Running  ? tr(STR_NN_PAUSE)
                        : state == State::Paused ? tr(STR_NN_RESUME)
                                                 : tr(STR_NN_START);
  const char* back = state == State::Idle ? tr(STR_BACK) : tr(STR_NN_STOP);
  const auto labels =
      mappedInput.mapLabels(back, confirm, state == State::Idle ? "-5" : "", state == State::Idle ? "+5" : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
