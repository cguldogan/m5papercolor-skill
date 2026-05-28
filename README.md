# m5papercolor — Claude Code skill

A [Claude Code](https://claude.com/claude-code) skill for writing Arduino / PlatformIO firmware for the [M5Stack PaperColor](https://docs.m5stack.com/en/core/PaperColor) (SKU C151) — ESP32-S3 with a 4" E Ink Spectra 6 (400×600) display, M5PM1 power management, ES8311 audio codec, SHT40 sensor, RX8130CE RTC, microSD, IR TX, 2× RGB LED, and 3 buttons.

When this skill is loaded, asking Claude things like "write a sketch that shows temperature on the PaperColor", "drive the IR TX on the C151", or "wake the M5PaperColor every minute via RTC alarm" causes it to auto-load the appropriate reference file and reach for the verified code patterns instead of guessing.

## What the M5PaperColor can do

A pocket-sized (70.8 × 103.9 × 8.5 mm, 73 g) battery-powered IoT board with a colour e-paper display, voice I/O, environmental sensing, and timed wake-up. Useful for: low-power dashboards, smart room signs, voice memo recorders, e-paper photo frames, environmental loggers, IR remotes, or any project that needs to *show information for hours/days on a single charge*.

**Compute / wireless**

- ESP32-S3R8 — dual-core Xtensa LX7 @ 240 MHz, 16 MB flash, 8 MB Octal PSRAM
- 2.4 GHz Wi-Fi (Wi-Fi 4 / 802.11 b/g/n)
- USB-C with native USB-Serial/JTAG bridge (no extra cable for flashing or debug)

**Display**

- 4" **E Ink Spectra 6** colour panel, 400 × 600, 6 physical colours (black, white, red, yellow, green, blue)
- Bistable: image survives power loss until next refresh
- Selectable refresh modes from `epd_quality` (best colour, ~10 s full refresh) to `epd_fastest` (partial-region, ~1 s washed-out)

**Audio**

- ES8311 audio codec + AW8737A amplifier driving a 1 W / 8 Ω built-in speaker
- MEMS microphone + ES7210 audio ADC with **hardware AEC** (acoustic echo cancellation)
- Drop-in `M5.Speaker.tone(freq, ms)` / `M5.Speaker.playWav(...)` and `M5.Mic.record(...)`
- Mic and speaker share the I2S bus — can't both be active at once

**Sensors**

- **SHT40** temperature + humidity (±0.2 °C, ±1.8 %RH typical) on the system I2C bus
- **RX8130CE** real-time clock with alarm IRQ, ~300 nA backup current — keeps time across full power-down
- Onboard ADC channels on the M5PM1 for battery voltage, VBUS, and internal die temperature

**Input**

- 3 user buttons (BtnA / BtnB / BtnC) on the front face
- Side **power button** with single-click reset, double-click power-off, long-press download mode (configurable through M5PM1)

**Output / signaling**

- **2× WS2812 RGB LEDs** on the side
- **IR transmitter** (G48, ~1–2 m effective range) — drives most NEC / Samsung / Sony / RC5 / RC6 / Panasonic devices via `Arduino-IRremote`

**Storage / expansion**

- **microSD slot** (SPI, up to 25 MHz, FAT32) — photo frames, data logs, OTA payloads
- **HY2.0-4P Grove Port A** (5 V / GND / 2 GPIO) for connecting M5Stack Units — sensors, actuators, displays
- 32-byte sleep-retained RTC scratch RAM in the M5PM1 + 16 MB internal flash for NVS / SPIFFS / FATFS

**Power**

- 1250 mAh LiPo battery, USB-C charging
- **M5PM1** custom power-management IC controls 5 power rails (L0–L3B) and a 32-bit wake timer
- Standby current: ~92 µA in M5PM1 shutdown, microamps in ESP32 deep sleep
- Full-load: ~212 mA
- Three sleep tiers: ESP32 light sleep, ESP32 deep sleep (RTC alarm wake), and full M5PM1 shutdown (button/timer/USB wake)

**Approximate runtime** (back-of-the-envelope): a once-per-minute screen refresh + sensor read pattern can run for **weeks** on a single charge via `pm1.shutdown()` + RTC alarm.

## Install

Drop the skill directory into either your project or your global `~/.claude/skills/`:

```bash
# project-local (recommended — lives with the firmware code that needs it)
git clone https://github.com/cguldogan/m5papercolor-skill.git /tmp/skill
cp -R /tmp/skill/.claude/skills/m5papercolor ./.claude/skills/

# or global (skill loads in every Claude Code session)
mkdir -p ~/.claude/skills
cp -R /tmp/skill/.claude/skills/m5papercolor ~/.claude/skills/
```

Restart Claude Code; the skill is then auto-discoverable.

## What's inside

```
.claude/skills/m5papercolor/
  SKILL.md             # short navigational index + verified PlatformIO config
  template/            # minimal "Hello PaperColor" sketch — verified to build
    platformio.ini
    src/main.cpp
  references/          # one file per peripheral, agent loads on demand
    hardware.md        # full pin map, specs, L0..L3B power architecture
    display.md         # M5.Display / M5Canvas, EPD modes, 6 colors, fonts
    input.md           # buttons + IR NEC TX on G48
    audio.md           # ES8311 speaker + ES7210 mic (mic/speaker exclusion rule)
    power.md           # full M5PM1 API: rails, GPIO, ADC, PWM, NeoPixel, wake
    rgb_led.md         # 2× WS2812 on G21 (needs M5PM1 LDO enabled first)
    sensors.md         # SHT40, RX8130CE RTC, SNTP sync, battery / VBUS
    storage.md         # microSD over SPI with the M5PM1 power-pin dance
    sleep_wake.md      # three sleep tiers + when to use each
```

## Build the included template

```bash
cd .claude/skills/m5papercolor/template
pio run                      # cold build ~2 min, warm ~15 s — produces firmware.bin (~500 KB)
pio run --target upload      # device must be in download mode (hold reset while plugging USB-C)
pio device monitor           # 115200 baud, USB-CDC
```

This config piggybacks on PlatformIO's `esp32s3box` board entry — the M5PaperColor doesn't yet have a native PlatformIO board definition (it does have one in Arduino IDE once you install M5Stack BSP ≥ 3.2.7). The ESP32-S3R8 / 16 MB flash / Octal PSRAM / USB-CDC combo is identical, so the build works.

## Status

Verified end-to-end on a real M5PaperColor (chip rev v0.2). Seven sketches built, flashed, and observed:

| # | Example | What it proves |
|---|---|---|
| 1 | `template/` hello-world | PIO config → flash → boot → M5Unified → EPD → sprite push |
| 2 | `examples/sht40-live/` | SHT40 over system I2C via `m5stack/M5Unit-ENV` |
| 3 | `examples/rgb-button/` | M5PM1 L3B power dance + 2× WS2812 + buttons |
| 4 | `examples/speaker-tone/` | `M5.Speaker.tone()` end-to-end (ES8311 + AW8737A) |
| 5 | `examples/sdcard-detect/` | M5PM1 PYG3/PYG4/PYG1 SD power dance + CARD_DEC |
| 6 | `examples/rtc-wake-loop/` | RX8130 alarm + `pm1.shutdown()` + PYG2 wake, NVS persistence |
| 7 | `examples/ir-tv-blaster/` | IR LED on G48 fires in **both** modes: raw `digitalWrite` 38 kHz and IRremote v4 via the canonical M5Stack `begin(DISABLE_LED_FEEDBACK) / setSendPin(48)` pattern (both camera-confirmed). |

Not yet verified on hardware: microphone capture (`M5.Mic.record`), Wi-Fi + SNTP RTC sync. The reference docs for those come straight from the M5Stack official examples and look correct, but haven't been driven on a board.

## Sources

All API and pin info distilled from the official M5Stack docs:

- Product page — https://docs.m5stack.com/en/core/PaperColor
- Per-peripheral Arduino guides at `https://docs.m5stack.com/en/arduino/papercolor/*` (program, display, button, battery, rgb_led, ir_nec, mic, speaker, microsd, sht40, rtc, wakeup, m5pm1)
- [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX), [M5PM1](https://github.com/m5stack/M5PM1) — driver libraries
- [M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo) — official factory firmware (ESP-IDF), great reference for L3B power sequencing and a real photo-frame app

## License

MIT — see [LICENSE](LICENSE).
