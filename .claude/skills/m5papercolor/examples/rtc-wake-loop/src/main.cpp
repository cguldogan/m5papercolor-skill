// RTC wake test: persists a boot counter in NVS, sleeps via pm1.shutdown(),
// wakes 20 s later via RTC alarm routed through M5PM1 PYG2, then redraws.
#include <M5Unified.h>
#include <M5PM1.h>
#include <Preferences.h>

static constexpr int WAKE_AFTER_S = 20;

M5Canvas canvas(&M5.Display);
M5PM1 pm1;
Preferences nvs;

static void render(uint32_t boots, uint8_t wake_src, bool pm1_ok)
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    char buf[64];

    canvas.fillSprite(WHITE);
    canvas.setTextDatum(top_center);

    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("RTC WAKE TEST", w / 2, 22);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(pm1_ok ? GREEN : RED);
    canvas.drawString(pm1_ok ? "M5PM1 ok" : "M5PM1 fail", w / 2, 80);

    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextColor(BLUE);
    snprintf(buf, sizeof(buf), "boot #%lu", (unsigned long)boots);
    canvas.drawString(buf, w / 2, h / 2 - 50);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(RED);
    const char* src = "unknown";
    if (wake_src & 0x01) src = "TIMER";
    else if (wake_src & 0x04) src = "PWRBTN";
    else if (wake_src & 0x20) src = "EXT_WAKE (RTC)";
    else if (wake_src & 0x02) src = "VIN/USB";
    snprintf(buf, sizeof(buf), "wake: %s", src);
    canvas.drawString(buf, w / 2, h / 2 + 30);

    canvas.setTextDatum(bottom_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(BLACK);
    snprintf(buf, sizeof(buf), "sleeping 20 s in 5 s...");
    canvas.drawString(buf, w / 2, h - 22);

    canvas.pushSprite(0, 0);
}

void setup()
{
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    m5pm1_err_t err = pm1.begin(&M5.In_I2C, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    bool pm1_ok = (err == M5PM1_OK);
    if (pm1_ok) pm1.setLdoEnable(true);

    // Read why we woke
    uint8_t wake_src = 0;
    if (pm1_ok) pm1.getWakeSource(&wake_src, M5PM1_CLEAN_ONCE);
    Serial.printf("[rtc] wake_src bitmap = 0x%02X\n", wake_src);

    // Increment persisted boot counter
    nvs.begin("m5pc-rtc", false);
    uint32_t boots = nvs.getUInt("boots", 0) + 1;
    nvs.putUInt("boots", boots);
    nvs.end();
    Serial.printf("[rtc] boot #%lu\n", (unsigned long)boots);

    render(boots, wake_src, pm1_ok);

    if (!pm1_ok) {
        Serial.println("[rtc] PM1 unavailable, halting");
        while (1) delay(1000);
    }

    // Configure PYG2 as wake input on falling edge (from RX8130 /IRQ)
    pm1.gpioSetFunc      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_WAKE);
    pm1.gpioSetPull      (M5PM1_GPIO_NUM_2, M5PM1_GPIO_PULL_UP);
    pm1.gpioSetWakeEdge  (M5PM1_GPIO_NUM_2, M5PM1_GPIO_WAKE_FALLING);
    pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_2, true);

    delay(5000);                                  // let user see this boot's screen

    // Arm RTC alarm and shut down
    if (M5.Rtc.isEnabled()) {
        M5.Rtc.disableIRQ();
        M5.Rtc.clearIRQ();
        M5.Rtc.setAlarmIRQ(WAKE_AFTER_S);
        Serial.printf("[rtc] alarm armed for %d s, shutting down...\n", WAKE_AFTER_S);
    } else {
        // Fallback: use M5PM1 timer if RTC isn't reachable
        pm1.timerSet(WAKE_AFTER_S, M5PM1_TIM_ACTION_POWERON);
        Serial.println("[rtc] RTC missing; using PM1 timer fallback");
    }

    delay(200);
    pm1.shutdown();
    // ... never reached normally ...
}

void loop()
{
    delay(1000);
}
