# Input reference — buttons + IR

## User buttons

Three physical buttons mapped through `M5Unified`'s `Button_Class`:

| ESP32 pin | M5Unified object | Label on case |
|---|---|---|
| G1 | `M5.BtnC` | USER_KEY1 |
| G9 | `M5.BtnB` | USER_KEY2 |
| G10 | `M5.BtnA` | USER_KEY3 |

`M5.update()` **must be called in `loop()`** for state to refresh.

```cpp
void loop() {
    M5.update();
    if (M5.BtnA.wasPressed())  Serial.println("A press");   // edge: just pressed
    if (M5.BtnA.wasReleased()) Serial.println("A release"); // edge: just released
    if (M5.BtnA.wasClicked())  Serial.println("A click");   // pressed then released
    if (M5.BtnA.isPressed())   Serial.println("A held");    // level
    if (M5.BtnA.wasHold())     Serial.println("A held > setHoldThresh()");
    delay(10);
}
```

Other useful `Button_Class` methods:
- `pressedFor(ms)` — true if held at least `ms`
- `releasedFor(ms)`
- `wasReleaseFor(ms)`  — released after being held at least `ms`
- `setHoldThresh(ms)` / `setDebounceThresh(ms)`

### Power button (side button)

Not connected to the ESP32 — it goes to the **M5PM1**. Read it via `pm1.btnGetState()` / `pm1.btnGetFlag()` (see [power.md](power.md)). Default behaviour, configured in M5PM1 firmware:

- Single click → ESP32 RESET (power on)
- Double click → power off
- Long press → enter download mode

To override: `pm1.btnSetConfig(...)` — see the M5PM1 datasheet for `BTN_CFG_1`/`BTN_CFG_2` register fields.

## IR NEC TX

The board has a transmit-only IR LED on **G48**. Drive it with `Arduino-IRremote` (`IRremote.hpp`).

### Library dep

Add to `platformio.ini`:
```ini
lib_deps =
    m5stack/M5Unified @ ^0.2.15
    m5stack/M5GFX @ ^0.2.21
    Arduino-IRremote/IRremote @ ^4.4.0
```

### Setup + send pattern

```cpp
#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.hpp>

#define IR_TX_PIN 48

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);

    gpio_reset_pin(gpio_num_t(IR_TX_PIN));         // mandatory — pin is muxed
    IrSender.begin(DISABLE_LED_FEEDBACK);
    IrSender.setSendPin(IR_TX_PIN);
}

void loop() {
    M5.update();
    IrSender.sendNEC(/*address*/ 0x1111, /*command*/ 0x34, /*repeats*/ 0);
    delay(1000);
}
```

### Other supported protocols (from Arduino-IRremote)

```cpp
IrSender.sendNEC(addr, cmd, repeats);
IrSender.sendSony(cmd, bits);
IrSender.sendRC5(addr, cmd);
IrSender.sendRC6(addr, cmd);
IrSender.sendSamsung(addr, cmd, repeats);
IrSender.sendPanasonic(addr, cmd);
// ...full list in IRremote.hpp
```

For raw timings: `IrSender.sendRaw(buf, len, kHz)`.

### Gotchas

- The board has **no IR receiver**. Don't try to use `IrReceiver`.
- Call `gpio_reset_pin(GPIO_NUM_48)` before `IrSender.begin()` — the pin defaults to a state that breaks NEC encoding.
- The IR LED draws from the main 3V3 rail; no M5PM1 enable is needed (it's not L3B).
