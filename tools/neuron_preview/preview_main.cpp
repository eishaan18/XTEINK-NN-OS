// Host preview: renders the Nautical Neuron screens with sample data from
// ./sd into out/*.ops (draw-op logs); render.py turns those into PNGs.
#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

#define private public  // preview only: lets the harness pose screens mid-state
#include "activities/neuron/BridgeActivity.h"
#include "activities/neuron/FocusActivity.h"
#include "activities/neuron/HabitsActivity.h"
#include "activities/neuron/MarketsActivity.h"
#include "activities/neuron/NeuronSyncActivity.h"
#include "activities/neuron/WeatherActivity.h"
#undef private
#include "neuron/NeuronData.h"
#include "neuron/NeuronNet.h"

// --- pieces the preview does not exercise -----------------------------------
namespace neuron {
bool fetchWeather(const Config&, Weather&) { return false; }
bool fetchQuote(freeink::SecureHttpClient&, const Symbol&, Quote&) { return false; }
int fetchMarkets(const Config&, Markets&, void (*)(int, int, void*), void*) { return 0; }
}  // namespace neuron
void HabitsActivity::onEnter() {}
const char* HabitsActivity::headerTitle() const { return ""; }
void HabitsActivity::buildScreen(UiScreen&) {}
void HabitsActivity::activateIndex(int) {}

namespace {
void dump(const char* name) {
  std::filesystem::create_directories("out");
  std::ofstream f(std::string("out/") + name + ".ops");
  for (const auto& op : GfxRenderer::ops()) f << op << '\n';
  GfxRenderer::ops().clear();
  printf("wrote out/%s.ops\n", name);
}

template <typename A>
void shot(A& a, const char* name) {
  a.render(RenderLock{});
  dump(name);
}
}  // namespace

int main() {
  setenv("TZ", "IST-5:30", 1);
  tzset();
  GfxRenderer renderer;
  MappedInputManager input;

  {
    BridgeActivity a(renderer, input);
    a.onEnter();
    shot(a, "bridge");
    a.selected = 3;
    shot(a, "bridge_markets_selected");
  }
  {
    WeatherActivity a(renderer, input);
    a.onEnter();
    shot(a, "weather");
  }
  {
    MarketsActivity a(renderer, input);
    a.onEnter();
    shot(a, "markets");
  }
  {
    FocusActivity a(renderer, input);
    a.onEnter();
    shot(a, "focus_idle");
    a.state = FocusActivity::State::Running;
    a.phaseMs = 25UL * 60000UL;
    a.elapsedBeforePause = 9UL * 60000UL;
    a.startedAt = millis();
    shot(a, "focus_running");
  }
  {
    NeuronSyncActivity a(renderer, input);
    a.state = NeuronSyncActivity::State::Done;
    a.results[0] = NeuronSyncActivity::Result::Ok;
    a.results[1] = NeuronSyncActivity::Result::Ok;
    a.results[2] = NeuronSyncActivity::Result::Ok;
    a.marketsOk = 5;
    a.marketsTotal = 6;
    shot(a, "sync_done");
  }
  return 0;
}
