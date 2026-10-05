#pragma once
#include <cstdint>
struct CrossPointSettings {
  uint8_t clockFormat = 0;  // 0 = 24h, 1 = 12h
  uint8_t clockHasBeenSynced = 1;
  bool saveToFile() { return true; }
  static CrossPointSettings& getInstance() {
    static CrossPointSettings s;
    return s;
  }
};
#define SETTINGS CrossPointSettings::getInstance()
