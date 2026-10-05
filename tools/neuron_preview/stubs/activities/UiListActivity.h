#pragma once
// Minimal stand-in so the list-app headers parse; list screens are not previewed.
#include <cstdint>

#include "activities/Activity.h"

namespace freeink::ui {
struct ListItem {
  const char* label = nullptr;
  const char* subtitle = nullptr;
  const char* value = nullptr;
  int16_t actionValue = 0;
  bool enabled = true;
  bool toggle = false;
  bool toggleChecked = false;
};
struct ListNav {
  int selected = 0;
};
}  // namespace freeink::ui

class UiListActivity : public Activity {
 protected:
  struct UiScreen {};
  struct App {
    void clearTapFlash() {}
  } app;
  freeink::ui::ListNav nav;
  UiListActivity(const char* n, GfxRenderer& r, MappedInputManager& m, bool = false) : Activity(n, r, m) {}
  virtual int listCount() const = 0;
  virtual void buildScreen(UiScreen&) = 0;
  virtual void activateIndex(int) = 0;
  virtual const char* headerTitle() const { return nullptr; }
  void moveSelectionTo(int) {}
};
