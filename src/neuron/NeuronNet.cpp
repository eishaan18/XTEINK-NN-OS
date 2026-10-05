#include "NeuronNet.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <Memory.h>
#include <SecureHttpClient.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace neuron {

namespace {

constexpr size_t MAX_RESPONSE = 32 * 1024;
constexpr const char* USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) NauticalNeuron/1.0";

// GET into `out`, capped at MAX_RESPONSE. Returns the HTTP status or -1.
int httpGet(freeink::SecureHttpClient& http, const char* url, std::string& out) {
  out.clear();
  http.setInsecure();
  http.setTimeout(20000);
  http.setUserAgent(USER_AGENT);
  http.setFollowRedirects(2);
  if (!http.begin(url)) {
    LOG_ERR("NNNET", "Bad URL: %s", url);
    return -1;
  }
  http.addHeader("Accept", "application/json");
  bool overflow = false;
  const int status = http.GET([&](const uint8_t* data, const size_t len) {
    if (out.size() + len > MAX_RESPONSE) {
      overflow = true;
      return false;
    }
    if (out.capacity() < out.size() + len) {
      const size_t want = std::min(MAX_RESPONSE, std::max(out.size() + len, out.capacity() * 2 + 512));
      if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < want + 1024) {
        overflow = true;
        return false;
      }
      out.reserve(want);
    }
    out.append(reinterpret_cast<const char*>(data), len);
    return true;
  });
  if (overflow || status < 0 || !http.responseComplete()) {
    LOG_ERR("NNNET", "GET failed status=%d overflow=%d url=%s", status, overflow, url);
    return -1;
  }
  return status;
}

// "2026-10-05T06:22" -> "06:22"
void copyClock(char* dst, const size_t cap, const char* iso) {
  const char* t = iso ? strchr(iso, 'T') : nullptr;
  snprintf(dst, cap, "%s", t ? t + 1 : "--:--");
}

// Percent-encode the few characters Yahoo tickers use (^ and spaces).
void encodeTicker(const char* ticker, char* out, const size_t cap) {
  size_t o = 0;
  for (const char* p = ticker; *p && o + 4 < cap; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (isalnum(c) || c == '.' || c == '-' || c == '_' || c == '=') {
      out[o++] = static_cast<char>(c);
    } else {
      o += snprintf(out + o, cap - o, "%%%02X", c);
    }
  }
  out[o] = '\0';
}

}  // namespace

bool fetchWeather(const Config& cfg, Weather& out) {
  constexpr size_t URL_CAP = 400;
  auto url = makeUniqueNoThrow<char[]>(URL_CAP);
  if (!url) {
    LOG_ERR("NNNET", "OOM: weather URL");
    return false;
  }
  snprintf(url.get(), URL_CAP,
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
           "&timezone=auto&forecast_days=4",
           cfg.lat, cfg.lon);

  freeink::SecureHttpClient http;
  std::string body;
  const int status = httpGet(http, url.get(), body);
  http.end();
  url.reset();
  if (status != 200) {
    LOG_ERR("NNNET", "Weather: HTTP %d", status);
    return false;
  }

  // Only the blocks we read; the *_units objects are skipped.
  JsonDocument filter;
  filter["current"] = true;
  filter["daily"] = true;
  JsonDocument doc;
  const auto err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  body.clear();
  body.shrink_to_fit();
  if (err) {
    LOG_ERR("NNNET", "Weather JSON: %s", err.c_str());
    return false;
  }

  JsonObjectConst current = doc["current"];
  JsonObjectConst daily = doc["daily"];
  JsonArrayConst dates = daily["time"];
  if (current.isNull() || current["temperature_2m"].isNull() || dates.isNull()) {
    LOG_ERR("NNNET", "Weather: unexpected response");
    return false;
  }

  out = Weather{};
  snprintf(out.place, sizeof(out.place), "%s", cfg.place);
  out.updated = time(nullptr);
  out.temp = current["temperature_2m"].as<float>();
  out.feelsLike = current["apparent_temperature"] | out.temp;
  out.humidity = current["relative_humidity_2m"].as<int>();
  out.wind = current["wind_speed_10m"].as<float>();
  out.code = current["weather_code"].as<int>();

  const size_t n = std::min<size_t>(4, dates.size());
  for (size_t i = 0; i < n; ++i) {
    WeatherDay& d = out.days[i];
    snprintf(d.date, sizeof(d.date), "%s", dates[i] | "");
    d.code = daily["weather_code"][i].as<int>();
    d.tMax = daily["temperature_2m_max"][i].as<float>();
    d.tMin = daily["temperature_2m_min"][i].as<float>();
    d.rainChance = daily["precipitation_probability_max"][i].as<int>();
    copyClock(d.sunrise, sizeof(d.sunrise), daily["sunrise"][i] | "");
    copyClock(d.sunset, sizeof(d.sunset), daily["sunset"][i] | "");
  }
  out.dayCount = static_cast<uint8_t>(n);
  out.valid = true;
  return true;
}

bool fetchQuote(freeink::SecureHttpClient& http, const Symbol& sym, Quote& q) {
  q = Quote{};
  snprintf(q.ticker, sizeof(q.ticker), "%s", sym.ticker);
  snprintf(q.label, sizeof(q.label), "%s", sym.label);

  char encoded[48];
  encodeTicker(sym.ticker, encoded, sizeof(encoded));
  // query1 first; query2 serves the same API and is the usual fallback when
  // one front end rate-limits (HTTP 429) or refuses the client.
  std::string body;
  int status = -1;
  for (const char* host : {"query1", "query2"}) {
    char url[160];
    snprintf(url, sizeof(url), "https://%s.finance.yahoo.com/v8/finance/chart/%s?range=1mo&interval=1d", host, encoded);
    status = httpGet(http, url, body);
    if (status == 200) break;
    LOG_ERR("NNNET", "Quote %s via %s: HTTP %d", sym.ticker, host, status);
  }
  if (status != 200) return false;

  JsonDocument filter;
  JsonObject fr = filter["chart"]["result"].add<JsonObject>();
  fr["meta"]["regularMarketPrice"] = true;
  fr["meta"]["regularMarketTime"] = true;
  fr["meta"]["gmtoffset"] = true;
  fr["meta"]["chartPreviousClose"] = true;
  fr["timestamp"] = true;
  fr["indicators"]["quote"].add<JsonObject>()["close"] = true;

  JsonDocument doc;
  const auto err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  body.clear();
  body.shrink_to_fit();
  if (err) {
    LOG_ERR("NNNET", "Quote JSON %s: %s", sym.ticker, err.c_str());
    return false;
  }

  JsonObjectConst result = doc["chart"]["result"][0];
  if (result.isNull()) return false;
  JsonObjectConst meta = result["meta"];
  const float price = meta["regularMarketPrice"].as<float>();
  const long long marketTime = meta["regularMarketTime"].as<long long>();
  const long gmtOffset = meta["gmtoffset"].as<long>();
  JsonArrayConst stamps = result["timestamp"];
  JsonArrayConst closes = result["indicators"]["quote"][0]["close"];

  // Keep the newest MAX_CLOSES non-null closes, remembering the day of the last one.
  const size_t total = std::min(stamps.size(), closes.size());
  float buf[MAX_CLOSES];
  uint8_t count = 0;
  long long lastStamp = 0;
  for (size_t i = 0; i < total; ++i) {
    if (closes[i].isNull()) continue;
    const float v = closes[i].as<float>();
    if (count == MAX_CLOSES) {
      memmove(buf, buf + 1, sizeof(float) * (MAX_CLOSES - 1));
      count--;
    }
    buf[count++] = v;
    lastStamp = stamps[i].as<long long>();
  }

  q.price = price > 0 ? price : (count ? buf[count - 1] : 0);
  // The previous session's close: if the newest daily bar is the same trading
  // day as the live price, it is that session's running bar, so step back one.
  const bool lastBarIsToday = count > 0 && marketTime > 0 && lastStamp > 0 &&
                              (lastStamp + gmtOffset) / 86400 == (marketTime + gmtOffset) / 86400;
  if (lastBarIsToday && count >= 2) {
    q.prevClose = buf[count - 2];
  } else if (count >= 1) {
    q.prevClose = buf[count - 1];
  } else {
    q.prevClose = meta["chartPreviousClose"] | q.price;
  }
  if (lastBarIsToday) {
    buf[count - 1] = q.price;
  } else if (price > 0) {
    // No bar for the live session yet: extend the sparkline with the live price.
    if (count == MAX_CLOSES) {
      memmove(buf, buf + 1, sizeof(float) * (MAX_CLOSES - 1));
      count--;
    }
    buf[count++] = price;
  }
  memcpy(q.closes, buf, sizeof(float) * count);
  q.closeCount = count;
  q.ok = q.price > 0;
  return q.ok;
}

int fetchMarkets(const Config& cfg, Markets& out, void (*progress)(int done, int total, void* ctx), void* ctx) {
  // Two ~1.5 KB snapshots: heap, not the loop task's stack. Symbols that fail
  // this round keep their previously cached values.
  auto previous = makeUniqueNoThrow<Markets>();
  auto m = makeUniqueNoThrow<Markets>();
  if (!previous || !m) {
    LOG_ERR("NNNET", "OOM: markets snapshot");
    return 0;
  }
  loadMarkets(*previous);

  m->updated = time(nullptr);
  freeink::SecureHttpClient http;  // one kept-alive TLS session for every symbol
  int okCount = 0;
  for (int i = 0; i < cfg.symbolCount; ++i) {
    if (progress) progress(i, cfg.symbolCount, ctx);
    Quote& q = m->quotes[m->count];
    if (fetchQuote(http, cfg.symbols[i], q)) {
      ++okCount;
    } else {
      for (int p = 0; p < previous->count; ++p) {
        if (strcmp(previous->quotes[p].ticker, cfg.symbols[i].ticker) == 0) {
          q = previous->quotes[p];
          snprintf(q.label, sizeof(q.label), "%s", cfg.symbols[i].label);
          break;
        }
      }
    }
    m->count++;
  }
  http.end();
  if (progress) progress(cfg.symbolCount, cfg.symbolCount, ctx);
  if (okCount == 0) return 0;
  m->valid = true;
  out = *m;
  return okCount;
}

}  // namespace neuron
