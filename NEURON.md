# Nautical Neuron firmware for the Xteink X4

A personal firmware for the Xteink X4 (and X3), forked from the open-source
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
(MIT). Everything CrossPoint does for books stays intact; on top of it sits
the **Bridge**, a daily dashboard, plus a set of small productivity apps.

## What's new

| Screen | What it does |
| --- | --- |
| **Boot / sleep** | Nautical Neuron crest instead of the CrossPoint logo. |
| **Bridge** (start screen) | Big local clock with **GMT** underneath, the date, weather, NIFTY 50 and SENSEX, today's two drills, a quote of the day, and an app launcher. |
| **Habits** | Two daily drills: **Workout** and **10,000 steps**. Confirm ticks today; each shows a 7-day strip and streak. |
| **Focus Watch** | Pomodoro timer (default 25 min, adjustable 5-90). Short break after each watch, long break after four. Logs watches per day. |
| **Weather** | Current conditions + 3-day forecast for Pune (configurable), from Open-Meteo. |
| **Indices** | NIFTY 50, SENSEX, BANK NIFTY, NIFTY IT and INDIA VIX with one-month sparklines (Yahoo Finance, delayed). Indices only, no stocks. |
| **Sync** | Restarts briefly for a clean memory, then one WiFi session sets the clock (NTP) and refreshes weather + indices, and returns to the Bridge. |
| **Books** | The original CrossPoint library home (also reachable with Back on the Bridge). |

The library home also has a **Bridge** entry at the bottom of its menu.

### Navigating the Bridge

- **Left / Right** (front buttons): move across the app tiles
- **Up / Down** (side buttons): move between tile rows
- **Confirm**: open the selected app
- **Back**: go to the book library

## Your files on the SD card

Everything lives in plain text under `/neuron`, so you can edit it from a
computer or through **File Transfer** in a browser:

```
/neuron/config.txt     place, lat, lon, market indices
/neuron/habits.txt     workout / steps history (managed by the device)
/neuron/quotes.txt     optional: your own quotes, "Quote -- Author" per line
/neuron/cache/         weather, indices, focus log
```

`config.txt` is created on first use:

```
place=Pune
lat=18.5204
lon=73.8567
symbols=^NSEI:NIFTY 50,^BSESN:SENSEX,^NSEBANK:BANK NIFTY,^CNXIT:NIFTY IT,^INDIAVIX:INDIA VIX
```

Symbols are Yahoo Finance index tickers (they start with `^`), optionally
followed by `:Label`, up to 10. Anything that is not an index is ignored.

## The clock on the X4

The X4 has no battery-backed clock chip (the X3 does). Nautical Neuron runs
the clock on the ESP32 itself: it keeps time through sleep and restarts, but
only after it has been set over WiFi, and it drifts a little between syncs.

- Set your timezone once in **Settings > System > Clock > Time Zone** (choose `India / Colombo`).
  This menu now appears on the X4 too, along with the option to show the
  time in the header bar.
- The clock is set by **Sync**, by **Settings > System > Clock > Sync Clock Now**, and
  automatically whenever the device joins WiFi while the time is unknown.
- After a full power-off the time is unknown again; the Bridge says
  "Clock not set" until the next sync, rather than showing a wrong time.
- When it has been more than a day and a half since the last sync, the Bridge
  shows "~ clock synced Nd ago" under the date.
- GMT is always shown in 24-hour format; local time follows
  **Clock Format** in the same menu.

## Building

The firmware builds with PlatformIO exactly like CrossPoint:

```sh
git clone --recursive https://github.com/eishaan18/XTEINK-NN-OS
cd XTEINK-NN-OS
pio run -e gh_release          # firmware at .pio/build/gh_release/firmware.bin
```

Every push to `main` also builds the firmware on GitHub Actions
(`.github/workflows/neuron-build.yml`): open the run and download the
`nautical-neuron-x3-x4-firmware` artifact.

## Flashing

1. **Back up first.** Connect the X4 by USB-C, open the web flasher at
   <https://crosspointreader.com/#flash-tools> in Chrome/Edge and use
   **Save full flash**. Keep the `.bin` safe; it restores the device exactly.
2. In the same tool, choose the option to flash a firmware file and pick `firmware.bin`.
3. Press Reset, then hold Power for 3 seconds.

## Over-the-air updates

The device's **Settings > System > Check for Updates** looks at this repository's
latest GitHub release. To ship an update: bump `version` in `platformio.ini`
(e.g. `1.0.1`), push, then publish a GitHub release tagged `1.0.1`. The
`release.yml` workflow builds and attaches `crosspoint-1.0.1-x3-x4.bin`,
which the updater downloads.

## Previewing and testing without the device

`tools/neuron_preview` compiles the Neuron code on a computer:

```sh
tools/neuron_preview/build.sh   # screen mock-ups with sample data -> out/*.png (g++, Python + Pillow)
tools/neuron_preview/test.sh    # checks for file parsing, formatting and the weather/index
                                # response handling (needs ArduinoJson from a prior `pio run`)
```

## Changing the logo

Replace `src/images/neuron_logo.png` and run `python scripts/neuron_logo.py`
to regenerate `src/images/NeuronLogo.h`.

## Credits

Built on CrossPoint Reader and the FreeInk SDK by the CrossPoint
contributors, under the MIT licence (see `LICENSE`). Weather data by
[Open-Meteo](https://open-meteo.com). Market data from Yahoo Finance's
public chart endpoint: unofficial, delayed, for personal use only.
