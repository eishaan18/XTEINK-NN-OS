// Host checks for the Nautical Neuron data, formatting and network parsing
// code. Built and run by test.sh against a throwaway SD directory.
#include <SecureHttpClient.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "neuron/NeuronData.h"
#include "neuron/NeuronDraw.h"
#include "neuron/NeuronNet.h"

namespace {
int failures = 0;
int checks = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    ++checks;                                                           \
    if (!(cond)) {                                                      \
      ++failures;                                                       \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
    }                                                                   \
  } while (0)
#define CHECK_STR(a, b) CHECK(std::string(a) == std::string(b))
#define CHECK_NEAR(a, b) CHECK(std::fabs((a) - (b)) < 0.01)

std::string sd() { return getenv("NEURON_SD"); }
void writeFile(const std::string& rel, const std::string& text) {
  std::filesystem::create_directories(std::filesystem::path(sd() + rel).parent_path());
  std::ofstream(sd() + rel, std::ios::binary) << text;
}
std::string readFile(const std::string& rel) {
  std::ifstream f(sd() + rel, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void testFormatting() {
  char b[48];
  neuron::formatPrice(2480531.5f, b, sizeof(b));
  CHECK_STR(b, "24,80,531.50");
  neuron::formatPrice(100000.0f, b, sizeof(b));
  CHECK_STR(b, "1,00,000.00");
  neuron::formatPrice(1234.5f, b, sizeof(b));
  CHECK_STR(b, "1,234.50");
  neuron::formatPrice(999.0f, b, sizeof(b));
  CHECK_STR(b, "999.00");
  neuron::formatPrice(0.5f, b, sizeof(b));
  CHECK_STR(b, "0.50");
  neuron::formatPrice(-1234.25f, b, sizeof(b));
  CHECK_STR(b, "-1,234.25");
  neuron::formatChange(105.0f, 100.0f, b, sizeof(b));
  CHECK_STR(b, "+5.00 (+5.00%)");
  neuron::formatChange(95.0f, 100.0f, b, sizeof(b));
  CHECK_STR(b, "-5.00 (-5.00%)");
  neuron::formatChange(95.0f, 0.0f, b, sizeof(b));
  CHECK_STR(b, "--");
}

void testDates() {
  CHECK(neuron::daysBetween(20261005, 20261006) == 1);
  CHECK(neuron::daysBetween(20261231, 20270101) == 1);
  CHECK(neuron::daysBetween(20280228, 20280301) == 2);  // leap year
  CHECK(neuron::daysBetween(20261006, 20261005) == -1);
  CHECK(neuron::daysBetween(0, 20261005) == 0);
  struct tm t{};
  t.tm_year = 126;
  t.tm_mon = 9;
  t.tm_mday = 5;
  CHECK(neuron::dayKey(t) == 20261005);
}

void testHabits() {
  // Saved on Oct 3 with Oct 3 and Oct 2 done; loaded on Oct 5. Unknown keys are ignored.
  writeFile("/neuron/habits.txt", "# drills\nworkout|20261003|11\nsteps|20260801|1111\nyoga|20261005|1\n");
  neuron::Habits h;
  CHECK(neuron::loadHabits(h, 20261005));
  CHECK(h.bits[neuron::HABIT_WORKOUT] == 0b1100ULL);  // shifted two days
  CHECK(h.bits[neuron::HABIT_STEPS] == 0);            // older than the 60-day window
  CHECK(neuron::habitStreak(0b1111ULL) == 4);
  CHECK(neuron::habitStreak(0b1110ULL) == 3);  // today not done yet: streak through yesterday
  CHECK(neuron::habitStreak(0b1100ULL) == 0);
  CHECK(neuron::habitStreak(0) == 0);

  h.bits[neuron::HABIT_STEPS] |= 1;
  CHECK(neuron::saveHabits(h, 20261005));
  CHECK(readFile("/neuron/habits.txt").find("\nworkout|20261005|0011") != std::string::npos);
  CHECK(readFile("/neuron/habits.txt").find("\nsteps|20261005|1000") != std::string::npos);
  neuron::Habits again;
  CHECK(neuron::loadHabits(again, 20261006));
  CHECK(again.bits[neuron::HABIT_STEPS] == 0b10ULL);
  CHECK(again.bits[neuron::HABIT_WORKOUT] == 0b11000ULL);

  // Same day, no date (clock unknown): no shift.
  CHECK(neuron::loadHabits(again, 0));
  CHECK(again.bits[neuron::HABIT_STEPS] == 0b1ULL);

  std::filesystem::remove(sd() + "/neuron/habits.txt");
  CHECK(!neuron::loadHabits(again, 20261005));
  CHECK(again.bits[0] == 0 && again.bits[1] == 0);
  CHECK_STR(neuron::habitKey(neuron::HABIT_WORKOUT), "workout");
  CHECK_STR(neuron::habitKey(neuron::HABIT_STEPS), "steps");
}

void testConfig() {
  std::filesystem::remove(sd() + "/neuron/config.txt");
  neuron::Config cfg;
  neuron::loadConfig(cfg);
  CHECK(std::filesystem::exists(sd() + "/neuron/config.txt"));
  CHECK_STR(cfg.place, "Pune");
  CHECK_NEAR(cfg.lat, 18.5204f);
  CHECK(cfg.symbolCount == 5);
  CHECK_STR(cfg.symbols[0].ticker, "^NSEI");
  CHECK_STR(cfg.symbols[0].label, "NIFTY 50");
  CHECK_STR(cfg.symbols[4].ticker, "^INDIAVIX");

  writeFile("/neuron/config.txt",
            "place=Mumbai\nlat=abc\nlon=72.87\nsymbols= ^CNXIT , ^NSEI:Nifty|50, :NoTicker,TATAMOTORS.BO:Tata,^BSESN\n");
  neuron::loadConfig(cfg);
  CHECK_STR(cfg.place, "Mumbai");
  CHECK_NEAR(cfg.lat, 18.5204f);  // malformed: default kept
  CHECK_NEAR(cfg.lon, 72.87f);
  CHECK(cfg.symbolCount == 3);  // stocks and blank tickers dropped: indices only
  CHECK_STR(cfg.symbols[0].ticker, "^CNXIT");
  CHECK_STR(cfg.symbols[0].label, "CNXIT");
  CHECK_STR(cfg.symbols[1].label, "Nifty/50");
  CHECK_STR(cfg.symbols[2].ticker, "^BSESN");
}

void testCaches() {
  neuron::Weather w;
  w.valid = true;
  w.updated = 1791194400;
  snprintf(w.place, sizeof(w.place), "Pune");
  w.temp = 27.4f;
  w.feelsLike = 30.1f;
  w.humidity = 74;
  w.wind = 9.4f;
  w.code = 2;
  w.dayCount = 2;
  snprintf(w.days[0].date, sizeof(w.days[0].date), "2026-10-05");
  w.days[0].code = 61;
  w.days[0].tMax = 30.8f;
  w.days[0].tMin = 21.9f;
  w.days[0].rainChance = 35;
  snprintf(w.days[0].sunrise, sizeof(w.days[0].sunrise), "06:24");
  snprintf(w.days[0].sunset, sizeof(w.days[0].sunset), "18:18");
  snprintf(w.days[1].date, sizeof(w.days[1].date), "2026-10-06");
  CHECK(neuron::saveWeather(w));
  neuron::Weather r;
  CHECK(neuron::loadWeather(r));
  CHECK(r.updated == w.updated && r.dayCount == 2 && r.humidity == 74 && r.code == 2);
  CHECK_NEAR(r.temp, 27.4f);
  CHECK_STR(r.days[0].sunrise, "06:24");
  CHECK_STR(r.days[1].sunrise, "--:--");
  CHECK(r.days[0].code == 61 && r.days[0].rainChance == 35);
  CHECK(neuron::isoDateKey("2026-10-06") == 20261006);
  CHECK(neuron::isoDateKey("garbage") == 0);
  CHECK(neuron::weatherDayIndex(r, 20261005) == 0);
  CHECK(neuron::weatherDayIndex(r, 20261006) == 1);
  CHECK(neuron::weatherDayIndex(r, 20261009) == -1);
  CHECK(neuron::weatherDayIndex(r, 0) == -1);

  neuron::Markets m;
  m.updated = 1791194400;
  m.count = 2;
  snprintf(m.quotes[0].ticker, sizeof(m.quotes[0].ticker), "^NSEI");
  snprintf(m.quotes[0].label, sizeof(m.quotes[0].label), "NIFTY 50");
  m.quotes[0].price = 25731.04f;
  m.quotes[0].prevClose = 25456.52f;
  m.quotes[0].closeCount = 3;
  m.quotes[0].closes[0] = 25000.0f;
  m.quotes[0].closes[1] = 25456.52f;
  m.quotes[0].closes[2] = 25731.04f;
  snprintf(m.quotes[1].ticker, sizeof(m.quotes[1].ticker), "TCS.NS");
  snprintf(m.quotes[1].label, sizeof(m.quotes[1].label), "TCS");
  CHECK(neuron::saveMarkets(m));
  neuron::Markets mr;
  CHECK(neuron::loadMarkets(mr));
  CHECK(mr.count == 2 && mr.updated == m.updated);
  CHECK_NEAR(mr.quotes[0].price, 25731.04f);
  CHECK(mr.quotes[0].closeCount == 3 && mr.quotes[0].ok);
  CHECK(mr.quotes[1].closeCount == 0 && !mr.quotes[1].ok);
}

void testFocusAndQuotes() {
  std::filesystem::remove(sd() + "/neuron/cache/focus.txt");
  CHECK(neuron::loadFocusDay(20261005).sessions == 0);
  neuron::addFocusSession(20261005, 25);
  neuron::addFocusSession(20261005, 30);
  const auto d = neuron::loadFocusDay(20261005);
  CHECK(d.sessions == 2 && d.minutes == 55);
  CHECK(neuron::loadFocusDay(20261006).sessions == 0);

  std::string text, author;
  std::filesystem::remove(sd() + "/neuron/quotes.txt");
  neuron::quoteOfDay(0, text, author);
  CHECK(!text.empty() && !author.empty());
  writeFile("/neuron/quotes.txt", "# mine\nStay the course -- Eishaan\nNo author here\n");
  neuron::quoteOfDay(0, text, author);
  CHECK_STR(text, "Stay the course");
  CHECK_STR(author, "Eishaan");
  neuron::quoteOfDay(1, text, author);
  CHECK_STR(text, "No author here");
  CHECK(author.empty());
  std::filesystem::remove(sd() + "/neuron/quotes.txt");
}

// Shapes follow the public Open-Meteo forecast and Yahoo chart responses.
const char* OPEN_METEO =
    R"({"latitude":18.5,"longitude":73.875,"generationtime_ms":0.05,"utc_offset_seconds":19800,)"
    R"("timezone":"Asia/Kolkata","timezone_abbreviation":"GMT+5:30","elevation":560.0,)"
    R"("current_units":{"time":"iso8601","interval":"seconds","temperature_2m":"°C"},)"
    R"("current":{"time":"2026-10-05T17:30","interval":900,"temperature_2m":27.4,"apparent_temperature":30.1,)"
    R"("relative_humidity_2m":74,"weather_code":2,"wind_speed_10m":9.4},)"
    R"("daily_units":{"time":"iso8601","weather_code":"wmo code"},)"
    R"("daily":{"time":["2026-10-05","2026-10-06","2026-10-07","2026-10-08"],"weather_code":[2,61,3,0],)"
    R"("temperature_2m_max":[30.8,29.1,29.9,31.2],"temperature_2m_min":[21.9,21.5,21.2,20.8],)"
    R"("precipitation_probability_max":[35,70,40,null],)"
    R"("sunrise":["2026-10-05T06:24","2026-10-06T06:24","2026-10-07T06:25","2026-10-08T06:25"],)"
    R"("sunset":["2026-10-05T18:18","2026-10-06T18:17","2026-10-07T18:16","2026-10-08T18:15"]}})";

std::string yahoo(const char* symbol, double price, long long marketTime, const std::string& stamps,
                  const std::string& closes) {
  char meta[512];
  snprintf(meta, sizeof(meta),
           R"({"currency":"INR","symbol":"%s","exchangeName":"NSI","instrumentType":"INDEX",)"
           R"("regularMarketTime":%lld,"gmtoffset":19800,"timezone":"IST","regularMarketPrice":%.2f,)"
           R"("chartPreviousClose":99.0,"priceHint":2,"dataGranularity":"1d","range":"1mo"})",
           symbol, marketTime, price);
  return std::string(R"({"chart":{"result":[{"meta":)") + meta + R"(,"timestamp":[)" + stamps +
         R"(],"indicators":{"quote":[{"open":[1,2,3,4,5],"close":[)" + closes +
         R"(],"high":[1,2,3,4,5],"low":[1,2,3,4,5],"volume":[0,0,0,0,0]}],"adjclose":[{"adjclose":[)" + closes +
         R"(]}]}}],"error":null}})";
}

void testNetwork() {
  auto& canned = freeink::cannedResponses();
  canned.clear();
  canned.push_back({"api.open-meteo.com", 200, OPEN_METEO});

  std::filesystem::remove(sd() + "/neuron/config.txt");  // defaults: Pune
  neuron::Config cfg;
  neuron::loadConfig(cfg);
  neuron::Weather w;
  CHECK(neuron::fetchWeather(cfg, w));
  CHECK(w.valid && w.dayCount == 4 && w.code == 2 && w.humidity == 74);
  CHECK_NEAR(w.temp, 27.4f);
  CHECK_NEAR(w.feelsLike, 30.1f);
  CHECK_STR(w.days[1].date, "2026-10-06");
  CHECK(w.days[1].code == 61 && w.days[1].rainChance == 70 && w.days[3].rainChance == 0);
  CHECK_STR(w.days[0].sunrise, "06:24");
  CHECK_STR(w.days[3].sunset, "18:15");
  CHECK(freeink::requestedUrls().back().find("latitude=18.5204&longitude=73.8567") != std::string::npos);

  // Bars: Sep 29, Sep 30, Oct 1, Oct 2 (holiday, null), Oct 5 09:15 IST.
  const long long oct5 = 1791171900;
  const std::string stamps = std::to_string(oct5 - 6 * 86400) + "," + std::to_string(oct5 - 5 * 86400) + "," +
                             std::to_string(oct5 - 4 * 86400) + "," + std::to_string(oct5 - 3 * 86400) + "," +
                             std::to_string(oct5);
  freeink::SecureHttpClient http;
  neuron::Symbol sym;
  snprintf(sym.ticker, sizeof(sym.ticker), "^NSEI");
  snprintf(sym.label, sizeof(sym.label), "NIFTY 50");

  // Live session already has a bar: previous close is Oct 1.
  canned.clear();
  canned.push_back({"chart/%5ENSEI", 200, yahoo("^NSEI", 104.5, 1791194400, stamps, "100,101,102,null,104")});
  neuron::Quote q;
  CHECK(neuron::fetchQuote(http, sym, q));
  CHECK_NEAR(q.price, 104.5f);
  CHECK_NEAR(q.prevClose, 102.0f);
  CHECK(q.closeCount == 4);
  CHECK_NEAR(q.closes[3], 104.5f);
  CHECK(freeink::requestedUrls().back().find("query1.finance.yahoo.com/v8/finance/chart/%5ENSEI?range=1mo") !=
        std::string::npos);

  // Newest bar is from an earlier day: it is the previous close; live price appended.
  canned.clear();
  const std::string oldStamps = stamps.substr(0, stamps.rfind(','));
  canned.push_back({"chart/%5ENSEI", 200, yahoo("^NSEI", 104.5, 1791171900 + 600, oldStamps, "100,101,102,null")});
  CHECK(neuron::fetchQuote(http, sym, q));
  CHECK_NEAR(q.prevClose, 102.0f);
  CHECK(q.closeCount == 4);
  CHECK_NEAR(q.closes[3], 104.5f);

  // query1 rate-limited: falls back to query2.
  canned.clear();
  canned.push_back({"query1.finance", 429, "Too Many Requests"});
  canned.push_back({"query2.finance", 200, yahoo("^NSEI", 104.5, 1791194400, stamps, "100,101,102,null,104")});
  CHECK(neuron::fetchQuote(http, sym, q));
  CHECK(freeink::requestedUrls().back().find("query2.finance") != std::string::npos);

  // Not found anywhere: failure, and fetchMarkets keeps the cached quote.
  canned.clear();
  canned.push_back({"chart/%5ENSEI", 404, R"({"chart":{"result":null,"error":{"code":"Not Found"}}})"});
  CHECK(!neuron::fetchQuote(http, sym, q));

  writeFile("/neuron/config.txt", "symbols=^NSEI:NIFTY 50,^BSESN:SENSEX\n");
  neuron::loadConfig(cfg);
  canned.push_back({"chart/%5EBSESN", 200, yahoo("^BSESN", 3000.0, 1791194400, stamps, "2900,2950,2980,null,2990")});
  neuron::Markets cached;
  cached.count = 1;
  snprintf(cached.quotes[0].ticker, sizeof(cached.quotes[0].ticker), "^NSEI");
  cached.quotes[0].price = 25000.0f;
  cached.quotes[0].ok = true;
  neuron::saveMarkets(cached);
  neuron::Markets m;
  CHECK(neuron::fetchMarkets(cfg, m) == 1);
  CHECK(m.count == 2);
  CHECK_NEAR(m.quotes[0].price, 25000.0f);
  CHECK_STR(m.quotes[0].label, "NIFTY 50");
  CHECK_NEAR(m.quotes[1].price, 3000.0f);
  CHECK_NEAR(m.quotes[1].prevClose, 2980.0f);

  canned.clear();
  neuron::Markets untouched;
  untouched.count = 7;
  CHECK(neuron::fetchMarkets(cfg, untouched) == 0);
  CHECK(untouched.count == 7);
  CHECK(!neuron::fetchWeather(cfg, w));
}
}  // namespace

int main() {
  testFormatting();
  testDates();
  testHabits();
  testConfig();
  testCaches();
  testFocusAndQuotes();
  testNetwork();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
