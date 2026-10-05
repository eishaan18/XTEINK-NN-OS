#include "NeuronClock.h"

#include <HalClock.h>

#include "CrossPointSettings.h"

namespace neuron {

bool localNow(struct tm& out) { return halClock.localTime(out); }

bool utcNow(struct tm& out) { return halClock.utcTime(out); }

long secondsSinceSync() {
  if (halClock.hasRtc()) return -1;
  const time_t last = halClock.lastSyncUtc();
  const time_t now = time(nullptr);
  if (last <= 0 || now < last) return -1;
  return static_cast<long>(now - last);
}

bool syncClockFromNtp() {
  if (!halClock.syncFromNTP()) return false;
  if (!SETTINGS.clockHasBeenSynced) {
    SETTINGS.clockHasBeenSynced = 1;
    SETTINGS.saveToFile();
  }
  return true;
}

}  // namespace neuron
