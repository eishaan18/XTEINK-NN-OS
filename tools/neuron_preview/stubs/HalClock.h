#pragma once
#include <cstdlib>
#include <ctime>
// Preview: system clock, "synced" an hour ago, no RTC chip (like the X4).
class HalClock {
 public:
  bool isAvailable() const { return true; }
  bool hasRtc() const { return false; }
  bool timeValid() const { return true; }
  time_t lastSyncUtc() const { return time(nullptr) - 3600; }
  void noteSystemTimeSynced() {}
  bool utcTime(struct tm& out) const {
    const time_t now = time(nullptr);
    gmtime_r(&now, &out);
    return true;
  }
  bool localTime(struct tm& out) const {
    const time_t now = time(nullptr);
    localtime_r(&now, &out);
    return true;
  }
  void setTimezone(const char* tz) {
    setenv("TZ", tz && *tz ? tz : "UTC0", 1);
    tzset();
  }
  bool syncFromNTP() { return false; }
};
inline HalClock halClock;
