#pragma once
// Host preview stand-in for the firmware Activity base.
#include <Logging.h>

#include <memory>
#include <string>
#include <utility>

#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "activities/ActivityResult.h"

class Activity;

class RenderLock {
 public:
  RenderLock() = default;
  explicit RenderLock(Activity&) {}
};

enum class HomeMenuItem { NONE, FILE_BROWSER, LIBRARY, OPDS_BROWSER, FILE_TRANSFER, SETTINGS_MENU, BRIDGE };

struct ActivityManagerStub {
  void goHome(HomeMenuItem = HomeMenuItem::NONE) {}
  void goToSettings() {}
  void goToBridge() {}
};
inline ActivityManagerStub activityManager;

class Activity {
 protected:
  std::string name;
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;

 public:
  explicit Activity(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput)
      : name(std::move(name)), renderer(renderer), mappedInput(mappedInput) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool skipLoopDelay() { return false; }
  virtual bool preventAutoSleep() { return false; }
  void requestUpdate(bool = false) {}
  void requestUpdateAndWait() {}
  void startActivityForResult(std::unique_ptr<Activity>&&, ActivityResultHandler) {}
  static void finish() {}
};
