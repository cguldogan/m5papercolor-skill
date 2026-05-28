// Samsung-targeted IR blaster.
// - Default mode: hammer Samsung Power codes (3 variants cycled by Button A).
// - Button B  : switch to RAW 38 kHz square wave on G48 (library-free baseline).
// - Button C  : back to library mode.
// EPD is drawn ONCE on boot and on mode change — never during the IR burst.

#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.hpp>

#define IR_TX_PIN 48

M5Canvas canvas(&M5.Display);

struct SamsungCode { const char* name; uint16_t addr; uint16_t cmd; };
static const SamsungCode CODES[] = {
    { "E0E0 / 40BF",  0xE0E0, 0x40BF },   // most common Samsung power (NEC-extended)
    { "E0E0 / 19E6",  0xE0E0, 0x19E6 },   // Samsung Source/Input — easier to see reacting
    { "0707 / 02FD",  0x0707, 0x02FD },   // older Samsung power
    { "BN59 0xE7",    0xE0E0, 0xE01F },   // VOL+ on newer models
};
static const int N = sizeof(CODES) / sizeof(CODES[0]);
static int idx = 0;

enum Mode { LIB, RAW_CARRIER };
static Mode mode = LIB;
static uint32_t pulses = 0;

static void render()
{
    const int w = M5.Display.width();
    const int h = M5.Display.height();
    char buf[64];
    canvas.fillSprite(WHITE);

    canvas.setTextDatum(top_center);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextColor(BLACK);
    canvas.drawString("IR -> Samsung QC55", w / 2, 22);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(mode == LIB ? GREEN : RED);
    canvas.drawString(mode == LIB ? "MODE: library" : "MODE: raw 38kHz", w / 2, 80);

    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeMonoBold24pt7b);
    canvas.setTextColor(BLUE);
    canvas.drawString(CODES[idx].name, w / 2, h / 2 - 50);

    canvas.setFont(&fonts::FreeSansBold18pt7b);
    snprintf(buf, sizeof(buf), "addr=%04X cmd=%04X", CODES[idx].addr, CODES[idx].cmd);
    canvas.setTextColor(BLACK);
    canvas.drawString(buf, w / 2, h / 2 + 20);

    canvas.setTextDatum(bottom_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawString("A=next code  B=raw  C=library", w / 2, h - 22);

    canvas.pushSprite(0, 0);
}

// ~38 kHz square wave for ~50 ms — easy to spot on a phone camera as a bright pulse.
static IRAM_ATTR void rawCarrierPulse(uint32_t total_us = 50000)
{
    const uint32_t half_us = 13;        // ~38 kHz (1/(2*13us) = 38.46 kHz)
    uint32_t t0 = micros();
    while ((micros() - t0) < total_us) {
        digitalWrite(IR_TX_PIN, HIGH);
        delayMicroseconds(half_us);
        digitalWrite(IR_TX_PIN, LOW);
        delayMicroseconds(half_us);
    }
}

void setup()
{
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    gpio_reset_pin(gpio_num_t(IR_TX_PIN));
    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(IR_TX_PIN);

    render();
    Serial.println("[ir] ready, mode=LIB");
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed()) {
        idx = (idx + 1) % N;
        Serial.printf("[ir] code -> %s\n", CODES[idx].name);
        render();
        return;
    }
    if (M5.BtnB.wasPressed() && mode != RAW_CARRIER) {
        mode = RAW_CARRIER;
        pinMode(IR_TX_PIN, OUTPUT);   // bring G48 back to plain GPIO
        Serial.println("[ir] mode -> RAW 38kHz");
        render();
        return;
    }
    if (M5.BtnC.wasPressed() && mode != LIB) {
        mode = LIB;
        gpio_reset_pin(gpio_num_t(IR_TX_PIN));
        IrSender.begin(DISABLE_LED_FEEDBACK);
        IrSender.setSendPin(IR_TX_PIN);
        Serial.println("[ir] mode -> LIB");
        render();
        return;
    }

    // Burst — every 100 ms, no EPD refresh.
    static uint32_t last = 0;
    if (millis() - last < 100) { delay(2); return; }
    last = millis();

    if (mode == LIB) {
        // 8 repeat frames simulate a held key — commercial signage often gates
        // power/source commands behind a sustained press, and consumer Samsungs
        // sometimes need it for source-switching too.
        IrSender.sendSamsung(CODES[idx].addr, CODES[idx].cmd, /*repeats*/8);
        ++pulses;
        if ((pulses % 5) == 0) Serial.printf("[ir] %lu held-key bursts sent (%s)\n",
                                             (unsigned long)pulses, CODES[idx].name);
    } else {
        rawCarrierPulse(60000);   // 60 ms of 38 kHz on G48
        ++pulses;
        if ((pulses % 10) == 0) Serial.printf("[ir] %lu raw carrier pulses\n", (unsigned long)pulses);
    }
}
