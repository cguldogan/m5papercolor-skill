---
name: m5papercolor
description: Write Arduino/PlatformIO firmware for the M5Stack PaperColor (SKU C151) — ESP32-S3 with 4" E Ink Spectra 6 (400x600) display, M5PM1 power management, ES8311 audio codec, SHT40, RX8130CE RTC, IR TX, microSD, 2x RGB LED, 3 buttons. Use when the user says PaperColor, M5PaperColor, C151, or asks to drive its display / audio / mic / buttons / RTC / SHT40 / IR / microSD / RGB LED / sleep / battery.
---

# M5PaperColor firmware development

The M5PaperColor (SKU **C151**) is an ESP32-S3R8 board (16MB flash, 8MB PSRAM, 2.4 GHz Wi-Fi) with a **4" E Ink Spectra 6 panel at 400 x 600** (panel `ED2208-DOA` / `EL040EF1`, 6 colors only: black, white, red, yellow, green, blue), a 1250 mAh battery, and the **M5PM1** power-management IC that controls peripheral power rails and provides 5 GPIOs, ADC, PWM, NeoPixel output, RTC backup RAM, and timer wake.

> **Important:** the device runs in *one* of five power levels (L0..L3B). The e-paper, RGB LEDs, audio, Grove port, and microSD all live on rail **L3B** and require `pm1.setLdoEnable(true)` (plus per-rail enables) before they work. The factory `M5.begin(cfg)` does **not** turn these on for you. See [references/power.md](references/power.md).

## Verified build

This config builds cleanly with PlatformIO 6.1.19 (espressif32 6.12.0, M5Unified 0.2.16, M5GFX 0.2.22, M5PM1 1.0.6). It's in [template/](template/) — copy that directory and run `pio run`.

```ini
; platformio.ini
[env:m5stack-papercolor]
platform = espressif32 @ 6.12.0
board = esp32s3box
framework = arduino
board_build.partitions = default_16MB.csv
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    m5stack/M5Unified @ ^0.2.15
    m5stack/M5GFX @ ^0.2.21
    m5stack/M5PM1 @ ^1.0.1
```

There's no native M5PaperColor board definition in PlatformIO yet, so we piggyback on `esp32s3box` — the ESP32-S3R8 / 16MB flash / Octal PSRAM / USB-CDC-on-boot combo is identical. Add any extra `lib_deps` your sketch needs (`adafruit/Adafruit NeoPixel`, `arduino-libraries/SD`, `m5stack/M5UnitENV`, `Arduino-IRremote/IRremote`).

```bash
cd template
pio run                     # build
pio run --target upload     # flash over USB-C (hold reset to enter download mode)
pio device monitor           # 115200 baud, USB-CDC
```

For the **Arduino IDE** path: install M5Stack board manager **>= 3.2.7** and select board **"M5PaperColor"** (this gives you a real board definition); then install M5Unified >= 0.2.15, M5GFX >= 0.2.21, M5PM1 >= 1.0.1.

## Skeleton every sketch needs

```cpp
#include <M5Unified.h>
#include <M5PM1.h>          // only if you touch any L3B peripheral

M5Canvas canvas(&M5.Display);
M5PM1 pm1;

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;                 // EPD: don't blank on boot
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setEpdMode(epd_mode_t::epd_quality);   // see references/display.md
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    // Bring up L3B if you use audio/SD/RGB/EPD-controlled rails:
    if (pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) == M5PM1_OK) {
        pm1.setLdoEnable(true);
    }
}

void loop() {
    M5.update();             // required — refreshes buttons, etc.
    delay(100);
}
```

`M5.In_I2C` (SDA=G3, SCL=G2) is shared by M5PM1 (0x6E), RX8130CE RTC (0x32), and SHT40 (0x44). **Always** initialize M5PM1 via `&M5.In_I2C`, never with raw `&Wire` — M5GFX owns that bus.

## Reference index — load by topic

- [references/hardware.md](references/hardware.md) — full pin map, specs, power-rail architecture, partition layout
- [references/display.md](references/display.md) — `M5.Display` / `M5Canvas`, EPD refresh modes, color constants, fonts, full demo
- [references/input.md](references/input.md) — buttons (`M5.BtnA/B/C`), IR NEC TX on G48
- [references/audio.md](references/audio.md) — `M5.Speaker` + `M5.Mic` (ES8311 codec, ES7210 ADC, AW8737A amp); mic/speaker mutual exclusion rule
- [references/sensors.md](references/sensors.md) — SHT40 temp/humidity (M5UnitENV), RX8130CE RTC (incl. SNTP sync), battery / VBUS voltage
- [references/power.md](references/power.md) — M5PM1 full API: power rails (L0..L3B), GPIO, ADC, PWM, NeoPixel, timer wake, RTC-alarm wake, button events, watchdog
- [references/rgb_led.md](references/rgb_led.md) — 2x WS2812 on G21 driven by `Adafruit_NeoPixel` (needs L3B enabled)
- [references/storage.md](references/storage.md) — microSD over SPI (CS=G47, SCK=G15, MOSI=G13, MISO=G14) with M5PM1 power gating
- [references/sleep_wake.md](references/sleep_wake.md) — ESP32 deep sleep, M5PM1 shutdown, RTC alarm wake, GPIO wake

## Gotchas (the ones that bit me)

- **`clear_display = false` is mandatory** for E-Ink. The default `M5.begin` blanks the display, which on this panel costs a full slow refresh.
- **Never use `&Wire` for M5PM1.** Use `&M5.In_I2C` — M5GFX has already claimed the system I2C pins.
- **L3B peripherals are off at boot.** Speaker, mic, RGB LEDs, microSD power, e-paper power-enable all need `pm1.setLdoEnable(true)` and often a per-rail `pm1.digitalWrite(...)` (see [references/storage.md](references/storage.md) for the SD card's 4-pin power dance).
- **Mic and Speaker can't both be `begin()`-ed.** Call `M5.Mic.end()` before `M5.Speaker.begin()` and vice versa — they share the I2S bus and AVDD.
- **EPD has only 6 colors.** Anything else gets dithered. There's no anti-aliasing — text edges show banding; use `epd_quality` mode for text.
- **`pio` uses board `esp32s3box`**, not a native `m5stack-papercolor` — that board doesn't exist in espressif32 6.12.0 yet. Arduino IDE *does* have a native M5PaperColor entry once you install M5Stack BSP >= 3.2.7.
- **Download mode:** hold the side reset button while connecting USB-C. Power off = press the power button **twice**, not once.

## Run the verified hello-world

```bash
cd .claude/skills/m5papercolor/template
pio run                          # ~2 min cold, builds firmware.bin (~500 KB)
pio run --target upload          # device must be in download mode
pio device monitor               # 115200 baud
```

The `template/` sketch draws "Hello PaperColor" centered on the display. This is the same sketch I used to verify the PlatformIO config in this skill.
