# microSD reference

The PaperColor exposes a microSD slot on a **dedicated SPI bus** that the EPD also shares for SCK/MOSI. The full 4-pin power dance below is mandatory — `SD.begin()` on its own is not enough.

## Pin map

| ESP32-S3 | Function | Shared with |
|---|---|---|
| G47 | SD CS | (SD only) |
| G15 | SPI_CLK | EPD |
| G13 | SPI_MOSI | EPD |
| G14 | SPI_MISO | (SD only) |

Plus four M5PM1 PYGx pins:
- **PYG3** → `PY_SD_PWR_EN` — **OUTPUT HIGH** to turn on SD power
- **PYG4** → `PY_SD_DET_EN` — **OUTPUT HIGH** to enable the detect pull-up
- **PYG1** → `CARD_DEC` — **INPUT_PULLUP**; reads **LOW when a card is inserted**
- **PYG0** → `PY_EPD_EN` — also drive HIGH (the EPD shares the SPI bus, leave its power on)

## Mounting recipe — copy/paste

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <M5Unified.h>
#include <M5PM1.h>

static constexpr uint8_t SD_CS   = 47;
static constexpr uint8_t SD_SCK  = 15;
static constexpr uint8_t SD_MOSI = 13;
static constexpr uint8_t SD_MISO = 14;

M5PM1 pm1;

bool mountSD() {
    if (pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK)
        return false;

    pm1.setLdoEnable(true);                              // L3B on

    pm1.pinMode(M5PM1_GPIO_NUM_0, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_0, HIGH);            // PY_EPD_EN (shares SPI)
    pm1.pinMode(M5PM1_GPIO_NUM_4, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_4, HIGH);            // PY_SD_DET_EN
    pm1.pinMode(M5PM1_GPIO_NUM_3, OUTPUT);
    pm1.digitalWrite(M5PM1_GPIO_NUM_3, HIGH);            // PY_SD_PWR_EN
    pm1.pinMode(M5PM1_GPIO_NUM_1, INPUT_PULLUP);         // CARD_DEC

    if (pm1.digitalRead(M5PM1_GPIO_NUM_1) != LOW) {
        Serial.println("SD: no card inserted");
        return false;
    }

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 25000000)) {                // 25 MHz works reliably
        Serial.println("SD: init failed");
        return false;
    }
    return true;
}
```

## Using the card

```cpp
File f = SD.open("/log.txt", FILE_APPEND);
if (f) { f.println("hello"); f.close(); }

File r = SD.open("/picture.png", FILE_READ);
// read / seek etc.

// Or directly via M5GFX:
M5.Display.drawPngFile(SD, "/picture.png");
canvas.drawJpgFile(SD, "/photo.jpg", x, y, 0, 0, 0, 0, scale, scale);
canvas.drawBmpFile(SD, "/banner.bmp", x, y);
```

## Card-detect interrupt (optional)

To get notified when a card is inserted/removed, set PYG1 as a wake/IRQ source:

```cpp
pm1.gpioSetFunc      (M5PM1_GPIO_NUM_1, M5PM1_GPIO_FUNC_IRQ);
pm1.gpioSetWakeEdge  (M5PM1_GPIO_NUM_1, M5PM1_GPIO_WAKE_FALLING);  // insert → low
pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_1, true);
// then poll pm1.irqGetGpioStatus() or wire it to the ESP32 IRQ pin
```

## Speed

`SD.begin(SD_CS, SPI, 25000000)` runs at 25 MHz over a shared SPI bus. Cards rated UHS-I can push 40 MHz, but the EPD's other peripherals on the same bus get unhappy. Stick to 25 MHz unless you have a reason.

## Gotchas

- **Don't call `SPI.begin()` before bringing up M5PM1.** Without `PY_SD_PWR_EN` the card draws no current and `SD.begin()` will hang or return false.
- **The CARD_DEC pin is shared with M5PM1** — `digitalRead(GPIO_NUM_1)` on the ESP32 won't work. You **must** go through `pm1.digitalRead(M5PM1_GPIO_NUM_1)`.
- **Shared SPI bus**: M5GFX is already driving the EPD on this bus. The Arduino-IDE-style `SD.begin()` ends up sharing — that works but be aware: a long PNG decode blocks the EPD's next refresh too.
- The factory firmware mounts a **FAT volume on the on-board flash** at `/data` for storage when no card is inserted — see `M5PaperColor-UserDemo` for that pattern.
- **microSD requires `setLdoEnable(true)`** on M5PM1, even if you've enabled the per-pin SD power.
