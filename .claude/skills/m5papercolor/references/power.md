# M5PM1 power-management reference

The M5PM1 is the custom power IC on the board — I2C 0x6E on `M5.In_I2C`. It controls peripheral power rails, exposes 5 GPIOs (PYG0..PYG4) with ADC/PWM/NeoPixel multiplexing, a 32-bit wake timer, a watchdog, button events, and 32 bytes of sleep-retained RTC RAM.

## Library

```ini
lib_deps = m5stack/M5PM1 @ ^1.0.1
```

```cpp
#include <M5Unified.h>      // must come FIRST so M5GFX claims I2C
#include <M5PM1.h>

M5PM1 pm1;

void setup() {
    auto cfg = M5.config(); cfg.clear_display = false;
    M5.begin(cfg);

    auto err = pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    if (err != M5PM1_OK) { /* I2C broken */ }
    pm1.setLdoEnable(true);     // turn on L3B for peripherals
}
```

**Never pass `&Wire`** to `pm1.begin()` on this board — `M5.In_I2C` already owns G2/G3.

## Power levels — what's actually on

| Level | After this happens | Awake |
|---|---|---|
| L0 | M5PM1 powered off | M5PM1 + RTC only (from battery) |
| L1 | M5PM1 in standby | (same) |
| L2 | ESP32 in deep sleep | + ESP32 RTC peripherals, SHT40, button pull-ups |
| L3A | normal `M5.begin()` | + full ESP32 active |
| L3B | `pm1.setLdoEnable(true)` | + audio (codec/mic/amp), RGB LEDs, Grove out, EPD power, microSD power, RTC IRQ routing |

If you skip the L3B step the screen still draws (the SPI bus is in L3A) but the **power-enable to the panel is off** — most pixels won't latch on the first refresh.

## Rail control

```cpp
pm1.setChargeEnable(true);       // Li-ion charge (on by default)
pm1.setDcdcEnable(true);         // 3.3 V DCDC (3V3_L2)
pm1.setLdoEnable(true);          // 3.3 V LDO (L3B — peripheral rail)
pm1.setBoostEnable(true);        // 5 V boost (Grove + amp)
pm1.setLedEnLevel(true);         // RGB power switch (PY_RGB_PWR_EN)

uint8_t src;
pm1.getPowerSource(&src);        // bitmap: bit0 5VIN, bit1 5VINOUT, bit2 BAT
pm1.getWakeSource(&src, M5PM1_CLEAN_ONCE);  // see m5pm1_wake_src_t enum
```

## Voltage / current readings

```cpp
uint16_t mv;
pm1.readVref(&mv);          // internal reference (~3300 mV)
pm1.readVbat(&mv);          // battery mV
pm1.readVin(&mv);           // USB-C 5VIN mV
pm1.read5VInOut(&mv);       // 5 V rail (boost)

uint16_t adc;
pm1.gpioSetFunc(M5PM1_GPIO_NUM_1, M5PM1_GPIO_FUNC_OTHER);    // ADC mux
pm1.analogRead(M5PM1_ADC_CH_1, &adc);                         // 12-bit
// volts = (adc * vref_mv) / 4096

uint16_t temp_raw;
pm1.readTemperature(&temp_raw);
```

For battery percentage M5Unified already exposes a friendly wrapper:
```cpp
int level = M5.Power.getBatteryLevel();      // 0..100
int batMv = M5.Power.getBatteryVoltage();    // mV
int vbus  = M5.Power.getVBUSVoltage();       // mV, -1 if unknown
bool charging = (vbus > 1000);
```

## Five M5PM1 GPIOs (PYG0..PYG4)

Arduino-style:
```cpp
pm1.pinMode (M5PM1_GPIO_NUM_3, OUTPUT);
pm1.digitalWrite(M5PM1_GPIO_NUM_3, HIGH);
int v = pm1.digitalRead(M5PM1_GPIO_NUM_1);    // 0 / 1
pm1.pinMode(M5PM1_GPIO_NUM_1, INPUT_PULLUP);
```

Multiplexing — set `M5PM1_GPIO_FUNC_OTHER` for the special peripheral on that pin:

| Pin | Special function |
|---|---|
| PYG0 | NeoPixel out (only pin that can drive WS2812) |
| PYG1 | ADC channel 1 |
| PYG2 | ADC channel 2 |
| PYG3 | PWM channel 0 |
| PYG4 | PWM channel 1 |

```cpp
pm1.gpioSetFunc(pyg, M5PM1_GPIO_FUNC_GPIO | _IRQ | _WAKE | _OTHER);
pm1.gpioSetPull(pyg, M5PM1_GPIO_PULL_NONE | _UP | _DOWN);
pm1.gpioSetDrive(pyg, M5PM1_GPIO_DRIVE_PUSHPULL | _OPENDRAIN);   // default is open-drain!
pm1.gpioSetWakeEnable(pyg, true);
pm1.gpioSetWakeEdge  (pyg, M5PM1_GPIO_WAKE_FALLING | _RISING);
```

**The factory pin assignments on PaperColor (don't repurpose unless you know what you're doing):**

| Pin | Used for | Notes |
|---|---|---|
| PYG0 | `PY_EPD_EN` | drive HIGH to enable EPD power |
| PYG1 | `CARD_DEC` | input pull-up; LOW = SD inserted |
| PYG2 | `RTC_IRQ` | wake input from RX8130 alarm |
| PYG3 | `PY_SD_PWR_EN` | drive HIGH to power SD card |
| PYG4 | `PY_SD_DET_EN` | drive HIGH to enable SD detect pull-up |

## PWM (on PYG3/PYG4)

```cpp
pm1.pinMode(M5PM1_GPIO_NUM_3, M5PM1_OTHER);
pm1.setPwmFrequency(1000);                                       // 1 kHz, shared by both channels
pm1.setPwmDuty(M5PM1_PWM_CH_0, /*duty %*/ 75, /*polarity*/false, /*enable*/true);
pm1.analogWrite(M5PM1_PWM_CH_0, 192);                            // 0..255
```

## NeoPixel via M5PM1 (only on PYG0)

This is *not* the same as the WS2812 chain on G21 — the M5PM1 has its own NeoPixel driver for an external chain you wire to PYG0. The two RGB LEDs on the board are on **G21 driven by the ESP32** (see [rgb_led.md](rgb_led.md)).

```cpp
pm1.pinMode(M5PM1_GPIO_NUM_0, M5PM1_OTHER);
pm1.setLedEnLevel(true);
pm1.setLedCount(8);
pm1.setLedColor(0, {0xff, 0x00, 0x00});      // m5pm1_rgb_t
pm1.refreshLeds();                            // blocking until DMA done (~7ms for 32 LEDs)
pm1.disableLeds();
```

After a NeoPixel refresh, the M5PM1's I2C bus is unresponsive for a few ms — don't pipeline I2C calls right behind it.

## Wake timer / shutdown

```cpp
pm1.timerSet(seconds, M5PM1_TIM_ACTION_POWERON);   // power-on after N s
pm1.timerSet(seconds, M5PM1_TIM_ACTION_POWEROFF);  // power-off after N s
pm1.timerSet(seconds, M5PM1_TIM_ACTION_REBOOT);
pm1.timerSet(0,        M5PM1_TIM_ACTION_STOP);     // cancel
pm1.timerClear();

pm1.shutdown();        // → L0
pm1.reboot();
pm1.enterDownloadMode();
```

Pattern — "go to sleep, wake in 10 s":

```cpp
pm1.timerSet(10, M5PM1_TIM_ACTION_POWERON);
delay(1000);                                        // let the EPD finish its current refresh
pm1.shutdown();
```

## RTC alarm wake (from M5PM1 shutdown)

```cpp
// One-time setup:
pm1.gpioSetFunc      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_WAKE);
pm1.gpioSetPull      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_PULL_UP);
pm1.gpioSetWakeEdge  (M5PM1_GPIO_NUM_2, M5PM1_GPIO_WAKE_FALLING);
pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_2, true);

// Each time before shutdown:
M5.Rtc.disableIRQ(); M5.Rtc.clearIRQ();
M5.Rtc.setAlarmIRQ(60);            // wake in 60 s
pm1.shutdown();
```

## Watchdog

```cpp
pm1.wdtSet(30);          // 30 s timeout, 0 = disable
pm1.wdtFeed();           // keep alive
```

If the ESP32 hangs and stops feeding, M5PM1 power-cycles the board.

## RTC backup RAM (32 bytes, sleep-retained)

```cpp
uint8_t scratch[32];
pm1.writeRtcRAM(/*offset*/0, scratch, 32);
pm1.readRtcRAM (0, scratch, 32);
```

Retained across ESP32 deep sleep — **not** across M5PM1 `shutdown()`.

## I2C idle sleep

```cpp
pm1.setI2cConfig(/*sleep_seconds*/0, /*speed*/M5PM1_I2C_FREQ_400K);   // disable auto-sleep
```

If I2C sleep is enabled (default), the **first** I2C transaction after idle is a wake-only NACK; the second succeeds. Most code paths don't care. If yours does, set sleep to 0.

## Side power-button events

```cpp
uint8_t state, flag;
pm1.btnGetState(&state);     // bit0 = pressed-now
pm1.btnGetFlag (&flag);      // bit0 = single, bit1 = wakeup, bit2 = double (auto-clear on read)
pm1.btnSetConfig(...);        // reconfigure click / double / long thresholds (see datasheet)
```

## Complete pattern — RTC-wake every minute, deep sleep otherwise

```cpp
#include <M5Unified.h>
#include <M5PM1.h>

M5PM1 pm1;

void setup() {
    auto cfg = M5.config(); cfg.clear_display = false;
    M5.begin(cfg);
    pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    pm1.setLdoEnable(true);

    // Configure RTC wake routing once:
    pm1.gpioSetFunc      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_WAKE);
    pm1.gpioSetPull      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_PULL_UP);
    pm1.gpioSetWakeEdge  (M5PM1_GPIO_NUM_2, M5PM1_GPIO_WAKE_FALLING);
    pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_2, true);

    // Draw whatever (your dashboard, etc.)
    // ...

    // Sleep until the next minute:
    M5.Rtc.disableIRQ(); M5.Rtc.clearIRQ();
    M5.Rtc.setAlarmIRQ(60);
    delay(500);                  // let EPD finish
    pm1.shutdown();              // → L0; we come back through setup() in 60 s
}

void loop() {}
```

## Gotchas

- **Always init M5PM1 *after* `M5.begin()`** — M5Unified must claim the I2C bus first.
- **Default GPIO drive is open-drain.** If you have no external pull-up, set `M5PM1_GPIO_DRIVE_PUSHPULL`.
- **NeoPixel refresh blocks I2C.** Don't issue PM1 reads/writes in the few ms after `refreshLeds()`.
- **Switching G0 into NeoPixel mode resets the I2C clock to 24 MHz internally** — give it a few ms before the next PM1 transaction.
- **Continuous-read register ranges are limited.** Don't try a single I2C burst across a range boundary (e.g. 0x0C → 0x10 must be two transactions); the M5PM1 driver already handles this, but if you bypass it you'll get garbage.
- `pm1.shutdown()` returns control briefly before power actually drops — don't do anything important after it.
- `pm1.setI2cSleepTime(0)` is your friend if you're seeing intermittent NACKs.
