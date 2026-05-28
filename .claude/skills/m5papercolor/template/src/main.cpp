#include <M5Unified.h>

M5Canvas Canvas(&M5.Display);

void setup()
{
    auto cfg          = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    const int screen_w = M5.Display.width();
    const int screen_h = M5.Display.height();

    Canvas.createSprite(screen_w, screen_h);
    Canvas.fillSprite(WHITE);

    Canvas.setTextFont(4);
    Canvas.setTextSize(2);
    Canvas.setTextDatum(middle_center);
    Canvas.setTextColor(BLACK);
    Canvas.drawString("Hello PaperColor", screen_w / 2, screen_h / 2);

    Canvas.pushSprite(0, 0);
}

void loop()
{
    M5.update();
    delay(100);
}
