# Sleep / wake reference

There are **three** levels of "asleep" on this board, with very different power profiles and wake sources. Pick the right one for your duty cycle.

| Approach | API | Power level reached | Idle current | Wake sources | State preserved |
|---|---|---|---|---|---|
| ESP32 light sleep | `esp_light_sleep_start()` | L3A (Wi-Fi off) | ~mA | timer, GPIO, UART | All RAM |
| ESP32 deep sleep | `M5.Power.timerSleep(s)` or `esp_deep_sleep_start()` | L2 | ~100 µA | RTC IO, RTC timer, RX8130 IRQ via G7 | RTC RAM + 32 B PM1 RTC RAM |
| Full M5PM1 shutdown | `pm1.shutdown()` | L0 | ~30 µA | M5PM1 timer, side power button, RTC alarm (via PYG2), ext wake | M5PM1 RTC RAM (NOT M5PM1's 32 B) |

## 1. Deep sleep with RTC timer (simplest)

```cpp
#include <M5Unified.h>

void setup() {
    auto cfg = M5.config(); cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    // ... draw something ...
    canvas.pushSprite(0, 0);

    M5.Power.timerSleep(60);     // RTC alarm wakes in 60 s; returns through setup()
}

void loop() {}                    // never reached
```

`M5.Power.timerSleep(seconds)` programs the RX8130 alarm, then enters ESP32 deep sleep. When the alarm fires, the chip resets — your `setup()` runs again. The EPD content **stays** on the screen (e-paper is bistable).

To wake on Button A instead (button → G10 → RTC GPIO):

```cpp
esp_sleep_enable_ext0_wakeup(GPIO_NUM_10, /*level*/ 0);  // active-low
esp_deep_sleep_start();
```

## 2. Full board shutdown via M5PM1 (lowest power)

Cuts every rail. Comes back through cold boot. Use when you want to sleep for **minutes to days** between wake events.

### Wake-on-timer

```cpp
pm1.timerSet(/*seconds*/ 600, M5PM1_TIM_ACTION_POWERON);
delay(1000);                       // let EPD finish current refresh
pm1.shutdown();
```

### Wake-on-RTC-alarm (so M5.Rtc.setAlarmIRQ time-of-day rules apply)

One-time setup (typically right after `pm1.begin`):

```cpp
pm1.gpioSetFunc      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_WAKE);
pm1.gpioSetPull      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_PULL_UP);
pm1.gpioSetWakeEdge  (M5PM1_GPIO_NUM_2, M5PM1_GPIO_WAKE_FALLING);
pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_2, true);
```

Each time before sleeping:

```cpp
M5.Rtc.disableIRQ();
M5.Rtc.clearIRQ();
M5.Rtc.setAlarmIRQ(60);            // fires in 60 s, RX8130 /IRQ → PYG2 → wake
delay(500);
pm1.shutdown();
```

### Detect why we woke up

```cpp
uint8_t src;
pm1.getWakeSource(&src, M5PM1_CLEAN_ONCE);

if (src & M5PM1_WAKE_SRC_TIM)       Serial.println("woke: M5PM1 timer");
if (src & M5PM1_WAKE_SRC_PWRBTN)    Serial.println("woke: power button");
if (src & M5PM1_WAKE_SRC_EXT_WAKE)  Serial.println("woke: RTC alarm (PYG2)");
if (src & M5PM1_WAKE_SRC_VIN)       Serial.println("woke: USB-C plugged");
```

## 3. ESP32 light sleep (rarely needed)

Use when you need to keep Wi-Fi state and wake on a GPIO inside a few ms:

```cpp
esp_sleep_enable_timer_wakeup(50 * 1000);   // µs — 50 ms
esp_light_sleep_start();
```

For most PaperColor sketches (which redraw every few seconds at most), deep sleep is fine.

## Pattern matrix — pick by duty cycle

| Wake interval | Use |
|---|---|
| < 1 s | Don't sleep — just `delay()` |
| 1 s – 1 min | `M5.Power.timerSleep(s)` (deep sleep) |
| 1 min – 30 min | RTC alarm + `pm1.shutdown()` |
| > 30 min, or until external event | `pm1.timerSet` + button/USB wake + `shutdown()` |

## Preserving state across deep sleep

ESP32 deep sleep preserves `RTC_DATA_ATTR` variables (~8 KB RTC slow RAM):

```cpp
RTC_DATA_ATTR int boot_count = 0;
RTC_DATA_ATTR char last_label[32] = {0};

void setup() {
    boot_count++;
    // ...
}
```

For full M5PM1 shutdown that's lost — only the **M5PM1's 32-byte RTC RAM** survives, and only across ESP32 deep sleep, **not** across `pm1.shutdown()`. If you need to remember anything across `shutdown()`, write to **NVS flash**:

```cpp
#include <Preferences.h>
Preferences nvs;
nvs.begin("papercolor", false);
nvs.putInt("count", boot_count);
nvs.end();
```

## Gotchas

- **Always let the EPD finish.** `pm1.shutdown()` cuts power mid-refresh and leaves visible artifacts on the panel. `delay(500)` to `delay(1000)` after `pushSprite` and before `shutdown`.
- **Battery-critical** check should run *before* the sleep call — see [sensors.md](sensors.md) for the 3100 mV threshold.
- `M5.Power.timerSleep` uses the RX8130 — if you also call `M5.Rtc.setAlarmIRQ` in the same sketch they will fight. Use one or the other.
- After `pm1.shutdown()` and timer wake the board cold-boots — `loop()` from the previous "session" never sees the wake event; check `pm1.getWakeSource()` in `setup()` instead.
- ESP32 GPIO **wake pins must be RTC-capable** (G0..G21 on S3, plus a few others). G47/G48/etc. **cannot** be deep-sleep wake sources directly; route them through M5PM1 PYG wake.
