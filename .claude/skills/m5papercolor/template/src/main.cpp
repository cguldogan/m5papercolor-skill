// Hello PaperColor — deep-sleep + button-wake demo.
//
// Boots, draws the screen, then enters ESP32 deep sleep (~100 µA).
// The e-paper image persists with zero power until the next refresh.
// Press any user button (A / B / C) to wake — the boot counter
// increments, the screen redraws, and the chip goes back to sleep.

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_sleep.h>

// Persisted in RTC slow memory across deep sleep (NOT across full power-off).
RTC_DATA_ATTR static uint32_t boot_count = 0;

// User-button RTC IOs on the M5PaperColor.
static constexpr gpio_num_t BTN_A_PIN = GPIO_NUM_10;  // BtnA / USER_KEY3
static constexpr gpio_num_t BTN_B_PIN = GPIO_NUM_9;   // BtnB / USER_KEY2
static constexpr gpio_num_t BTN_C_PIN = GPIO_NUM_1;   // BtnC / USER_KEY1
static constexpr uint64_t BTN_MASK = (1ULL << BTN_A_PIN) |
                                     (1ULL << BTN_B_PIN) |
                                     (1ULL << BTN_C_PIN);

static M5Canvas canvas(&M5.Display);

static const char* wakeReasonName(esp_sleep_wakeup_cause_t r)
{
    switch (r) {
        case ESP_SLEEP_WAKEUP_EXT0:     return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1:     return "button";
        case ESP_SLEEP_WAKEUP_TIMER:    return "timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "touch";
        case ESP_SLEEP_WAKEUP_ULP:      return "ulp";
        case ESP_SLEEP_WAKEUP_GPIO:     return "gpio";
        case ESP_SLEEP_WAKEUP_UART:     return "uart";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:                        return "power-on";
    }
}

static void drawScreen(const char* reason)
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    char buf[48];

    canvas.fillSprite(WHITE);

    canvas.setTextDatum(top_center);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("Hello PaperColor", w / 2, 40);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(BLUE);
    canvas.drawString("sleeps when idle", w / 2, 100);

    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextColor(RED);
    snprintf(buf, sizeof(buf), "wakes: %lu", (unsigned long)boot_count);
    canvas.drawString(buf, w / 2, h / 2 - 30);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(GREEN);
    snprintf(buf, sizeof(buf), "last wake: %s", reason);
    canvas.drawString(buf, w / 2, h / 2 + 40);

    canvas.setTextDatum(bottom_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("press A / B / C to wake", w / 2, h - 22);

    canvas.pushSprite(0, 0);
}

void setup()
{
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);

    const auto wake = esp_sleep_get_wakeup_cause();
    ++boot_count;
    Serial.printf("\n[hello] boot #%lu, wake=%s\n",
                  (unsigned long)boot_count, wakeReasonName(wake));

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    drawScreen(wakeReasonName(wake));

    // Let the EPD finish its refresh before we cut clocks.
    delay(200);

    // Make sure all three buttons read HIGH at idle (so a single press = falling edge to LOW).
    gpio_pullup_en(BTN_A_PIN);
    gpio_pullup_en(BTN_B_PIN);
    gpio_pullup_en(BTN_C_PIN);
    gpio_pulldown_dis(BTN_A_PIN);
    gpio_pulldown_dis(BTN_B_PIN);
    gpio_pulldown_dis(BTN_C_PIN);

    // Configure ext1 wake — any of the three buttons going LOW wakes us.
    esp_sleep_enable_ext1_wakeup(BTN_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

    Serial.println("[hello] entering deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
    // ... unreachable ...
}

void loop() {}
