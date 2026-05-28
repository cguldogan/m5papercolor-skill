#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.hpp>

#define IR_TX_PIN 48

M5Canvas canvas(&M5.Display);

// TV power codes. Each held for 3 s then advances. Press BtnA to skip immediately.
struct Code {
    const char* brand;
    enum { NEC, SAMSUNG, SONY, RC5 } proto;
    uint32_t addr;
    uint32_t cmd;
};

static const Code CODES[] = {
    { "Samsung",  Code::SAMSUNG, 0xE0E0, 0x40BF },   // power
    { "LG",       Code::NEC,     0x04,   0x08   },   // power (NEC variant)
    { "Sony",     Code::SONY,    0x01,   0x15   },   // 12-bit SIRC power
    { "Philips",  Code::RC5,     0x00,   0x0C   },   // RC5 power
    { "NEC gen",  Code::NEC,     0x00FF, 0x40   },   // generic NEC power
    { "Apple TV", Code::NEC,     0x77E1, 0x05   },   // play/pause
};
static const int N_CODES = sizeof(CODES) / sizeof(CODES[0]);
static int idx = 0;
static uint32_t pulse_count = 0;

static void sendOne(const Code& c)
{
    switch (c.proto) {
        case Code::NEC:     IrSender.sendNEC(c.addr, c.cmd, 0); break;
        case Code::SAMSUNG: IrSender.sendSamsung(c.addr, c.cmd, 0); break;
        case Code::SONY:    IrSender.sendSony(c.addr, 12); break;
        case Code::RC5:     IrSender.sendRC5(c.addr, c.cmd); break;
    }
    ++pulse_count;
}

static void render()
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    char buf[64];
    canvas.fillSprite(WHITE);

    canvas.setTextDatum(top_center);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("IR TX TEST", w / 2, 22);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(BLUE);
    canvas.drawString("Aim at TV", w / 2, 78);

    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(RED);
    canvas.drawString(CODES[idx].brand, w / 2, h / 2 - 40);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    snprintf(buf, sizeof(buf), "pulse #%lu", (unsigned long)pulse_count);
    canvas.setTextColor(GREEN);
    canvas.drawString(buf, w / 2, h / 2 + 40);

    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(BLACK);
    canvas.drawString("BtnA = skip brand", w / 2, h - 22);

    canvas.pushSprite(0, 0);
}

void setup()
{
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    gpio_reset_pin(gpio_num_t(IR_TX_PIN));
    // With -DIR_SEND_PIN=48 in platformio.ini, IRremote attaches LEDC to G48 at compile time.
    // No runtime arg is accepted — IrSender.setSendPin() is silently ignored by the ESP32 LEDC path.
    IrSender.begin();

    Serial.println("[ir] ready");
    render();
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed()) {
        idx = (idx + 1) % N_CODES;
        Serial.printf("[ir] BtnA -> %s\n", CODES[idx].brand);
        render();
    }

    static uint32_t last_send = 0;
    if (millis() - last_send >= 800) {                  // burst the current code every 800 ms
        last_send = millis();
        Serial.printf("[ir] send #%lu %s\n", (unsigned long)pulse_count, CODES[idx].brand);
        sendOne(CODES[idx]);
        sendOne(CODES[idx]);                            // double-tap: most TVs need 2 frames
    }

    static uint32_t last_advance = 0;
    if (millis() - last_advance >= 6000) {              // auto-advance every 6 s
        last_advance = millis();
        idx = (idx + 1) % N_CODES;
        render();
    }

    delay(20);
}
