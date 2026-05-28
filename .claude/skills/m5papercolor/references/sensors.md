# Sensor reference — SHT40, RTC, battery

All three sit on the **system I2C** bus (`M5.In_I2C` / `Wire` with SDA=G3 SCL=G2). SHT40 0x44, RX8130CE RTC 0x32, M5PM1 0x6E.

## SHT40 — temperature + humidity

Needs `m5stack/M5Unit-ENV` (>= 1.4.0) — note the **dash** in the package name. `m5stack/M5UnitENV` (no dash, the form used in the official Arduino IDE docs) is *not* in the PlatformIO registry and will fail with `UnknownPackageError`. M5Unit-ENV transitively pulls in `m5stack/M5UnitUnified`.

```ini
lib_deps =
    m5stack/M5Unified @ ^0.2.15
    m5stack/M5GFX @ ^0.2.21
    m5stack/M5Unit-ENV @ ^1.4.0
```

```cpp
#include <M5Unified.h>
#include <M5UnitENV.h>

SHT4X sht4;

void setup() {
    auto cfg = M5.config(); cfg.clear_display = false;
    M5.begin(cfg);

    // SDA = GPIO 3, SCL = GPIO 2, 400 kHz, addr 0x44
    if (!sht4.begin(&Wire, SHT40_I2C_ADDR_44, /*sda*/3, /*scl*/2, /*freq*/400000U)) {
        Serial.println("SHT40 not found");
    }
}

void loop() {
    if (sht4.update()) {
        float c = sht4.cTemp;       // Celsius
        float h = sht4.humidity;    // %RH
        Serial.printf("%.1f C / %.1f %%\n", c, h);
    }
    delay(120000);                  // SHT40 self-heats — don't poll faster than every 2 min for accurate readings
}
```

The sensor is on rail L3A (always on after `M5.begin()`) — you do **not** need to bring up L3B.

## RX8130CE RTC

`M5.Rtc` exposes it through `M5Unified`:

```cpp
if (!M5.Rtc.isEnabled()) { /* RTC missing */ }

M5.Rtc.setDateTime({{2026, 5, 28}, {12, 0, 0}});         // {year,month,date},{h,m,s}
m5::rtc_datetime_t dt = M5.Rtc.getDateTime();
// dt.date.year / .month / .date / .weekDay (0=Sun..6=Sat)
// dt.time.hours / .minutes / .seconds

M5.Rtc.setAlarmIRQ(/*seconds_from_now*/ 60);             // wake-in-60s alarm
M5.Rtc.clearIRQ();
M5.Rtc.disableIRQ();
```

The alarm pin (`/IRQ` on RX8130, pin 7 on ESP32 = G7) is **also routed to M5PM1 PYG2**, which is how RTC alarms can wake the board out of `pm1.shutdown()` — see [power.md](power.md).

The RTC also has its own coin-cell-style backup; it survives main-battery removal. After M5PM1 power-off it keeps ticking.

### SNTP time sync over Wi-Fi

```cpp
#include <WiFi.h>
#include <esp_sntp.h>
#include <sntp.h>

#define TZ      "UTC-8"
#define NTP1    "0.pool.ntp.org"

WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) delay(250);

configTzTime(TZ, NTP1, "1.pool.ntp.org", "2.pool.ntp.org");
while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) delay(500);

time_t now = time(nullptr) + 1;
while (now > time(nullptr)) delay(10);
struct tm* lt = localtime(&now);
M5.Rtc.setDateTime(lt);          // pass struct tm*, not the {{}, {}} literal
```

### Datasheet note

The Epson `RX8130CE_cn.pdf` linked from the product page is a **1-page product brief** only — register map not included. For deep access (custom calibration, FOUT control, backup-battery charge mode) pull the full RX8130CE application manual from Seiko Epson. The fields `M5.Rtc` exposes cover the alarm/IRQ/time-of-day workflow, which is enough for 95% of sketches.

## Battery / VBUS

The PaperColor's gas gauge is M5PM1, surfaced through `Power_Class`:

```cpp
int level   = M5.Power.getBatteryLevel();     // 0..100
int batMv   = M5.Power.getBatteryVoltage();   // mV
int vbusMv  = M5.Power.getVBUSVoltage();      // mV, -1 if unknown
bool plugged = (vbusMv > 1000);
```

For more detail (5VINOUT rail, charge enable bit, low-voltage shutdown threshold) reach into M5PM1 directly:

```cpp
uint16_t mv;
pm1.readVbat(&mv);
pm1.readVin (&mv);
pm1.read5VInOut(&mv);
pm1.setChargeEnable(true);
pm1.setBatteryLvp(/*reg_value*/ 0x40);     // threshold = 2000 + value*7.81 mV  (≈3000 mV at 0x80)
```

Factory firmware uses **3100 mV** as the "battery critical, shutdown now" line. Mirror that if you want similar safety:

```cpp
uint16_t mv = 0;
if (pm1.readVbat(&mv) == M5PM1_OK && mv < 3100) {
    pm1.shutdown();
}
```

## Display + sensor dashboard pattern

Read every 2 minutes, redraw with the values, sleep in between.

```cpp
auto cfg = M5.config(); cfg.clear_display = false;
M5.begin(cfg);
M5.Display.setEpdMode(epd_mode_t::epd_quality);
M5.Display.setRotation(1);

sht4.begin(&Wire, SHT40_I2C_ADDR_44, 3, 2, 400000U);
pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
pm1.setLdoEnable(true);

uint32_t last = 0;
void loop() {
    M5.update();
    if (millis() - last >= 120000) {
        last = millis();
        if (sht4.update()) draw(sht4.cTemp, sht4.humidity);
    }
    delay(100);
}
```
