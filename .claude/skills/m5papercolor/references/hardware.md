# Hardware reference — M5PaperColor (SKU C151)

## Specs

| Item | Value |
|---|---|
| SoC | ESP32-S3R8 — dual-core Xtensa LX7 @ 240 MHz |
| Flash | 16 MB |
| PSRAM | 8 MB (Octal, `qio_opi`) |
| Wi-Fi | 2.4 GHz |
| Display | 4" E Ink Spectra 6 — ED2208-DOA (panel `EL040EF1`), 400 x 600, 6 colors |
| Battery | 1250 mAh LiPo |
| Audio codec | ES8311 (I2C 0x18) |
| Audio ADC | ES7210 (I2C 0x40) — used for MEMS mic + AEC |
| Speaker amp | AW8737A, 1 W into 8 Ω |
| Temp/humidity | SHT40 (I2C 0x44) |
| RTC | RX8130CE (I2C 0x32) |
| Power IC | M5PM1 (I2C 0x6E) |
| User input | 3x user buttons + 1x power button (also ON/OFF/RESET/BOOT) |
| IR | TX only, on G48 |
| RGB | 2x WS2812 on G21 |
| Expansion | HY2.0-4P Grove Port A on G4/G5 |
| Standby current | 92.53 µA |
| Full-load current | 211.97 mA |
| Size | 70.8 x 103.9 x 8.5 mm |
| Weight | 73.3 g |

## Pin map

### E-paper (`ED2208-DOA` / `EL040EF1`)

| ESP32-S3 | Function |
|---|---|
| G15 | SPI_CLK (shared with microSD) |
| G13 | SPI_MOSI (shared with microSD) |
| G44 | EINK_CS |
| G43 | EINK_DC |
| G11 | EINK_BUSY |
| G12 | EINK_RST |

Power-enable comes from M5PM1 GPIO **PYG0 → `PY_EPD_EN`** (active high).

### Buttons

| ESP32-S3 | Label | M5Unified object |
|---|---|---|
| G1 | USER_KEY1 / Button C | `M5.BtnC` |
| G9 | USER_KEY2 / Button B | `M5.BtnB` |
| G10 | USER_KEY3 / Button A | `M5.BtnA` |

The power button is wired to the M5PM1, not to the ESP32 directly — see [power.md](power.md) for `pm1.btnGetState()`.

### IR

| ESP32-S3 | Function |
|---|---|
| G48 | IR_TX |

### RGB LED

| ESP32-S3 | Function |
|---|---|
| G21 | 2x WS2812 chain (NEO_GRB + NEO_KHZ800) |

LDO must be enabled on M5PM1 (`pm1.setLdoEnable(true)`).

### Audio — shared I2C, separate I2S frames

```
G2 = AUDIO_I2C_SCL   (ES8311 0x18, ES7210 0x40)
G3 = AUDIO_I2C_SDA
```

Note: G2/G3 are the same pins as `SYS_SCL`/`SYS_SDA` (shared system I2C bus — M5PM1, RTC, SHT40 are on the same wires).

**I2S (ES8311 — playback):**
- G42 = I2S_MCLK
- G41 = I2S_LRCK
- G40 = I2S_BCLK
- G39 = I2S_DSDIN

**I2S (ES7210 — mic capture):** same MCLK/LRCK/BCLK, but `G38 = I2S_SDOUT`.

**Audio power:**
- G45 = AUDIO_PWR_EN (codec + mic, rail `CODEC_3V3_L3B`)
- G46 = SPK_EN (AW8737A enable)

### microSD (SPI)

| ESP32-S3 | Function |
|---|---|
| G47 | CS |
| G15 | SPI_CLK (shared with EPD) |
| G13 | SPI_MOSI (shared with EPD) |
| G14 | SPI_MISO |

Plus three M5PM1 GPIO controls (see [storage.md](storage.md)):
- PYG3 → PY_SD_PWR_EN (output high)
- PYG4 → PY_SD_DET_EN (output high)
- PYG1 → CARD_DEC (input pull-up — LOW = card inserted)

### Grove Port A (HY2.0-4P)

| Pin | Color | ESP32-S3 |
|---|---|---|
| 1 | Black | GND |
| 2 | Red | 5 V |
| 3 | Yellow | G4 |
| 4 | White | G5 |

Grove output direction is gated by `M5PM1::BOOST5V_EN_PP` (PY_GROVE_OUT_EN).

### System I2C — M5PM1, RTC, SHT40

All three sit on the same internal bus exposed by `M5.In_I2C`:

| ESP32-S3 | Function | Devices |
|---|---|---|
| G2 | SYS_SCL | M5PM1 0x6E, RX8130 0x32, SHT40 0x44 |
| G3 | SYS_SDA | (same) |
| G7 | RTC_IRQ | RX8130 alarm → ESP32 |

### M5PM1 (0x6E) — PYGx pin functions

| PYG | Function on PaperColor |
|---|---|
| PYG0 | PY_EPD_EN — e-paper power |
| PYG1 | CARD_DEC — microSD insertion (input pull-up) |
| PYG2 | RTC_IRQ — RX8130 interrupt routed through PM1 (for wake-from-shutdown) |
| PYG3 | PY_SD_PWR_EN — microSD VDD |
| PYG4 | PY_SD_DET_EN — microSD detect pull-up enable |

M5PM1 also owns these power switches:
- `DCDC3V3_EN_PP` (PY_MPWR_EN) — 3V3_L2 power layer switch
- `LDO3V3_EN_PP` (PY_RGB_PWR_EN) — RGB LED power rail
- `BOOST5V_EN_PP` (PY_GROVE_OUT_EN) — Grove 5 V output direction

## Power architecture

The board is structured into five levels; **only L3A is on by default after `M5.begin()`**:

| Level | State | What's powered |
|---|---|---|
| L0 | Off | M5PM1 + RTC (from battery only) |
| L1 | M5PM1 standby | (same as L0) |
| L2 | ESP32-S3 deep sleep | ESP32-S3, SHT40, button pull-ups (3V3_L2) |
| L3A | Active (default) | L2 + normal ESP32 operation |
| L3B | Full peripherals | L3A + audio codec/mic, speaker amp, RGB LEDs, Grove, EPD power, microSD, RTC IRQ |

To use **anything in L3B**, init M5PM1 and call `setLdoEnable(true)`, then any extra per-rail enable (see specific reference doc).

## Partition layout (from official UserDemo)

```
nvs,      data, nvs,     0x9000,   0x6000
phy_init, data, phy,     0xf000,   0x1000
factory,  app,  factory, 0x10000,  0x9F0000   (~9.9 MB app slot)
storage,  data, fat,     0xA00000, 0x600000   (~6 MB FAT for assets)
```

For PlatformIO simple sketches the stock `default_16MB.csv` partition table is fine.

## Download mode / power

- **Power on / reset:** press the side power button **once**.
- **Power off:** press the side power button **twice quickly**.
- **Enter download mode:** plug in USB-C, then **hold the side reset button** while the host probes for the device. `pio run --target upload` will then succeed.

## Reference URLs

- Product page: https://docs.m5stack.com/en/core/PaperColor
- Schematic PDF: linked from the product page
- Factory firmware (full ESP-IDF app): https://github.com/m5stack/M5PaperColor-UserDemo
- M5Unified: https://github.com/m5stack/M5Unified
- M5GFX: https://github.com/m5stack/M5GFX
- M5PM1 driver: https://github.com/m5stack/M5PM1
