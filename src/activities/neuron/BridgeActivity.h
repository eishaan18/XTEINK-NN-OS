#pragma once

#include <memory>
#include <string>

#include "activities/Activity.h"
#include "neuron/NeuronData.h"

// The Bridge: Nautical Neuron's home dashboard. A large local clock with GMT
// underneath, the date, cached weather and index snapshots, today's two
// drills (workout, 10,000 steps), a quote, and a launcher grid for the Neuron
// apps and the book library.
class BridgeActivity final : public Activity {
 public:
  enum class App : uint8_t { Habits, Focus, Weather, Markets, Sync, Books, Settings, Count };

  explicit BridgeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Bridge", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int selected = 0;
  int lastMinute = -1;
  int renderCount = 0;

  neuron::Habits habits;
  int habitsDay = 0;  // day key the habits were loaded for
  neuron::Weather weather;
  std::unique_ptr<neuron::Markets> markets;
  std::string quote;
  std::string quoteAuthor;
  int quoteDay = -1;

  void reloadData();
  void open(App app);

  int drawClock(int y);
  int drawCards(int y);
  int drawDrills(int y);
  void drawQuote(int y, int maxY);
  void drawLauncher(int y);
  int launcherHeight() const;
};
