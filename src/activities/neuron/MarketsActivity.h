#pragma once

#include <memory>

#include "activities/Activity.h"
#include "neuron/NeuronData.h"

// Cached quotes for the configured symbols (NIFTY 50, SENSEX, ... by
// default) with a one-month sparkline each. Confirm runs a Neuron sync.
class MarketsActivity final : public Activity {
 public:
  explicit MarketsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NeuronMarkets", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  std::unique_ptr<neuron::Markets> markets;
  int page = 0;
  int perPage = 1;
};
