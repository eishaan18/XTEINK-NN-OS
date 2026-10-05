#pragma once
#include "activities/Activity.h"
class WifiSelectionActivity final : public Activity {
 public:
  WifiSelectionActivity(GfxRenderer& r, MappedInputManager& m, bool = true) : Activity("Wifi", r, m) {}
};
