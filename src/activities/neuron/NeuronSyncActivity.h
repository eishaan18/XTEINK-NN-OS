#pragma once

#include "activities/Activity.h"

// One WiFi session that refreshes everything online: NTP clock, weather and
// market quotes. Reuses the saved-network auto-connect (or the network
// picker), shows per-step results, and on exit reboots silently into the
// Bridge to hand back a clean heap.
class NeuronSyncActivity final : public Activity {
 public:
  explicit NeuronSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NeuronSync", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { Connecting, Syncing, Done };
  enum class Result : uint8_t { Pending, Running, Ok, Failed };
  static constexpr int STEP_COUNT = 3;  // clock, weather, markets

  State state = State::Connecting;
  Result results[STEP_COUNT] = {Result::Pending, Result::Pending, Result::Pending};
  int step = 0;
  int marketsOk = 0;
  int marketsTotal = 0;
  bool usedWifi = false;

  void runStep(int index);
};
