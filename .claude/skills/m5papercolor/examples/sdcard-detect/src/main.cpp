#include <M5Unified.h>
#include <M5PM1.h>
#include <SPI.h>
#include <SD.h>

static constexpr uint8_t SD_CS   = 47;
static constexpr uint8_t SD_SCK  = 15;
static constexpr uint8_t SD_MOSI = 13;
static constexpr uint8_t SD_MISO = 14;

M5Canvas canvas(&M5.Display);
M5PM1 pm1;

static void render(bool pm1_ok, int card_dec, bool sd_ok, uint64_t bytes)
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    char buf[64];

    canvas.fillSprite(WHITE);
    canvas.setTextDatum(top_center);

    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("microSD TEST", w / 2, 22);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextDatum(top_left);
    int y = 100;

    canvas.setTextColor(pm1_ok ? GREEN : RED);
    snprintf(buf, sizeof(buf), "M5PM1 begin: %s", pm1_ok ? "OK" : "FAIL");
    canvas.drawString(buf, 30, y); y += 50;

    canvas.setTextColor(BLACK);
    canvas.drawString("PYG3 SD_PWR_EN: HIGH", 30, y); y += 50;
    canvas.drawString("PYG4 SD_DET_EN: HIGH", 30, y); y += 50;

    snprintf(buf, sizeof(buf), "PYG1 CARD_DEC: %s",
             card_dec < 0 ? "?" : (card_dec == 0 ? "LOW (card!)" : "HIGH (none)"));
    canvas.setTextColor(card_dec == 0 ? GREEN : BLUE);
    canvas.drawString(buf, 30, y); y += 50;

    canvas.setTextColor(sd_ok ? GREEN : RED);
    snprintf(buf, sizeof(buf), "SD.begin: %s", sd_ok ? "OK" : "no card");
    canvas.drawString(buf, 30, y); y += 50;

    if (sd_ok) {
        canvas.setTextColor(BLACK);
        snprintf(buf, sizeof(buf), "size: %.1f MB", (double)bytes / (1024.0*1024.0));
        canvas.drawString(buf, 30, y);
    }

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
    Serial.printf("[sd] pm1.begin -> %d\n", (int)err);

    if (pm1_ok) {
        pm1.setLdoEnable(true);
        pm1.pinMode(M5PM1_GPIO_NUM_0, OUTPUT); pm1.digitalWrite(M5PM1_GPIO_NUM_0, HIGH); // EPD power (shares SPI)
        pm1.pinMode(M5PM1_GPIO_NUM_4, OUTPUT); pm1.digitalWrite(M5PM1_GPIO_NUM_4, HIGH); // SD_DET_EN
        pm1.pinMode(M5PM1_GPIO_NUM_3, OUTPUT); pm1.digitalWrite(M5PM1_GPIO_NUM_3, HIGH); // SD_PWR_EN
        pm1.pinMode(M5PM1_GPIO_NUM_1, INPUT_PULLUP);                                       // CARD_DEC
        delay(50);
    }

    int card_dec = pm1_ok ? pm1.digitalRead(M5PM1_GPIO_NUM_1) : -1;
    Serial.printf("[sd] CARD_DEC = %d (0=card present, 1=no card)\n", card_dec);

    bool sd_ok = false;
    uint64_t cardSize = 0;
    if (card_dec == 0) {
        SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
        sd_ok = SD.begin(SD_CS, SPI, 25000000);
        if (sd_ok) {
            cardSize = SD.cardSize();
            File f = SD.open("/m5pc_test.txt", FILE_WRITE);
            if (f) { f.println("hello from m5papercolor skill verifier"); f.close(); }
            Serial.printf("[sd] mounted, size=%llu bytes\n", (unsigned long long)cardSize);
        } else {
            Serial.println("[sd] SD.begin failed");
        }
    }

    render(pm1_ok, card_dec, sd_ok, cardSize);
}

void loop()
{
    M5.update();
    delay(100);
}
