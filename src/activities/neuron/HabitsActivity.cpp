#include "HabitsActivity.h"

#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "neuron/NeuronClock.h"

namespace fui = freeink::ui;

namespace {
const char* habitLabel(const int id) {
  return id == neuron::HABIT_WORKOUT ? tr(STR_NN_HABIT_WORKOUT) : tr(STR_NN_HABIT_STEPS);
}
}  // namespace

void HabitsActivity::onEnter() {
  UiListActivity::onEnter();
  struct tm now;
  haveDate = neuron::localNow(now);
  todayKey = haveDate ? neuron::dayKey(now) : 0;
  neuron::loadHabits(habits, todayKey);
  rebuildRows();
}

const char* HabitsActivity::headerTitle() const { return tr(STR_NN_HABITS); }

void HabitsActivity::rebuildRows() {
  for (int id = 0; id < neuron::HABIT_COUNT; ++id) {
    const uint64_t bits = habits.bits[id];
    char* d = details[id];
    const size_t cap = sizeof(details[id]);
    if (!haveDate) {
      snprintf(d, cap, "%s", tr(STR_NN_CLOCK_NOT_SET_HINT));
    } else {
      // Seven-day strip, oldest on the left: bullet = done, middle dot = missed.
      int o = 0;
      for (int day = 6; day >= 0; --day) {
        o += snprintf(d + o, cap - o, "%s%s", ((bits >> day) & 1ULL) ? "\xE2\x80\xA2" : "\xC2\xB7", day ? " " : "");
      }
      snprintf(d + o, cap - o, "   %d %s", neuron::habitStreak(bits), tr(STR_NN_DAY_STREAK));
    }
    fui::ListItem item;
    item.label = habitLabel(id);
    item.subtitle = d;
    item.toggle = true;
    item.toggleChecked = (bits & 1ULL) != 0;
    item.enabled = haveDate;
    item.actionValue = static_cast<int16_t>(id);
    rows[id] = item;
  }
  requestUpdate();
}

void HabitsActivity::activateIndex(const int index) {
  if (!haveDate || index < 0 || index >= neuron::HABIT_COUNT) return;
  habits.bits[index] ^= 1ULL;
  neuron::saveHabits(habits, todayKey);
  rebuildRows();
}

void HabitsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(neuron::HABIT_COUNT);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
