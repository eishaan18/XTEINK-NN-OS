#pragma once
#include <chrono>
#include <cstdint>
#include <thread>
inline unsigned long millis() {
  using namespace std::chrono;
  static const auto t0 = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - t0).count());
}
inline void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
