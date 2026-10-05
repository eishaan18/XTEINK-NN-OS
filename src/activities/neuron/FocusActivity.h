#pragma once

#include "activities/Activity.h"

// "Focus Watch": a Pomodoro timer. Confirm starts/pauses, Left/Right change
// the watch length while stopped, Back stops (or leaves when stopped).
// Completed focus watches are logged per day in /neuron/cache/focus.txt.
class FocusActivity final : public Activity {
 public:
  explicit FocusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NeuronFocus", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::Running; }

 private:
  enum class State : uint8_t { Idle, Running, Paused, Done };
  enum class Phase : uint8_t { Focus, ShortBreak, LongBreak };

  State state = State::Idle;
  Phase phase = Phase::Focus;
  int focusMinutes = 25;
  int completedInRow = 0;  // focus watches since the last long break
  unsigned long phaseMs = 0;
  unsigned long startedAt = 0;
  unsigned long elapsedBeforePause = 0;
  int lastShownMinute = -1;
  int todaySessions = 0;
  int todayMinutes = 0;
  int todayKey = 0;

  unsigned long elapsedMs() const;
  int phaseMinutes() const;
  void startPhase(Phase p);
  void completePhase();
  void refreshToday();
};
