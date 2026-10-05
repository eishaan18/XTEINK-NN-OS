#pragma once

#include <string>

#include "activities/UiListActivity.h"
#include "neuron/NeuronData.h"

// The two daily drills: Workout and 10,000 steps. Each row shows the last
// seven days and the current streak; Confirm ticks or unticks today.
// History lives in /neuron/habits.txt.
class HabitsActivity final : public UiListActivity {
 public:
  explicit HabitsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("NeuronHabits", renderer, mappedInput) {}

  void onEnter() override;

 private:
  neuron::Habits habits;
  freeink::ui::ListItem rows[neuron::HABIT_COUNT]{};
  char details[neuron::HABIT_COUNT][64] = {};
  int todayKey = 0;
  bool haveDate = false;

  int listCount() const override { return neuron::HABIT_COUNT; }
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;

  void rebuildRows();
};
