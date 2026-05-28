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

```ini
lib_deps =
    m5stack/M5Unified @ ^0.2.15
    m5stack/M5GFX @ ^0.2.21
    Arduino-IRremote/IRremote @ ^4.4.0
```

### Setup + send pattern (M5Stack canonical)

```cpp
#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.hpp>

#define IR_TX_PIN 48

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);

    gpio_reset_pin(gpio_num_t(IR_TX_PIN));   // detach G48 from any other peripheral
    IrSender.begin(DISABLE_LED_FEEDBACK);    // resolves to begin(uint_fast8_t aSendPin=0) — placeholder
    IrSender.setSendPin(IR_TX_PIN);          // overrides sendPin to 48 before the first send
}

void loop() {
    M5.update();
    IrSender.sendNEC(/*addr*/ 0x1111, /*cmd*/ 0x34, /*repeats*/ 0);
    delay(1000);
}
```

Why this works (even though it looks odd): in IRremote v4, when `IR_SEND_PIN` is *not* defined as a build flag, the ESP32 LEDC carrier code in `IRTimer.hpp` reads `IrSender.sendPin` (the runtime field) inside `timerConfigForSend()` and calls `ledcAttach(IrSender.sendPin, ...)`. So the order `begin(0) → setSendPin(48) → sendNEC()` correctly attaches LEDC to G48 before the first transmission. The `DISABLE_LED_FEEDBACK` arg looks like it's disabling feedback but is actually a placeholder for the send-pin field that gets overwritten one line later — harmless because there's no feedback-LED hardware to drive.

### Alternative: compile-time pin pattern

If you prefer to bake the pin in at build time:

```ini
build_flags =
    -DIR_SEND_PIN=48
```

```cpp
#include <IRremote.hpp>   // IR_SEND_PIN=48 is honored at compile time
void setup() {
    gpio_reset_pin(GPIO_NUM_48);
    IrSender.begin();      // no-arg form; setSendPin() is #ifndef'd out
}
```

Don't mix the two — with `-DIR_SEND_PIN=...` set, `IrSender.setSendPin(...)` won't compile.

```cpp
IrSender.sendNEC(addr, cmd, repeats);
IrSender.sendSamsung(addr, cmd, repeats);
IrSender.sendSony(cmd, bits);
IrSender.sendRC5(addr, cmd);
// ... full list in IRremote.hpp
```

For raw timings: `IrSender.sendRaw(buf, len, kHz)`.

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
- Call `gpio_reset_pin(gpio_num_t(IR_TX_PIN))` before `IrSender.begin()` — the pin may default to a strapping/USB state.
- The IR LED draws from the main 3V3 rail; no M5PM1 enable is needed (it's not L3B). Confirmed: raw `digitalWrite(48, HIGH)` lights the LED without any M5PM1 calls.
- **Phone-camera verification needs patience.** NEC frames are only ~67 ms long. If you fire a single NEC per second, the camera sees a brief flicker, not a sustained glow. To eyeball the LED clearly, either burst frames continuously (every ~100 ms) or briefly switch to raw `digitalWrite(48, HIGH)` for a longer pulse.
