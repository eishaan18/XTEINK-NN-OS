#pragma once

#include "activities/Activity.h"
#include "neuron/NeuronData.h"

// Cached weather for the configured place (Pune by default). Confirm runs a
// Neuron sync to refresh it.
class WeatherActivity final : public Activity {
 public:
  explicit WeatherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NeuronWeather", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  neuron::Weather weather;
  neuron::Config config;
};
