#pragma once

#include <ctime>

// Wall clock for the Nautical Neuron screens, on top of HalClock. Boards with
// an RTC chip (X3) keep time on their own; the X4 has none, so HalClock uses
// the ESP32 system clock, which survives deep sleep and soft restarts but is
// only trusted after an NTP sync this power cycle and drifts between syncs.
namespace neuron {

// Local time; false when the time is not known.
bool localNow(struct tm& out);
// GMT/UTC time; false when the time is not known.
bool utcNow(struct tm& out);
// Seconds since the last NTP sync on RTC-less boards; -1 with an RTC or when unknown.
long secondsSinceSync();
// NTP sync (WiFi must be connected).
bool syncClockFromNtp();

}  // namespace neuron
