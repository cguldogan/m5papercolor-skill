# Audio reference — speaker + mic

The PaperColor audio chain:
- **ES8311** (I2C 0x18) — playback codec, drives the AW8737A amp into an 8 Ω 1 W speaker.
- **ES7210** (I2C 0x40) — mic ADC with hardware AEC.
- **AW8737A** — speaker amp, enable on G46.
- Codec + mic power on G45 (`AUDIO_PWR_EN`, rail `CODEC_3V3_L3B`).

I2S MCLK/LRCK/BCLK shared on G42/G41/G40; `G39 = I2S_DSDIN` (playback), `G38 = I2S_SDOUT` (capture).

`M5Unified` handles all of the codec init, pin muxing, and amp enable for you — you just call `M5.Speaker.begin()` and `M5.Mic.begin()`.

## Hard rule: mic and speaker are mutually exclusive

They share the same I2S peripheral and the same `CODEC_3V3_L3B` rail. Only one can be `begin()`-ed at a time.

```cpp
M5.Mic.end();
M5.Speaker.begin();
// ... play ...
M5.Speaker.end();
M5.Mic.begin();
```

## Speaker

```cpp
M5.Speaker.begin();
M5.Speaker.setVolume(255);             // 0..255 — 200 is typical, 255 is loud
M5.Speaker.tone(4000, 200);            // 4 kHz for 200 ms
M5.Speaker.tone(8000, 200, /*ch*/0, /*stop_current*/true);

bool playing = M5.Speaker.isPlaying();

M5.Speaker.playRaw(int16_pcm, sample_count, rate_hz,
                   /*stereo*/false, /*repeat*/1, /*virtual_ch*/0);
M5.Speaker.playWav(wav_bytes, byte_len);       // parses RIFF header
M5.Speaker.playWav(wav_bytes, byte_len, /*repeat*/1);
M5.Speaker.stop();
M5.Speaker.end();
```

### Minimal beep

```cpp
#include <M5Unified.h>

void setup() {
    M5.begin();
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);
}

void loop() {
    M5.Speaker.tone(4000, 200); delay(2000);
    M5.Speaker.tone(8000, 200); delay(2000);
}
```

### Play a WAV embedded in flash

```cpp
#include "boot_sfx.h"   // const uint8_t boot_sfx[] PROGMEM = { ... };
M5.Speaker.setVolume(200);
M5.Speaker.playWav(boot_sfx, sizeof(boot_sfx));
while (M5.Speaker.isPlaying()) { delay(10); }
```

## Microphone

```cpp
M5.Mic.begin();
bool ok = M5.Mic.record(int16_buf, sample_count, sample_rate_hz);
bool rec = M5.Mic.isRecording();
M5.Mic.end();
```

`record()` is non-blocking: it queues a chunk to the I2S DMA and returns whether the queue accepted it. Call it in a tight loop, advance your write pointer when it returns true.

### Push-to-talk loopback (full example)

Press Button A → records 16 kHz mono into PSRAM; release → plays back.

```cpp
#include <M5Unified.h>

static constexpr size_t REC_CHUNKS = 256;     // ~3.2 s at 200 samples/chunk @ 16 kHz
static constexpr size_t REC_LEN    = 200;
static constexpr size_t REC_TOTAL  = REC_CHUNKS * REC_LEN;
static constexpr uint32_t REC_RATE = 16000;

int16_t* buf;
size_t idx = 0;
bool recording = false, play_now = false;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    buf = (int16_t*)heap_caps_malloc(REC_TOTAL * sizeof(int16_t), MALLOC_CAP_8BIT);  // ~100 KB in PSRAM
    memset(buf, 0, REC_TOTAL * sizeof(int16_t));

    M5.Speaker.setVolume(255);
    M5.Speaker.end();
    M5.Mic.begin();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed() && M5.Mic.isEnabled()) {
        recording = true; idx = 0;
        memset(buf, 0, REC_TOTAL * sizeof(int16_t));
    }
    if (recording && M5.Mic.isEnabled()) {
        if (M5.Mic.record(&buf[idx * REC_LEN], REC_LEN, REC_RATE)) {
            if (++idx >= REC_CHUNKS) idx = 0;
        }
        if (M5.BtnA.wasReleased()) {
            recording = false; play_now = true;
            while (M5.Mic.isRecording()) M5.delay(1);
            M5.Mic.end(); M5.Speaker.begin();    // hand off
        }
    }
    if (play_now && M5.Speaker.isEnabled()) {
        M5.Speaker.playRaw(buf, REC_TOTAL, REC_RATE, /*stereo*/false, 1, 0);
        while (M5.Speaker.isPlaying()) { M5.delay(1); M5.update(); }
        M5.Speaker.end(); M5.Mic.begin();        // hand back
        play_now = false;
    }
    M5.delay(1);
}
```

## Sample-rate guidance

- 8 kHz — voice memos, tiny buffers
- **16 kHz** — voice + speech recognition (factory default)
- 22.05 kHz / 44.1 kHz — music; consumes 4× the buffer space
- 48 kHz — only via `M5.Speaker.config()` overrides; not commonly worth it on a 1 W mono speaker

Allocate big buffers in PSRAM with `heap_caps_malloc(size, MALLOC_CAP_8BIT)` — DRAM tops out around 300 KB free.

## Gotchas

- **Mute on boot:** the amp enable (G46) and codec power (G45) are off until `M5.begin()` runs. If you call `M5.Speaker.tone()` before `M5.Speaker.begin()`, nothing comes out and there's no error.
- **Calling `M5.Speaker.begin()` while mic is active** → garbage audio or silent failure. Always `end()` the other one first.
- **The mic is mono via ES7210**, even though the codec is 2-channel. Don't pass `stereo=true` to playback if you recorded mono.
- **AEC** is handled in hardware by ES7210 — you don't have to do anything in software.
- **No headphone jack** on this board.

## API references

- `Speaker_Class`: https://docs.m5stack.com/en/arduino/m5unified/speaker_class
- `Mic_Class`: https://docs.m5stack.com/en/arduino/m5unified/mic_class
- ES8311 datasheet (public 12-page version) lists I2C protocol and pinout; the full register map ships with ESP-IDF's `es8311` codec driver — read that if you need to bypass `M5.Speaker`.
