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

### Library dep (with critical build flag)

The M5Stack docs example for IR is broken on IRremote v4 (the library was rewritten between v3 and v4 and the runtime `setSendPin()` no longer works on ESP32). You **must** pass the send pin at compile time:

```ini
lib_deps =
    m5stack/M5Unified @ ^0.2.15
    m5stack/M5GFX @ ^0.2.21
    Arduino-IRremote/IRremote @ ^4.4.0

build_flags =
    ; ...your other flags...
    -DIR_SEND_PIN=48        ; HARD-required for IRremote v4 on ESP32-S3
```

Why: IRremote v4 on ESP32 drives the 38 kHz carrier with the **LEDC** peripheral, calling `ledcAttach(IR_SEND_PIN, ...)`. That `IR_SEND_PIN` is a **compile-time macro**, not a runtime variable — `IrSender.setSendPin(48)` updates an internal field that `ledcAttach()` never reads. Without `-DIR_SEND_PIN=48`, the carrier ends up on whatever pin the library defaults to (often nothing), and the IR LED stays dark.

Bonus footgun in the same docs example: `IrSender.begin(DISABLE_LED_FEEDBACK)`. `DISABLE_LED_FEEDBACK` is `#define`d to `false` (`0`) in IRremote v4. That call resolves to `IrSender.begin((uint_fast8_t)0)`, which silently sets the send pin to **GPIO 0**. Don't pass `DISABLE_LED_FEEDBACK` as the first arg.

### Setup + send pattern (corrected)

```cpp
#include <Arduino.h>
#include <M5Unified.h>
#include <IRremote.hpp>     // IR_SEND_PIN=48 is baked in via build_flags

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);

    gpio_reset_pin(GPIO_NUM_48);    // detach G48 from any other peripheral
    IrSender.begin();                // no args — pin came from build_flags
}

void loop() {
    M5.update();
    IrSender.sendNEC(0x1111, 0x34, /*repeats*/ 0);
    delay(1000);
}
```

### Known-shaky behaviour on ESP32-S3

Even with the compile-time pin fix, IRremote v4 + M5GFX + ESP32-S3 has a **LEDC channel allocation conflict** in some configurations — the first IR burst fires but subsequent ones don't reach the LED. This was observed during skill verification (LED pulse seen on a phone camera once, then went dark). If you need reliable sustained IR TX, options:

1. **Drive the IR LED yourself**: `digitalWrite(48, HIGH/LOW)` works at the GPIO level (confirmed by raw toggle test). Write a hand-rolled NEC encoder that toggles the pin at 38 kHz during marks and holds LOW during spaces. ~100 lines.
2. **Use the ESP32 RMT peripheral directly** via `driver/rmt.h` — that's what production-grade IR codebases do and what IRremote v4 *should* be using on ESP-IDF.
3. **Pin LEDC ownership** by passing `-DSEND_LEDC_CHANNEL=4` to keep IRremote off the channels M5GFX uses (channels 0-3 are commonly taken by audio/EPD).

### Verified on hardware

- The IR LED on G48 fires correctly under raw `digitalWrite` — see [examples/ir-tv-blaster/](../examples/ir-tv-blaster/) (build-verified) and the raw-toggle test referenced in commit history.
- IRremote v4 with the `-DIR_SEND_PIN=48` fix transmits once but not reliably across a `pushSprite` cycle.

### Other supported protocols (from Arduino-IRremote)

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
- Call `gpio_reset_pin(GPIO_NUM_48)` before `IrSender.begin()` — the pin defaults to a state that breaks NEC encoding.
- The IR LED draws from the main 3V3 rail; no M5PM1 enable is needed (it's not L3B). Confirmed: raw `digitalWrite(48, HIGH)` lights the LED without any M5PM1 calls.
- `DISABLE_LED_FEEDBACK` is `0` in IRremote v4 — **don't** pass it as the first argument to `begin()`.
- IRremote v4 on ESP32 ignores the runtime `setSendPin()` for LEDC carrier generation. Always use `-DIR_SEND_PIN=...` as a build flag.
