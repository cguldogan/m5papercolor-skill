#include <M5Unified.h>

M5Canvas canvas(&M5.Display);

static const struct { int hz; const char* label; uint16_t color; } TONES[] = {
    { 2000, "2 kHz",  BLUE },
    { 4000, "4 kHz",  GREEN },
    { 8000, "8 kHz",  RED },
    {  500, "500 Hz", BLACK },
};
static int idx = 0;

static void render(const char* label, uint16_t color)
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    canvas.fillSprite(WHITE);

    canvas.setTextDatum(top_center);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("SPEAKER TEST", w / 2, 24);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(GREEN);
    canvas.drawString("ES8311 + AW8737A", w / 2, 78);

    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(color);
    canvas.drawString(label, w / 2, h / 2);

    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(BLACK);
    canvas.drawString("beep every 2 s", w / 2, h - 22);

    canvas.pushSprite(0, 0);
}

void setup()
{
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    M5.Speaker.begin();
    M5.Speaker.setVolume(200);

    render(TONES[0].label, TONES[0].color);
}

void loop()
{
    M5.update();
    static uint32_t last = 0;
    if (millis() - last >= 2000) {
        last = millis();
        M5.Speaker.tone(TONES[idx].hz, 250);          // 250 ms beep
        idx = (idx + 1) % (sizeof(TONES) / sizeof(TONES[0]));
        render(TONES[idx].label, TONES[idx].color);   // next one (the one playing next)
    }
    delay(20);
}
