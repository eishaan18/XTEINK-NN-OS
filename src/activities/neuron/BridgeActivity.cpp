#include "BridgeActivity.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/neuron/FocusActivity.h"
#include "activities/neuron/HabitsActivity.h"
#include "activities/neuron/MarketsActivity.h"
#include "activities/neuron/NeuronSyncLauncher.h"
#include "activities/neuron/WeatherActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "neuron/NeuronClock.h"
#include "neuron/NeuronDraw.h"

namespace {

constexpr int APP_COUNT = static_cast<int>(BridgeActivity::App::Count);
constexpr int GRID_COLS = 4;
constexpr int SIDE = 16;
// A periodic half refresh clears the ghosting minute-by-minute fast refreshes leave.
constexpr int RENDERS_PER_CLEAN = 15;
// Clock considered stale (shown with "~") this long after the last NTP sync on RTC-less boards.
constexpr long STALE_CLOCK_SECONDS = 36L * 3600L;
// Weather older than this shows its age on the card.
constexpr long long STALE_WEATHER_SECONDS = 3LL * 3600LL;

const char* appLabel(const BridgeActivity::App app) {
  switch (app) {
    case BridgeActivity::App::Focus:
      return tr(STR_NN_FOCUS);
    case BridgeActivity::App::Habits:
      return tr(STR_NN_HABITS);
    case BridgeActivity::App::Weather:
      return tr(STR_NN_WEATHER);
    case BridgeActivity::App::Markets:
      return tr(STR_NN_MARKETS);
    case BridgeActivity::App::Sync:
      return tr(STR_NN_SYNC);
    case BridgeActivity::App::Books:
      return tr(STR_NN_BOOKS);
    case BridgeActivity::App::Settings:
      return tr(STR_SETTINGS_TITLE);
    default:
      return "";
  }
}

const char* weekdayName(const int wday) {
  static constexpr StrId DAYS[] = {StrId::STR_NN_SUNDAY,    StrId::STR_NN_MONDAY,   StrId::STR_NN_TUESDAY,
                                   StrId::STR_NN_WEDNESDAY, StrId::STR_NN_THURSDAY, StrId::STR_NN_FRIDAY,
                                   StrId::STR_NN_SATURDAY};
  return I18n::getInstance().get(DAYS[(wday % 7 + 7) % 7]);
}

const char* monthName(const int mon) {
  static constexpr StrId MONTHS[] = {StrId::STR_NN_JANUARY, StrId::STR_NN_FEBRUARY, StrId::STR_NN_MARCH,
                                     StrId::STR_NN_APRIL,   StrId::STR_NN_MAY,      StrId::STR_NN_JUNE,
                                     StrId::STR_NN_JULY,    StrId::STR_NN_AUGUST,   StrId::STR_NN_SEPTEMBER,
                                     StrId::STR_NN_OCTOBER, StrId::STR_NN_NOVEMBER, StrId::STR_NN_DECEMBER};
  return I18n::getInstance().get(MONTHS[(mon % 12 + 12) % 12]);
}

}  // namespace

void BridgeActivity::onEnter() {
  Activity::onEnter();
  neuron::ensureDirs();
  if (!markets) markets = makeUniqueNoThrow<neuron::Markets>();
  reloadData();
  requestUpdate();
}

void BridgeActivity::reloadData() {
  struct tm now;
  habitsDay = neuron::localNow(now) ? neuron::dayKey(now) : 0;
  neuron::loadHabits(habits, habitsDay);
  neuron::loadWeather(weather);
  if (markets) neuron::loadMarkets(*markets);
}

void BridgeActivity::open(const App app) {
  std::unique_ptr<Activity> next;
  switch (app) {
    case App::Focus:
      next = makeUniqueNoThrow<FocusActivity>(renderer, mappedInput);
      break;
    case App::Habits:
      next = makeUniqueNoThrow<HabitsActivity>(renderer, mappedInput);
      break;
    case App::Weather:
      next = makeUniqueNoThrow<WeatherActivity>(renderer, mappedInput);
      break;
    case App::Markets:
      next = makeUniqueNoThrow<MarketsActivity>(renderer, mappedInput);
      break;
    case App::Sync:
      launchNeuronSync(*this, renderer, mappedInput);
      return;
    case App::Books:
      activityManager.goHome();
      return;
    case App::Settings:
      activityManager.goToSettings();
      return;
    default:
      return;
  }
  if (!next) {
    LOG_ERR("BRDG", "OOM: opening app %d", static_cast<int>(app));
    return;
  }
  startActivityForResult(std::move(next), [this](const ActivityResult&) {
    reloadData();
    lastMinute = -1;
  });
}

void BridgeActivity::loop() {
  struct tm now;
  if (neuron::localNow(now) && now.tm_min != lastMinute) {
    lastMinute = now.tm_min;
    // Past midnight the drills roll over to a fresh day.
    if (neuron::dayKey(now) != habitsDay) reloadData();
    requestUpdate();
  }

  using B = MappedInputManager::Button;
  if (mappedInput.wasReleased(B::Back)) {
    activityManager.goHome();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    open(static_cast<App>(selected));
    return;
  }
  int next = selected;
  if (mappedInput.wasPressed(B::Right)) next = (selected + 1) % APP_COUNT;
  if (mappedInput.wasPressed(B::Left)) next = (selected + APP_COUNT - 1) % APP_COUNT;
  if (mappedInput.wasPressed(B::Down)) next = (selected + GRID_COLS) % APP_COUNT;
  if (mappedInput.wasPressed(B::Up)) next = (selected + APP_COUNT - GRID_COLS) % APP_COUNT;
  if (next != selected) {
    selected = next;
    requestUpdate();
  }
}

int BridgeActivity::drawClock(int y) {
  const int width = renderer.getScreenWidth();
  struct tm now;
  const bool haveTime = neuron::localNow(now);
  const int digitH = renderer.getScreenHeight() / 7;

  char hhmm[8];
  if (haveTime) {
    int hour = now.tm_hour;
    if (SETTINGS.clockFormat == 1) {
      hour %= 12;
      if (hour == 0) hour = 12;
    }
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", hour, now.tm_min);
  } else {
    snprintf(hhmm, sizeof(hhmm), "--:--");
  }
  neuron::drawBigTextCentered(renderer, y, digitH, hhmm);
  if (haveTime && SETTINGS.clockFormat == 1) {
    const int clockW = neuron::bigTextWidth(hhmm, digitH);
    renderer.drawText(UI_10_FONT_ID, (width + clockW) / 2 + 6, y + digitH - renderer.getLineHeight(UI_10_FONT_ID),
                      now.tm_hour >= 12 ? "PM" : "AM", true, EpdFontFamily::BOLD);
  }
  y += digitH + 14;

  if (haveTime) {
    char line[64];
    snprintf(line, sizeof(line), "%s, %d %s %d", weekdayName(now.tm_wday), now.tm_mday, monthName(now.tm_mon),
             now.tm_year + 1900);
    renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 2;
    struct tm gmt;
    if (neuron::utcNow(gmt)) {
      snprintf(line, sizeof(line), "%s %02d:%02d", tr(STR_NN_GMT), gmt.tm_hour, gmt.tm_min);
      renderer.drawCenteredText(UI_12_FONT_ID, y, line);
      y += renderer.getLineHeight(UI_12_FONT_ID) + 2;
    }
    const long since = neuron::secondsSinceSync();
    if (since > STALE_CLOCK_SECONDS) {
      char ago[24];
      neuron::formatAgo(static_cast<long long>(time(nullptr)) - since, ago, sizeof(ago));
      snprintf(line, sizeof(line), "%s %s", tr(STR_NN_CLOCK_SYNCED), ago);
      renderer.drawCenteredText(SMALL_FONT_ID, y, line);
      y += renderer.getLineHeight(SMALL_FONT_ID);
    }
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_NN_CLOCK_NOT_SET), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID) + 2;
    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_NN_CLOCK_NOT_SET_HINT));
    y += renderer.getLineHeight(SMALL_FONT_ID);
  }
  return y + 10;
}

int BridgeActivity::drawCards(int y) {
  const int width = renderer.getScreenWidth();
  const int cardW = (width - SIDE * 2 - 12) / 2;
  const int cardH = 118;
  const int pad = 10;
  const int lh10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);

  // Weather card
  {
    const int x = SIDE;
    renderer.drawRoundedRect(x, y, cardW, cardH, 2, 10, true);
    if (weather.valid) {
      const int icon = 52;
      neuron::drawWeatherIcon(renderer, x + pad, y + pad, icon, weather.code);
      char temp[16];
      snprintf(temp, sizeof(temp), "%ld\xC2\xB0", lroundf(weather.temp));
      renderer.drawText(NOTOSANS_18_FONT_ID, x + pad + icon + 8, y + pad + 4, temp, true, EpdFontFamily::BOLD);
      // Older snapshots carry their age so a stale reading is not mistaken for now.
      char condition[64];
      const long long age = static_cast<long long>(time(nullptr)) - static_cast<long long>(weather.updated);
      if (age > STALE_WEATHER_SECONDS) {
        char ago[24];
        neuron::formatAgo(static_cast<long long>(weather.updated), ago, sizeof(ago));
        snprintf(condition, sizeof(condition), "%s, %s", neuron::weatherLabel(weather.code), ago);
      } else {
        snprintf(condition, sizeof(condition), "%s", neuron::weatherLabel(weather.code));
      }
      const std::string label = renderer.truncatedText(SMALL_FONT_ID, condition, cardW - 2 * pad);
      renderer.drawText(SMALL_FONT_ID, x + pad, y + pad + icon + 4, label.c_str());
      struct tm now;
      const int today = neuron::weatherDayIndex(weather, neuron::localNow(now) ? neuron::dayKey(now) : 0);
      if (today >= 0) {
        const auto& d = weather.days[today];
        char line[48];
        snprintf(line, sizeof(line), "H %ld\xC2\xB0  L %ld\xC2\xB0  %d%%", lroundf(d.tMax), lroundf(d.tMin),
                 d.rainChance);
        renderer.drawText(SMALL_FONT_ID, x + pad, y + pad + icon + 4 + lhSmall, line);
      }
    } else {
      renderer.drawText(UI_10_FONT_ID, x + pad, y + pad, tr(STR_NN_WEATHER), true, EpdFontFamily::BOLD);
      const auto lines = renderer.wrappedText(SMALL_FONT_ID, tr(STR_NN_NO_DATA_SYNC), cardW - 2 * pad, 3);
      int ly = y + pad + lh10 + 4;
      for (const auto& l : lines) {
        renderer.drawText(SMALL_FONT_ID, x + pad, ly, l.c_str());
        ly += lhSmall;
      }
    }
  }

  // Markets card: the first two symbols (normally NIFTY 50 and SENSEX).
  {
    const int x = SIDE + cardW + 12;
    renderer.drawRoundedRect(x, y, cardW, cardH, 2, 10, true);
    if (markets && markets->valid) {
      int ly = y + pad;
      const int rows = std::min<int>(2, markets->count);
      for (int i = 0; i < rows; ++i) {
        const auto& q = markets->quotes[i];
        const std::string name = renderer.truncatedText(SMALL_FONT_ID, q.label, cardW - 2 * pad);
        renderer.drawText(SMALL_FONT_ID, x + pad, ly, name.c_str(), true, EpdFontFamily::BOLD);
        ly += lhSmall;
        char price[24];
        neuron::formatPrice(q.price, price, sizeof(price));
        renderer.drawText(UI_10_FONT_ID, x + pad, ly, price, true, EpdFontFamily::BOLD);
        if (q.prevClose > 0) {
          const float pct = (q.price - q.prevClose) / q.prevClose * 100.0f;
          char chg[16];
          snprintf(chg, sizeof(chg), "%.2f%%", pct < 0 ? -pct : pct);
          const int chgW = renderer.getTextWidth(SMALL_FONT_ID, chg);
          const int cx = x + cardW - pad - chgW;
          renderer.drawText(SMALL_FONT_ID, cx, ly + 2, chg);
          neuron::drawTrend(renderer, cx - 12, ly + 5, 9, pct >= 0);
        }
        ly += lh10 + 4;
      }
    } else {
      renderer.drawText(UI_10_FONT_ID, x + pad, y + pad, tr(STR_NN_MARKETS), true, EpdFontFamily::BOLD);
      const auto lines = renderer.wrappedText(SMALL_FONT_ID, tr(STR_NN_NO_DATA_SYNC), cardW - 2 * pad, 3);
      int ly = y + pad + lh10 + 4;
      for (const auto& l : lines) {
        renderer.drawText(SMALL_FONT_ID, x + pad, ly, l.c_str());
        ly += lhSmall;
      }
    }
  }
  return y + cardH + 14;
}

int BridgeActivity::drawDrills(int y) {
  const int width = renderer.getScreenWidth();
  const int lh = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);

  renderer.drawText(UI_10_FONT_ID, SIDE, y, tr(STR_NN_TODAYS_DRILLS), true, EpdFontFamily::BOLD);
  y += lh + 2;
  renderer.fillRect(SIDE, y, width - 2 * SIDE, 2, true);
  y += 10;

  // Two side-by-side items: tick box, name, streak underneath.
  const int colW = (width - 2 * SIDE) / neuron::HABIT_COUNT;
  const int box = lh;
  for (int id = 0; id < neuron::HABIT_COUNT; ++id) {
    const int x = SIDE + id * colW;
    const uint64_t bits = habits.bits[id];
    const bool done = habitsDay > 0 && (bits & 1ULL);
    if (done) {
      renderer.fillRect(x, y + 1, box, box, true);
      // White tick inside the filled box.
      renderer.drawLine(x + box / 5, y + 1 + box / 2, x + box * 2 / 5, y + 1 + box * 3 / 4, 3, false);
      renderer.drawLine(x + box * 2 / 5, y + 1 + box * 3 / 4, x + box * 4 / 5, y + 1 + box / 4, 3, false);
    } else {
      renderer.drawRect(x, y + 1, box, box, 2, true);
    }
    const char* name = id == neuron::HABIT_WORKOUT ? tr(STR_NN_HABIT_WORKOUT) : tr(STR_NN_HABIT_STEPS);
    const std::string label = renderer.truncatedText(UI_10_FONT_ID, name, colW - box - 16, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, x + box + 8, y, label.c_str(), true, EpdFontFamily::BOLD);
    char streak[32];
    snprintf(streak, sizeof(streak), "%d %s", habitsDay > 0 ? neuron::habitStreak(bits) : 0, tr(STR_NN_DAY_STREAK));
    renderer.drawText(SMALL_FONT_ID, x + box + 8, y + lh, streak);
  }
  return y + lh + lhSmall + 12;
}

void BridgeActivity::drawQuote(const int y, const int maxY) {
  const int width = renderer.getScreenWidth();
  const int lh = renderer.getLineHeight(NOTOSERIF_12_FONT_ID);
  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);
  const int available = maxY - y - lhSmall;
  if (available < lh) return;

  struct tm now;
  const int day = neuron::localNow(now) ? now.tm_yday : 0;
  if (day != quoteDay) {
    neuron::quoteOfDay(day, quote, quoteAuthor);
    quoteDay = day;
  }
  const int maxLines = std::max(1, std::min(3, available / lh));
  const auto lines =
      renderer.wrappedText(NOTOSERIF_12_FONT_ID, quote.c_str(), width - 2 * SIDE - 16, maxLines, EpdFontFamily::ITALIC);
  const int blockH = static_cast<int>(lines.size()) * lh + (quoteAuthor.empty() ? 0 : lhSmall);
  int ly = y + std::max(0, (maxY - y - blockH) / 2);
  renderer.fillRect(SIDE, ly, 3, blockH, true);
  for (const auto& l : lines) {
    renderer.drawText(NOTOSERIF_12_FONT_ID, SIDE + 14, ly, l.c_str(), true, EpdFontFamily::ITALIC);
    ly += lh;
  }
  if (!quoteAuthor.empty()) {
    char by[64];
    snprintf(by, sizeof(by), "\xE2\x80\x94 %s", quoteAuthor.c_str());
    const int w = renderer.getTextWidth(SMALL_FONT_ID, by);
    renderer.drawText(SMALL_FONT_ID, width - SIDE - w, ly, by);
  }
}

int BridgeActivity::launcherHeight() const {
  const int rows = (APP_COUNT + GRID_COLS - 1) / GRID_COLS;
  return rows * 52 + (rows - 1) * 8;
}

void BridgeActivity::drawLauncher(int y) {
  const int width = renderer.getScreenWidth();
  const int gapX = 8;
  const int tileW = (width - 2 * SIDE - (GRID_COLS - 1) * gapX) / GRID_COLS;
  const int tileH = 52;
  for (int i = 0; i < APP_COUNT; ++i) {
    const int col = i % GRID_COLS;
    const int row = i / GRID_COLS;
    const int x = SIDE + col * (tileW + gapX);
    const int ty = y + row * (tileH + 8);
    const bool sel = i == selected;
    if (sel) {
      renderer.fillRoundedRect(x, ty, tileW, tileH, 8, Color::Black);
    } else {
      renderer.drawRoundedRect(x, ty, tileW, tileH, 2, 8, true);
    }
    const std::string label =
        renderer.truncatedText(UI_10_FONT_ID, appLabel(static_cast<App>(i)), tileW - 8, EpdFontFamily::BOLD);
    const int tw = renderer.getTextWidth(UI_10_FONT_ID, label.c_str(), EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, x + (tileW - tw) / 2, ty + (tileH - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                      label.c_str(), !sel, EpdFontFamily::BOLD);
  }
}

void BridgeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  renderer.clearScreen();
  // Short band (like the library home): title and battery on one line, which
  // leaves the clock and the day's briefing most of the screen.
  constexpr int HEADER_H = 44;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, HEADER_H}, tr(STR_NN_TAGLINE), nullptr, false);

  int y = metrics.topPadding + HEADER_H + 14;
  y = drawClock(y);
  y = drawCards(y);

  const int launcherY = height - metrics.buttonHintsHeight - 10 - launcherHeight();
  y = drawDrills(y);
  drawQuote(y, launcherY - 10);
  drawLauncher(launcherY);

  const auto labels = mappedInput.mapLabels(tr(STR_NN_BOOKS), tr(STR_NN_OPEN), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  const bool clean = (renderCount++ % RENDERS_PER_CLEAN) == 0;
  renderer.displayBuffer(clean ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
}
