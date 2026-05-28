#include <M5Unified.h>
#include <M5UnitENV.h>

static constexpr int SHT_SDA = 3;
static constexpr int SHT_SCL = 2;

M5Canvas canvas(&M5.Display);
SHT4X sht4;
bool sht_ok = false;

static void render(float t, float h, bool ok, uint32_t sample_count)
{
    const int w = M5.Display.width();
    const int hgt = M5.Display.height();
    char tBuf[40], hBuf[40], sBuf[40];
    snprintf(tBuf, sizeof(tBuf), "%.1f C", t);
    snprintf(hBuf, sizeof(hBuf), "%.1f %%RH", h);
    snprintf(sBuf, sizeof(sBuf), "samples: %lu", (unsigned long)sample_count);

    canvas.fillSprite(WHITE);
    canvas.setTextDatum(top_center);

    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("SHT40 LIVE", w / 2, 22);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(ok ? GREEN : RED);
    canvas.drawString(ok ? "I2C: ok" : "I2C: NACK", w / 2, 80);

    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(RED);
    canvas.drawString(tBuf, w / 2, hgt / 2 - 50);
    canvas.setTextColor(BLUE);
    canvas.drawString(hBuf, w / 2, hgt / 2 + 50);

    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(BLACK);
    canvas.drawString(sBuf, w / 2, hgt - 22);

    canvas.pushSprite(0, 0);
}

void setup()
{
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);
    Serial.println("\n[sht40] boot");

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setRotation(0);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    sht_ok = sht4.begin(&Wire, SHT40_I2C_ADDR_44, SHT_SDA, SHT_SCL, 400000U);
    Serial.printf("[sht40] sht4.begin -> %d\n", sht_ok ? 1 : 0);

    float t = 0, h = 0;
    if (sht_ok && sht4.update()) {
        t = sht4.cTemp;
        h = sht4.humidity;
        Serial.printf("[sht40] first read: %.2f C / %.2f %%RH\n", t, h);
    }
    render(t, h, sht_ok, 1);
}

void loop()
{
    M5.update();
    static uint32_t last = 0;
    static uint32_t n = 1;
    if (millis() - last >= 30000) {                // every 30 s for visible iteration
        last = millis();
        if (sht_ok && sht4.update()) {
            ++n;
            Serial.printf("[sht40] %.2f C / %.2f %%RH (n=%lu)\n",
                          sht4.cTemp, sht4.humidity, (unsigned long)n);
            M5.Display.setEpdMode(epd_mode_t::epd_fast);   // faster refresh for updates
            render(sht4.cTemp, sht4.humidity, true, n);
        }
    }
    delay(100);
}
