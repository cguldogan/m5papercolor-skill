#include <M5Unified.h>

M5Canvas Canvas(&M5.Display);

void setup()
{
    auto cfg          = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);
    Serial.println("\n[hello] boot");

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    const int screen_w = M5.Display.width();
    const int screen_h = M5.Display.height();
    Serial.printf("[hello] display %d x %d\n", screen_w, screen_h);

    Canvas.createSprite(screen_w, screen_h);
    Canvas.fillSprite(WHITE);

    Canvas.setTextFont(4);
    Canvas.setTextSize(2);
    Canvas.setTextDatum(middle_center);
    Canvas.setTextColor(BLACK);
    Canvas.drawString("Hello PaperColor", screen_w / 2, screen_h / 2);

    Canvas.pushSprite(0, 0);
    Serial.println("[hello] frame pushed");
}

void loop()
{
    M5.update();
    static uint32_t last = 0;
    if (millis() - last >= 2000) {
        last = millis();
        Serial.printf("[hello] up %lu ms\n", millis());
    }
    delay(100);
}
