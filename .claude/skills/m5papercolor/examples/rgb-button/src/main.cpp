#include <M5Unified.h>
#include <M5PM1.h>
#include <Adafruit_NeoPixel.h>

static constexpr uint8_t LED_PIN   = 21;
static constexpr uint8_t LED_COUNT = 2;

M5Canvas canvas(&M5.Display);
M5PM1 pm1;
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static const char* COLOR_NAMES[] = {"RED", "GREEN", "BLUE", "YELLOW", "WHITE", "OFF"};
static const uint8_t COLOR_RGB[][3] = {
    {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {255, 255, 255}, {0, 0, 0}
};
static int color_idx = 0;
static bool pm1_ready = false;

static void setBothLeds(int idx)
{
    uint32_t c = pixels.Color(COLOR_RGB[idx][0], COLOR_RGB[idx][1], COLOR_RGB[idx][2]);
    pixels.setPixelColor(0, c);
    pixels.setPixelColor(1, c);
    pixels.show();
}

static void render(const char* state)
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    canvas.fillSprite(WHITE);

    canvas.setTextDatum(top_center);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("RGB + BTN A", w / 2, 24);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(pm1_ready ? GREEN : RED);
    canvas.drawString(pm1_ready ? "M5PM1: ok" : "M5PM1: fail", w / 2, 78);

    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(BLUE);
    canvas.drawString(state, w / 2, h / 2);

    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(BLACK);
    canvas.drawString("press BtnA to cycle", w / 2, h - 22);

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
    pm1_ready = (err == M5PM1_OK);
    Serial.printf("[rgb] pm1.begin -> %d\n", (int)err);

    if (pm1_ready) {
        pm1.setLdoEnable(true);                  // L3B on
    }

    pixels.begin();
    pixels.setBrightness(80);
    setBothLeds(color_idx);                       // RED

    render(COLOR_NAMES[color_idx]);
}

void loop()
{
    M5.update();
    if (M5.BtnA.wasPressed()) {
        color_idx = (color_idx + 1) % (sizeof(COLOR_NAMES)/sizeof(COLOR_NAMES[0]));
        setBothLeds(color_idx);
        Serial.printf("[rgb] BtnA -> %s\n", COLOR_NAMES[color_idx]);
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        render(COLOR_NAMES[color_idx]);
    }
    delay(20);
}
