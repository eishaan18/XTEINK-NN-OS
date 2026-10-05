#pragma once

#include "NeuronData.h"

namespace freeink {
class SecureHttpClient;
}

// Online data for the Nautical Neuron apps. All calls block and need WiFi.
//   Weather: Open-Meteo (free, no API key).
//   Markets: Yahoo Finance chart API (unofficial; delayed quotes).
namespace neuron {

bool fetchWeather(const Config& cfg, Weather& out);
bool fetchQuote(freeink::SecureHttpClient& http, const Symbol& sym, Quote& out);
// Returns the number of symbols refreshed (0 = nothing usable, `out` untouched).
int fetchMarkets(const Config& cfg, Markets& out, void (*progress)(int done, int total, void* ctx) = nullptr,
                 void* ctx = nullptr);

}  // namespace neuron
