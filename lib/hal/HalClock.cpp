#include "HalClock.h"

#include <Logging.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

HalClock halClock;  // Singleton instance

namespace {
// 2025-01-01T00:00:00Z: an earlier system time means it was never set.
constexpr time_t MIN_VALID_EPOCH = 1735689600;
constexpr const char* PREFS_NAMESPACE = "nnclock";
constexpr const char* PREFS_SYNC = "sync";

// UTC of the last NTP sync while the system clock has run continuously since
// (0 = untrusted). Mirrored in NVS so deep sleep and restarts keep it. The
// SNTP callback updates it from the lwIP task; 64-bit stores are not atomic
// on the C3.
int64_t lastSync = 0;
portMUX_TYPE syncLock = portMUX_INITIALIZER_UNLOCKED;

void storeLastSync(const int64_t value) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, /*readOnly=*/false)) return;
  prefs.putLong64(PREFS_SYNC, value);
  prefs.end();
}
}  // namespace

void HalClock::begin() {
  _hasRtc = _sdkRtc.begin();
  _available = true;  // system clock fallback when there is no RTC chip
  LOG_INF("CLK", _hasRtc ? "SDK RTC found" : "RTC not found, using system clock");

  // Runs before TrustedTime restores its floor: the system clock still holds
  // a real time only if it ran through the reset (deep sleep, soft restart).
  // After a power loss it restarts near 1970 and the old sync no longer counts.
  int64_t saved = 0;
  {
    Preferences prefs;
    if (prefs.begin(PREFS_NAMESPACE, /*readOnly=*/true)) {
      saved = prefs.getLong64(PREFS_SYNC, 0);
      prefs.end();
    }
  }
  if (saved > 0 && time(nullptr) < MIN_VALID_EPOCH) {
    LOG_INF("CLK", "System clock reset since the last sync; time unknown until next sync");
    saved = 0;
    storeLastSync(0);
  }
  portENTER_CRITICAL(&syncLock);
  lastSync = saved;
  portEXIT_CRITICAL(&syncLock);
}

bool HalClock::timeValid() const {
  struct tm t;
  return localTime(t);
}

void HalClock::noteSystemTimeSynced() {
  const time_t now = time(nullptr);
  if (now < MIN_VALID_EPOCH) return;
  portENTER_CRITICAL(&syncLock);
  lastSync = static_cast<int64_t>(now);
  portEXIT_CRITICAL(&syncLock);
  storeLastSync(static_cast<int64_t>(now));
}

time_t HalClock::lastSyncUtc() const {
  portENTER_CRITICAL(&syncLock);
  const time_t last = static_cast<time_t>(lastSync);
  portEXIT_CRITICAL(&syncLock);
  return last;
}

namespace {
// UTC calendar date -> Unix epoch, no timezone involvement (newlib has no
// timegm). Days-from-civil per Howard Hinnant's algorithm.
time_t epochFromUtc(const Rtc::DateTime& dt) {
  int y = dt.year;
  const int m = dt.month;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + dt.day - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const long days = static_cast<long>(era) * 146097L + static_cast<long>(doe) - 719468L;
  return static_cast<time_t>(days) * 86400 + dt.hour * 3600L + dt.minute * 60L + dt.second;
}
}  // namespace

void HalClock::setTimezone(const char* posixTz) {
  setenv("TZ", posixTz && posixTz[0] != '\0' ? posixTz : "UTC0", 1);
  tzset();
  _lastPollMs = 0;  // re-derive local time under the new rule immediately
}

bool HalClock::nowUtc(time_t& out) const {
  if (!_available) return false;
  if (!_hasRtc) {
    // Only trust the system clock once NTP has set it this power cycle: after
    // a power loss TrustedTime restores a persisted floor, which is plausible
    // but stale.
    const time_t now = time(nullptr);
    if (now < MIN_VALID_EPOCH || lastSyncUtc() == 0) return false;
    out = now;
    return true;
  }

  const unsigned long now = millis();
  if (_lastPollMs == 0 || (now - _lastPollMs) >= CLOCK_POLL_MS) {
    Rtc::DateTime dt;
    if (_sdkRtc.now(dt)) {
      _cachedUtc = epochFromUtc(dt);
      _hasCachedTime = true;
    } else if (!_hasCachedTime) {
      return false;
    }
    _lastPollMs = now != 0 ? now : 1;  // 0 doubles as the invalidation sentinel
  }
  out = _cachedUtc;
  return true;
}

bool HalClock::localTime(struct tm& out) const {
  time_t now;
  if (!nowUtc(now)) return false;
  localtime_r(&now, &out);
  return true;
}

bool HalClock::utcTime(struct tm& out) const {
  time_t now;
  if (!nowUtc(now)) return false;
  gmtime_r(&now, &out);
  return true;
}

bool HalClock::getTime(uint8_t& hour, uint8_t& minute) const {
  struct tm local;
  if (!localTime(local)) return false;
  hour = static_cast<uint8_t>(local.tm_hour);
  minute = static_cast<uint8_t>(local.tm_min);
  return true;
}

bool HalClock::formatTime(char* buf, size_t bufSize, bool use12Hour) const {
  if (bufSize < (use12Hour ? 9u : 6u)) return false;
  struct tm local;
  if (!localTime(local)) return false;

  if (use12Hour) {
    const bool pm = local.tm_hour >= 12;
    int hour12 = local.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, bufSize, "%d:%02d %s", hour12, local.tm_min, pm ? "PM" : "AM");
  } else {
    snprintf(buf, bufSize, "%02d:%02d", local.tm_hour, local.tm_min);
  }
  return true;
}

bool HalClock::syncFromNTP() {
  if (!_available) return false;

  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERR("CLK", "WiFi not connected, cannot sync NTP");
    return false;
  }

  LOG_INF("CLK", "Starting NTP sync...");
  // configTzTime overwrites the process TZ with UTC0 for the SNTP exchange;
  // remember the display timezone so it can be restored below.
  const char* tzBefore = getenv("TZ");
  char savedTz[64] = {0};
  if (tzBefore) snprintf(savedTz, sizeof(savedTz), "%s", tzBefore);
  configTzTime("UTC0", "pool.ntp.org", "time.nist.gov");

  // Wait for SNTP sync to complete (up to 8 seconds)
  constexpr int maxAttempts = 80;
  for (int i = 0; i < maxAttempts; i++) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && time(nullptr) >= MIN_VALID_EPOCH) {
      time_t now = time(nullptr);
      struct tm timeinfo;
      gmtime_r(&now, &timeinfo);

      Rtc::DateTime dt;
      dt.year = static_cast<uint16_t>(timeinfo.tm_year + 1900);
      dt.month = static_cast<uint8_t>(timeinfo.tm_mon + 1);
      dt.day = static_cast<uint8_t>(timeinfo.tm_mday);
      dt.hour = static_cast<uint8_t>(timeinfo.tm_hour);
      dt.minute = static_cast<uint8_t>(timeinfo.tm_min);
      dt.second = static_cast<uint8_t>(timeinfo.tm_sec);
      dt.weekday = static_cast<uint8_t>(timeinfo.tm_wday);
      const bool ok = !_hasRtc || _sdkRtc.set(dt);
      if (ok) noteSystemTimeSynced();
      if (ok && _hasRtc) {
        _cachedUtc = epochFromUtc(dt);
        _hasCachedTime = true;
        _lastPollMs = 0;
        LOG_INF("CLK", "Clock set to %04u-%02u-%02u %02u:%02u:%02u UTC", dt.year, dt.month, dt.day, dt.hour, dt.minute,
                dt.second);
      }
      setTimezone(savedTz);
      return ok;
    }
    delay(100);
  }

  LOG_ERR("CLK", "NTP sync timed out");
  setTimezone(savedTz);
  return false;
}
