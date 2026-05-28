# m5papercolor — Claude Code skill

A [Claude Code](https://claude.com/claude-code) skill for writing Arduino / PlatformIO firmware for the [M5Stack PaperColor](https://docs.m5stack.com/en/core/PaperColor) (SKU C151) — ESP32-S3 with a 4" E Ink Spectra 6 (400×600) display, M5PM1 power management, ES8311 audio codec, SHT40 sensor, RX8130CE RTC, microSD, IR TX, 2× RGB LED, and 3 buttons.

When this skill is loaded, asking Claude things like "write a sketch that shows temperature on the PaperColor", "drive the IR TX on the C151", or "wake the M5PaperColor every minute via RTC alarm" causes it to auto-load the appropriate reference file and reach for the verified code patterns instead of guessing.

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
| 7 | `examples/ir-tv-blaster/` | IR LED on G48 fires (camera-verified). **Real finding:** the M5Stack docs example for IRremote is broken on v4; the corrected pattern with `-DIR_SEND_PIN=48` is documented in [references/input.md](.claude/skills/m5papercolor/references/input.md). |

Not yet verified on hardware: microphone capture (`M5.Mic.record`), Wi-Fi + SNTP RTC sync. The reference docs for those come straight from the M5Stack official examples and look correct, but haven't been driven on a board.

## Sources

All API and pin info distilled from the official M5Stack docs:

- Product page — https://docs.m5stack.com/en/core/PaperColor
- Per-peripheral Arduino guides at `https://docs.m5stack.com/en/arduino/papercolor/*` (program, display, button, battery, rgb_led, ir_nec, mic, speaker, microsd, sht40, rtc, wakeup, m5pm1)
- [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX), [M5PM1](https://github.com/m5stack/M5PM1) — driver libraries
- [M5PaperColor-UserDemo](https://github.com/m5stack/M5PaperColor-UserDemo) — official factory firmware (ESP-IDF), great reference for L3B power sequencing and a real photo-frame app

## License

MIT — see [LICENSE](LICENSE).
