# RGB LED reference

Two **WS2812** addressable LEDs are chained on **GPIO 21**, driven by the ESP32 (not by the M5PM1 NeoPixel driver — that's a separate channel on PYG0 for *external* chains).

Color order is `NEO_GRB`, signal rate `800 kHz`.

## Dependency

```ini
lib_deps =
    m5stack/M5Unified @ ^0.2.15
    m5stack/M5GFX @ ^0.2.21
    m5stack/M5PM1 @ ^1.0.1
    adafruit/Adafruit NeoPixel @ ^1.15.4
```

## Power-on dance

The RGB LED rail is in **L3B** — gated by the M5PM1 LDO. The LEDs are physically present but won't light until:

```cpp
M5PM1 pm1;
pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
pm1.setLdoEnable(true);
```

## Basic usage

```cpp
#include <M5Unified.h>
#include <M5PM1.h>
#include <Adafruit_NeoPixel.h>

static constexpr uint8_t LED_PIN   = 21;
static constexpr uint8_t LED_COUNT = 2;

M5PM1 pm1;
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    auto cfg = M5.config(); cfg.clear_display = false;
    M5.begin(cfg);

    pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    pm1.setLdoEnable(true);

    pixels.begin();
    pixels.setBrightness(80);          // 0..255 — 80 is comfortable, 255 burns retinas
    pixels.clear();
    pixels.show();
}

void loop() {
    pixels.setPixelColor(0, pixels.Color(255, 0,   0));    // index 0 red
    pixels.setPixelColor(1, pixels.Color(0,   0, 255));    // index 1 blue
    pixels.show();                                         // commit to LEDs
    delay(500);
    pixels.setPixelColor(0, pixels.Color(0,   0, 255));
    pixels.setPixelColor(1, pixels.Color(255, 0,   0));
    pixels.show();
    delay(500);
}
```

## Status-indicator pattern (factory demo style)

```cpp
static void setAllLeds(uint32_t color) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) pixels.setPixelColor(i, color);
    pixels.show();
}

setAllLeds(pixels.Color(0, 255, 0));   // green = ready
setAllLeds(pixels.Color(255, 0, 0));   // red = error
setAllLeds(pixels.Color(255, 255, 0)); // yellow = working
setAllLeds(0);                          // off
```

## Gotchas

- **`pixels.show()` is required** to commit; setting colors alone does nothing.
- **`setBrightness()` is applied at `show()` time** as a per-channel scale — calling it after `setPixelColor` works fine.
- **Without `setLdoEnable(true)`** the LEDs look broken (no light at all).
- The LED data pin sits on a regular ESP32 GPIO, so timing-sensitive RMT is used internally by Adafruit_NeoPixel — Wi-Fi can occasionally glitch a single frame. Re-`show()` is harmless.
- These are **WS2812**, not WS2812B-V5 — color order is `NEO_GRB`, never `NEO_RGB`.
