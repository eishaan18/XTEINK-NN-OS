#include "WeatherActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "MappedInputManager.h"
#include "activities/neuron/NeuronSyncLauncher.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "neuron/NeuronClock.h"
#include "neuron/NeuronDraw.h"

namespace {

// "2026-10-06" -> 0..6 (Sunday = 0), -1 if malformed.
int weekdayOf(const char* iso) {
  int y = 0, m = 0, d = 0;
  if (sscanf(iso, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12) return -1;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const long days = static_cast<long>(era) * 146097L + static_cast<long>(doe) - 719468L;
  return static_cast<int>(((days % 7) + 11) % 7);  // 1970-01-01 was a Thursday
}

const char* shortDay(const int wday) {
  static constexpr StrId DAYS[] = {StrId::STR_NN_SUN, StrId::STR_NN_MON, StrId::STR_NN_TUE, StrId::STR_NN_WED,
                                   StrId::STR_NN_THU, StrId::STR_NN_FRI, StrId::STR_NN_SAT};
  if (wday < 0 || wday > 6) return "";
  return I18n::getInstance().get(DAYS[wday]);
}

}  // namespace

void WeatherActivity::onEnter() {
  Activity::onEnter();
  neuron::loadConfig(config);
  neuron::loadWeather(weather);
  requestUpdate();
}

void WeatherActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    launchNeuronSync(*this, renderer, mappedInput);
  }
}

void WeatherActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  constexpr int SIDE = 20;

  renderer.clearScreen();
  char title[64];
  snprintf(title, sizeof(title), "%s  |  %s", tr(STR_NN_WEATHER), weather.place[0] ? weather.place : config.place);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title);

  int y = metrics.topPadding + metrics.headerHeight + 24;
  if (!weather.valid) {
    renderer.drawCenteredText(UI_12_FONT_ID, height / 3, tr(STR_NN_NO_WEATHER), true, EpdFontFamily::BOLD);
    const auto lines = renderer.wrappedText(UI_10_FONT_ID, tr(STR_NN_NO_DATA_SYNC), width - 2 * SIDE, 3);
    int ly = height / 3 + renderer.getLineHeight(UI_12_FONT_ID) + 10;
    for (const auto& l : lines) {
      renderer.drawCenteredText(UI_10_FONT_ID, ly, l.c_str());
      ly += renderer.getLineHeight(UI_10_FONT_ID);
    }
  } else {
    // Now: icon on the left, big temperature on the right.
    const int icon = 130;
    neuron::drawWeatherIcon(renderer, SIDE, y, icon, weather.code);
    char temp[8];
    snprintf(temp, sizeof(temp), "%ld", lroundf(weather.temp));
    const int digitH = 100;
    const int tx = SIDE + icon + 30;
    const int tw = neuron::drawBigText(renderer, tx, y + 10, digitH, temp);
    renderer.drawText(NOTOSANS_18_FONT_ID, tx + tw + 8, y + 6,
                      "\xC2\xB0"
                      "C",
                      true, EpdFontFamily::BOLD);
    y += icon + 16;

    renderer.drawText(NOTOSANS_18_FONT_ID, SIDE, y, neuron::weatherLabel(weather.code), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(NOTOSANS_18_FONT_ID) + 8;

    char line[96];
    snprintf(line, sizeof(line), "%s %ld\xC2\xB0   %s %d%%   %s %ld km/h", tr(STR_NN_FEELS_LIKE),
             lroundf(weather.feelsLike), tr(STR_NN_HUMIDITY), weather.humidity, tr(STR_NN_WIND), lroundf(weather.wind));
    renderer.drawText(UI_10_FONT_ID, SIDE, y, line);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
    // Line up the forecast with the calendar: an older cache may no longer
    // start today.
    struct tm now;
    const int todayKey = neuron::localNow(now) ? neuron::dayKey(now) : 0;
    const int todayIdx = neuron::weatherDayIndex(weather, todayKey);
    const int base = todayIdx >= 0 ? todayIdx : 0;
    if (weather.dayCount > 0) {
      const auto& d0 = weather.days[base];
      const char* dayName = todayIdx >= 0 ? tr(STR_NN_TODAY) : shortDay(weekdayOf(d0.date));
      snprintf(line, sizeof(line), "%s %ld\xC2\xB0 / %ld\xC2\xB0   %s %d%%", dayName, lroundf(d0.tMax),
               lroundf(d0.tMin), tr(STR_NN_RAIN), d0.rainChance);
      renderer.drawText(UI_10_FONT_ID, SIDE, y, line);
      y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
      snprintf(line, sizeof(line), "%s %s   %s %s", tr(STR_NN_SUNRISE), d0.sunrise, tr(STR_NN_SUNSET), d0.sunset);
      renderer.drawText(UI_10_FONT_ID, SIDE, y, line);
      y += renderer.getLineHeight(UI_10_FONT_ID) + 20;
    }

    // The days after that.
    const int days = std::max(0, static_cast<int>(weather.dayCount) - 1 - base);
    if (days > 0) {
      renderer.fillRect(SIDE, y, width - 2 * SIDE, 2, true);
      y += 14;
      const int colW = (width - 2 * SIDE) / days;
      const int lh = renderer.getLineHeight(UI_10_FONT_ID);
      for (int i = 0; i < days; ++i) {
        const auto& d = weather.days[base + 1 + i];
        const int cx = SIDE + i * colW;
        const bool tomorrow = todayKey > 0 && neuron::daysBetween(todayKey, neuron::isoDateKey(d.date)) == 1;
        const char* name = tomorrow ? tr(STR_NN_TOMORROW) : shortDay(weekdayOf(d.date));
        const int nw = renderer.getTextWidth(UI_10_FONT_ID, name, EpdFontFamily::BOLD);
        renderer.drawText(UI_10_FONT_ID, cx + (colW - nw) / 2, y, name, true, EpdFontFamily::BOLD);
        const int ic = 64;
        neuron::drawWeatherIcon(renderer, cx + (colW - ic) / 2, y + lh + 6, ic, d.code);
        char hl[24];
        snprintf(hl, sizeof(hl), "%ld\xC2\xB0 / %ld\xC2\xB0", lroundf(d.tMax), lroundf(d.tMin));
        const int hw = renderer.getTextWidth(UI_10_FONT_ID, hl);
        renderer.drawText(UI_10_FONT_ID, cx + (colW - hw) / 2, y + lh + ic + 12, hl);
        char rain[16];
        snprintf(rain, sizeof(rain), "%d%%", d.rainChance);
        const int rw = renderer.getTextWidth(SMALL_FONT_ID, rain);
        renderer.drawText(SMALL_FONT_ID, cx + (colW - rw) / 2, y + 2 * lh + ic + 14, rain);
      }
    }

    char ago[24];
    neuron::formatAgo(static_cast<long long>(weather.updated), ago, sizeof(ago));
    char footer[64];
    snprintf(footer, sizeof(footer), "%s %s  |  Open-Meteo", tr(STR_NN_UPDATED), ago);
    renderer.drawCenteredText(SMALL_FONT_ID, height - metrics.buttonHintsHeight - 30, footer);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_NN_SYNC), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
