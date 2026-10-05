#include "NeuronData.h"

#include <HalStorage.h>
#include <Logging.h>
#include <WString.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace neuron {

namespace {

constexpr size_t TEXT_FILE_CAP = 12 * 1024;

// Calls fn(line) for every non-empty, trimmed line of the file. Missing or
// oversized files yield no lines.
template <typename Fn>
bool forEachLine(const char* path, Fn&& fn) {
  std::string content;
  if (!Storage.exists(path)) return false;
  if (!Storage.readFileToString("NN", path, TEXT_FILE_CAP, content)) return false;
  // Files saved by Windows editors may start with a UTF-8 byte-order mark.
  size_t start = content.rfind("\xEF\xBB\xBF", 0) == 0 ? 3 : 0;
  while (start < content.size()) {
    size_t end = content.find('\n', start);
    if (end == std::string::npos) end = content.size();
    size_t a = start;
    size_t b = end;
    while (a < b && (content[a] == ' ' || content[a] == '\t' || content[a] == '\r')) ++a;
    while (b > a && (content[b - 1] == ' ' || content[b - 1] == '\t' || content[b - 1] == '\r')) --b;
    if (b > a) fn(content.substr(a, b - a));
    start = end + 1;
  }
  return true;
}

// Splits "key=value"; returns false for comments and malformed lines.
bool splitKeyValue(const std::string& line, std::string& key, std::string& value) {
  if (line.empty() || line[0] == '#') return false;
  const size_t eq = line.find('=');
  if (eq == std::string::npos || eq == 0) return false;
  key = line.substr(0, eq);
  value = line.substr(eq + 1);
  while (!key.empty() && key.back() == ' ') key.pop_back();
  while (!value.empty() && value.front() == ' ') value.erase(0, 1);
  return true;
}

void copyField(char* dst, size_t cap, const std::string& src) { snprintf(dst, cap, "%s", src.c_str()); }

// Parses a coordinate, keeping `fallback` for empty, malformed or out-of-range input.
float parseCoord(const std::string& value, const float limit, const float fallback) {
  char* end = nullptr;
  const float v = strtof(value.c_str(), &end);
  if (end == value.c_str() || v < -limit || v > limit) return fallback;
  return v;
}

bool writeText(const char* path, const String& text) {
  ensureDirs();
  if (!Storage.writeFile(path, text)) {
    LOG_ERR("NN", "Write failed: %s", path);
    return false;
  }
  return true;
}

void appendf(String& out, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void appendf(String& out, const char* fmt, ...) {
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  out += buf;
}

// Days since 1970-01-01 for a proleptic Gregorian date (Hinnant).
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return static_cast<long>(era) * 146097L + static_cast<long>(doe) - 719468L;
}

long daysFromKey(int key) { return daysFromCivil(key / 10000, (key / 100) % 100, key % 100); }

constexpr const char* DEFAULT_CONFIG =
    "# Nautical Neuron settings. Edit on a computer or via File Transfer.\n"
    "# Weather location (decimal degrees)\n"
    "place=Pune\n"
    "lat=18.5204\n"
    "lon=73.8567\n"
    "# Market indices: Yahoo Finance index ticker:Label, comma separated (max 10).\n"
    "# Only index tickers (starting with ^) are shown.\n"
    "symbols=^NSEI:NIFTY 50,^BSESN:SENSEX,^NSEBANK:BANK NIFTY,^CNXIT:NIFTY IT,^INDIAVIX:INDIA VIX\n";

struct BuiltinQuote {
  const char* text;
  const char* author;
};

constexpr BuiltinQuote QUOTES[] = {
    {"A smooth sea never made a skilled sailor.", "Proverb"},
    {"We cannot direct the wind, but we can adjust the sails.", "Proverb"},
    {"A ship in harbour is safe, but that is not what ships are built for.", "John A. Shedd"},
    {"Slow is smooth, and smooth is fast.", "Naval saying"},
    {"We suffer more often in imagination than in reality.", "Seneca"},
    {"It is not that we have a short time to live, but that we waste a lot of it.", "Seneca"},
    {"Luck is what happens when preparation meets opportunity.", "Seneca"},
    {"Waste no more time arguing what a good man should be. Be one.", "Marcus Aurelius"},
    {"What stands in the way becomes the way.", "Marcus Aurelius"},
    {"No man is free who is not master of himself.", "Epictetus"},
    {"First say to yourself what you would be; then do what you have to do.", "Epictetus"},
    {"Well begun is half done.", "Aristotle"},
    {"Fortune favours the bold.", "Virgil"},
    {"The journey of a thousand miles begins with a single step.", "Lao Tzu"},
    {"Knowing others is intelligence; knowing yourself is true wisdom.", "Lao Tzu"},
    {"You have a right to your actions, never to their fruits.", "Bhagavad Gita"},
    {"Yoga is skill in action.", "Bhagavad Gita"},
    {"Arise, awake, and stop not till the goal is reached.", "Swami Vivekananda"},
    {"He who has a why to live can bear almost any how.", "Nietzsche"},
    {"Measure twice, cut once.", "Proverb"},
    {"Plans are nothing; planning is everything.", "Eisenhower"},
    {"What gets measured gets managed.", "Proverb"},
    {"Be fearful when others are greedy, and greedy when others are fearful.", "Warren Buffett"},
    {"The best time to plant a tree was twenty years ago. The second best time is now.", "Proverb"},
    {"The secret of getting ahead is getting started.", "Mark Twain"},
    {"Simplicity is the ultimate sophistication.", "Leonardo da Vinci"},
    {"In the middle of difficulty lies opportunity.", "Albert Einstein"},
    {"Discipline is the bridge between goals and accomplishment.", "Jim Rohn"},
};
constexpr int QUOTE_COUNT = sizeof(QUOTES) / sizeof(QUOTES[0]);

}  // namespace

void ensureDirs() {
  if (!Storage.exists(DIR)) Storage.mkdir(DIR);
  if (!Storage.exists(CACHE_DIR)) Storage.mkdir(CACHE_DIR);
}

int dayKey(const struct tm& t) { return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday; }

int daysBetween(const int fromKey, const int toKey) {
  if (fromKey <= 0 || toKey <= 0) return 0;
  return static_cast<int>(daysFromKey(toKey) - daysFromKey(fromKey));
}

// --- config ------------------------------------------------------------------

void loadConfig(Config& out) {
  out = Config{};
  snprintf(out.place, sizeof(out.place), "Pune");
  out.lat = 18.5204f;
  out.lon = 73.8567f;

  if (!Storage.exists(CONFIG_PATH)) {
    writeText(CONFIG_PATH, DEFAULT_CONFIG);
  }

  std::string symbols;
  bool haveSymbols = false;
  forEachLine(CONFIG_PATH, [&](const std::string& line) {
    std::string key, value;
    if (!splitKeyValue(line, key, value)) return;
    if (key == "place") {
      copyField(out.place, sizeof(out.place), value);
    } else if (key == "lat") {
      out.lat = parseCoord(value, 90.0f, out.lat);
    } else if (key == "lon") {
      out.lon = parseCoord(value, 180.0f, out.lon);
    } else if (key == "symbols") {
      symbols = value;
      haveSymbols = true;
    }
  });
  if (!haveSymbols) {
    symbols = "^NSEI:NIFTY 50,^BSESN:SENSEX";
  }

  size_t start = 0;
  while (start < symbols.size() && out.symbolCount < MAX_SYMBOLS) {
    size_t end = symbols.find(',', start);
    if (end == std::string::npos) end = symbols.size();
    std::string item = symbols.substr(start, end - start);
    while (!item.empty() && item.front() == ' ') item.erase(0, 1);
    while (!item.empty() && item.back() == ' ') item.pop_back();
    if (!item.empty()) {
      Symbol& s = out.symbols[out.symbolCount++];
      const size_t colon = item.find(':');
      const std::string ticker = colon == std::string::npos ? item : item.substr(0, colon);
      std::string label = colon == std::string::npos ? item : item.substr(colon + 1);
      if (colon == std::string::npos) {
        const size_t dot = label.find('.');
        if (dot != std::string::npos) label.resize(dot);
        if (!label.empty() && label[0] == '^') label.erase(0, 1);
      }
      // '|' separates fields in the markets cache.
      std::replace(label.begin(), label.end(), '|', '/');
      copyField(s.ticker, sizeof(s.ticker), ticker);
      copyField(s.label, sizeof(s.label), label);
      // Indices only: Yahoo index tickers start with '^'.
      if (s.ticker[0] != '^' || strchr(s.ticker, '|')) out.symbolCount--;
    }
    start = end + 1;
  }
}

// --- habits ------------------------------------------------------------------

const char* habitKey(const int id) {
  switch (id) {
    case HABIT_WORKOUT:
      return "workout";
    case HABIT_STEPS:
      return "steps";
    default:
      return "";
  }
}

bool loadHabits(Habits& out, const int todayKey) {
  out = Habits{};
  constexpr uint64_t WINDOW = (HABIT_DAYS >= 64) ? ~0ULL : ((1ULL << HABIT_DAYS) - 1);
  return forEachLine(HABITS_PATH, [&](const std::string& line) {
    if (line[0] == '#') return;
    const size_t p1 = line.find('|');
    const size_t p2 = p1 == std::string::npos ? p1 : line.find('|', p1 + 1);
    if (p2 == std::string::npos) return;
    const std::string key = line.substr(0, p1);
    int id = -1;
    for (int i = 0; i < HABIT_COUNT; ++i) {
      if (key == habitKey(i)) id = i;
    }
    if (id < 0) return;
    const int savedKey = atoi(line.substr(p1 + 1, p2 - p1 - 1).c_str());
    uint64_t bits = 0;
    const std::string history = line.substr(p2 + 1);
    for (size_t i = 0; i < history.size() && i < static_cast<size_t>(HABIT_DAYS); ++i) {
      if (history[i] == '1') bits |= (1ULL << i);
    }
    const int shift = daysBetween(savedKey, todayKey);
    if (shift >= HABIT_DAYS) {
      bits = 0;
    } else if (shift > 0) {
      bits <<= shift;
    }
    out.bits[id] = bits & WINDOW;
  });
}

bool saveHabits(const Habits& habits, const int todayKey) {
  String text;
  text.reserve(HABIT_COUNT * (HABIT_DAYS + 24) + 64);
  text += "# Daily drills: key|YYYYMMDD|history (first digit = that day, 1 = done)\n";
  for (int id = 0; id < HABIT_COUNT; ++id) {
    appendf(text, "%s|%d|", habitKey(id), todayKey);
    for (int i = 0; i < HABIT_DAYS; ++i) text += ((habits.bits[id] >> i) & 1ULL) ? '1' : '0';
    text += '\n';
  }
  return writeText(HABITS_PATH, text);
}

int habitStreak(const uint64_t bits) {
  int i = (bits & 1ULL) ? 0 : 1;
  int streak = 0;
  while (i < HABIT_DAYS && ((bits >> i) & 1ULL)) {
    ++streak;
    ++i;
  }
  return streak;
}

// --- weather -----------------------------------------------------------------

bool loadWeather(Weather& out) {
  out = Weather{};
  forEachLine(WEATHER_PATH, [&](const std::string& line) {
    std::string key, value;
    if (!splitKeyValue(line, key, value)) return;
    if (key == "updated") {
      out.updated = static_cast<time_t>(strtoll(value.c_str(), nullptr, 10));
    } else if (key == "place") {
      copyField(out.place, sizeof(out.place), value);
    } else if (key == "now") {
      // temp,feels,humidity,wind,code
      sscanf(value.c_str(), "%f,%f,%d,%f,%d", &out.temp, &out.feelsLike, &out.humidity, &out.wind, &out.code);
      out.valid = true;
    } else if (key.size() == 2 && key[0] == 'd' && key[1] >= '0' && key[1] <= '3') {
      // date,code,max,min,rain,sunrise,sunset
      WeatherDay d;
      char date[11] = {0}, rise[6] = {0}, set[6] = {0};
      if (sscanf(value.c_str(), "%10[^,],%d,%f,%f,%d,%5[^,],%5s", date, &d.code, &d.tMax, &d.tMin, &d.rainChance, rise,
                 set) >= 5) {
        snprintf(d.date, sizeof(d.date), "%s", date);
        snprintf(d.sunrise, sizeof(d.sunrise), "%s", rise);
        snprintf(d.sunset, sizeof(d.sunset), "%s", set);
        const int idx = key[1] - '0';
        out.days[idx] = d;
        if (idx + 1 > out.dayCount) out.dayCount = static_cast<uint8_t>(idx + 1);
      }
    }
  });
  return out.valid;
}

int isoDateKey(const char* iso) {
  int y = 0, m = 0, d = 0;
  if (!iso || sscanf(iso, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return y * 10000 + m * 100 + d;
}

int weatherDayIndex(const Weather& w, const int todayKey) {
  if (todayKey <= 0) return -1;
  for (int i = 0; i < w.dayCount; ++i) {
    if (isoDateKey(w.days[i].date) == todayKey) return i;
  }
  return -1;
}

bool saveWeather(const Weather& w) {
  String text;
  text.reserve(512);
  appendf(text, "updated=%lld\n", static_cast<long long>(w.updated));
  appendf(text, "place=%s\n", w.place);
  appendf(text, "now=%.1f,%.1f,%d,%.1f,%d\n", w.temp, w.feelsLike, w.humidity, w.wind, w.code);
  for (int i = 0; i < w.dayCount && i < 4; ++i) {
    const WeatherDay& d = w.days[i];
    appendf(text, "d%d=%s,%d,%.1f,%.1f,%d,%s,%s\n", i, d.date, d.code, d.tMax, d.tMin, d.rainChance,
            d.sunrise[0] ? d.sunrise : "--:--", d.sunset[0] ? d.sunset : "--:--");
  }
  return writeText(WEATHER_PATH, text);
}

// --- markets -----------------------------------------------------------------

bool loadMarkets(Markets& out) {
  out = Markets{};
  forEachLine(MARKETS_PATH, [&](const std::string& line) {
    if (line.rfind("updated=", 0) == 0) {
      out.updated = static_cast<time_t>(strtoll(line.c_str() + 8, nullptr, 10));
      return;
    }
    // ticker|label|price|prev|c1,c2,...
    if (out.count >= MAX_SYMBOLS) return;
    const size_t p1 = line.find('|');
    const size_t p2 = p1 == std::string::npos ? p1 : line.find('|', p1 + 1);
    const size_t p3 = p2 == std::string::npos ? p2 : line.find('|', p2 + 1);
    const size_t p4 = p3 == std::string::npos ? p3 : line.find('|', p3 + 1);
    if (p4 == std::string::npos) return;
    Quote& q = out.quotes[out.count];
    q = Quote{};
    copyField(q.ticker, sizeof(q.ticker), line.substr(0, p1));
    copyField(q.label, sizeof(q.label), line.substr(p1 + 1, p2 - p1 - 1));
    q.price = strtof(line.c_str() + p2 + 1, nullptr);
    q.prevClose = strtof(line.c_str() + p3 + 1, nullptr);
    const char* c = line.c_str() + p4 + 1;
    while (*c && q.closeCount < MAX_CLOSES) {
      char* end = nullptr;
      const float v = strtof(c, &end);
      if (end == c) break;
      q.closes[q.closeCount++] = v;
      c = (*end == ',') ? end + 1 : end;
    }
    q.ok = q.price > 0;
    out.count++;
  });
  out.valid = out.count > 0;
  return out.valid;
}

bool saveMarkets(const Markets& m) {
  String text;
  text.reserve(160 + m.count * 260);
  appendf(text, "updated=%lld\n", static_cast<long long>(m.updated));
  for (int i = 0; i < m.count; ++i) {
    const Quote& q = m.quotes[i];
    appendf(text, "%s|%s|%.2f|%.2f|", q.ticker, q.label, q.price, q.prevClose);
    for (int c = 0; c < q.closeCount; ++c) appendf(text, c ? ",%.2f" : "%.2f", q.closes[c]);
    text += '\n';
  }
  return writeText(MARKETS_PATH, text);
}

// --- focus -------------------------------------------------------------------

FocusDay loadFocusDay(const int todayKey) {
  FocusDay day;
  day.dayKey = todayKey;
  forEachLine(FOCUS_PATH, [&](const std::string& line) {
    int key = 0, sessions = 0, minutes = 0;
    if (sscanf(line.c_str(), "%d|%d|%d", &key, &sessions, &minutes) == 3 && key == todayKey) {
      day.sessions = sessions;
      day.minutes = minutes;
    }
  });
  return day;
}

void addFocusSession(const int todayKey, const int minutes) {
  FocusDay day = loadFocusDay(todayKey);
  day.sessions += 1;
  day.minutes += minutes;
  String text;
  appendf(text, "%d|%d|%d\n", todayKey, day.sessions, day.minutes);
  writeText(FOCUS_PATH, text);
}

// --- quotes ------------------------------------------------------------------

void quoteOfDay(const int yearDay, std::string& text, std::string& author) {
  text.clear();
  author.clear();
  int count = 0;
  forEachLine(QUOTES_PATH, [&](const std::string& line) {
    if (line[0] != '#') ++count;
  });
  if (count > 0) {
    const int pick = yearDay % count;
    int i = 0;
    forEachLine(QUOTES_PATH, [&](const std::string& line) {
      if (line[0] == '#') return;
      if (i++ != pick) return;
      // "Quote text -- Author" splits off an author.
      const size_t dash = line.rfind(" -- ");
      if (dash != std::string::npos) {
        text = line.substr(0, dash);
        author = line.substr(dash + 4);
      } else {
        text = line;
      }
    });
    if (!text.empty()) return;
  }
  const BuiltinQuote& q = QUOTES[yearDay % QUOTE_COUNT];
  text = q.text;
  author = q.author;
}

}  // namespace neuron
