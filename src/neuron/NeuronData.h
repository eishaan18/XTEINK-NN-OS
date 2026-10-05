#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

// Nautical Neuron data layer. Everything lives in plain text under /neuron on
// the SD card so it can be edited from a computer or the web file manager:
//
//   /neuron/config.txt       place, lat/lon, market indices (key=value)
//   /neuron/habits.txt       "workout|YYYYMMDD|bits" (bits[0] = that day)
//   /neuron/quotes.txt       optional, one quote per line (overrides built-ins)
//   /neuron/cache/*.txt      weather, markets, focus log
namespace neuron {

constexpr const char* DIR = "/neuron";
constexpr const char* CACHE_DIR = "/neuron/cache";
constexpr const char* CONFIG_PATH = "/neuron/config.txt";
constexpr const char* HABITS_PATH = "/neuron/habits.txt";
constexpr const char* QUOTES_PATH = "/neuron/quotes.txt";
constexpr const char* WEATHER_PATH = "/neuron/cache/weather.txt";
constexpr const char* MARKETS_PATH = "/neuron/cache/markets.txt";
constexpr const char* FOCUS_PATH = "/neuron/cache/focus.txt";

constexpr size_t MAX_SYMBOLS = 10;
constexpr size_t MAX_CLOSES = 24;
constexpr int HABIT_DAYS = 60;

void ensureDirs();

// YYYYMMDD for a local calendar date.
int dayKey(const struct tm& t);
// Whole days from a to b (b - a); both YYYYMMDD.
int daysBetween(int fromKey, int toKey);

// --- config ------------------------------------------------------------------
struct Symbol {
  char ticker[20] = {0};  // Yahoo Finance index ticker, e.g. ^NSEI
  char label[20] = {0};   // display name
};

struct Config {
  char place[32] = {0};
  float lat = 0;
  float lon = 0;
  Symbol symbols[MAX_SYMBOLS];
  uint8_t symbolCount = 0;
};

// Loads /neuron/config.txt, writing the defaults first if it is missing.
// Only index tickers (starting with '^') are accepted for markets.
void loadConfig(Config& out);

// --- habits ------------------------------------------------------------------
// Two fixed daily drills. bits: bit 0 = today, bit 1 = yesterday, ...
enum HabitId : uint8_t { HABIT_WORKOUT = 0, HABIT_STEPS = 1, HABIT_COUNT = 2 };

struct Habits {
  uint64_t bits[HABIT_COUNT] = {0, 0};
};

// Stable key stored in habits.txt.
const char* habitKey(int id);
// Loads and shifts each history so bit 0 is `todayKey` (0 = date unknown).
bool loadHabits(Habits& out, int todayKey);
bool saveHabits(const Habits& habits, int todayKey);
// Consecutive days ending today (or yesterday, if today is not yet done).
int habitStreak(uint64_t bits);

// --- weather cache -----------------------------------------------------------
struct WeatherDay {
  int code = 0;
  float tMax = 0;
  float tMin = 0;
  int rainChance = 0;
  char sunrise[6] = {0};  // HH:MM
  char sunset[6] = {0};
  char date[11] = {0};  // YYYY-MM-DD
};

struct Weather {
  bool valid = false;
  time_t updated = 0;
  char place[32] = {0};
  float temp = 0;
  float feelsLike = 0;
  int humidity = 0;
  float wind = 0;
  int code = 0;
  WeatherDay days[4];
  uint8_t dayCount = 0;
};

bool loadWeather(Weather& out);
// Index of the forecast day matching todayKey (YYYYMMDD), or -1 when the
// cache does not cover today.
int weatherDayIndex(const Weather& w, int todayKey);
// YYYYMMDD of a "YYYY-MM-DD" date, 0 if malformed.
int isoDateKey(const char* iso);
bool saveWeather(const Weather& w);

// --- markets cache -----------------------------------------------------------
struct Quote {
  char ticker[20] = {0};
  char label[20] = {0};
  float price = 0;
  float prevClose = 0;
  float closes[MAX_CLOSES] = {0};
  uint8_t closeCount = 0;
  bool ok = false;
};

struct Markets {
  bool valid = false;
  time_t updated = 0;
  Quote quotes[MAX_SYMBOLS];
  uint8_t count = 0;
};

bool loadMarkets(Markets& out);
bool saveMarkets(const Markets& m);

// --- focus log ---------------------------------------------------------------
struct FocusDay {
  int dayKey = 0;
  int sessions = 0;
  int minutes = 0;
};

FocusDay loadFocusDay(int todayKey);
void addFocusSession(int todayKey, int minutes);

// --- quote of the day --------------------------------------------------------
// Fills `text`/`author` for the given day; author may come back empty.
void quoteOfDay(int yearDay, std::string& text, std::string& author);

}  // namespace neuron
