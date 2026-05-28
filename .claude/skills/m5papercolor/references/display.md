# Display reference

The PaperColor screen is a **400 x 600 E Ink Spectra 6** panel — 6 colors only, no anti-aliasing. Driven through `M5.Display` (which is `M5GFX` underneath), with the usual M5GFX/LovyanGFX API.

## Refresh modes

Pick the slowest mode that matches your content — faster modes ghost more and clip colors.

```cpp
namespace m5 { namespace epd_mode {
    enum epd_mode_t : uint8_t {
        epd_quality = 1,   // best color, slowest (~5-15 s full refresh)
        epd_text    = 2,   // tuned for text — keeps blacks crisp
        epd_fast    = 3,   // faster, lower color fidelity
        epd_fastest = 4,   // partial-region updates, very washed-out color
    };
}}
M5.Display.setEpdMode(epd_mode_t::epd_quality);
```

**Pattern:** flip to `epd_fastest` for transient UI (status text, frame counters), back to `epd_quality` for the persistent screen.

## Colors

Only these six are physical pixels:

```cpp
BLACK, WHITE, RED, YELLOW, GREEN, BLUE
```

Anything else (e.g. `RGB565(0x8410)`) gets dithered. There are also `TFT_WHITE`, `TFT_BLACK`, etc., from M5GFX — equivalent to the bare names above. Avoid `MAGENTA`/`CYAN`/`ORANGE` etc., they all dither.

## Sprite (off-screen canvas) — the only sane way to draw

E-ink hates partial updates. Build the whole frame in PSRAM, then `pushSprite` once.

```cpp
M5Canvas canvas(&M5.Display);

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;   // mandatory on E-Ink
    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void redraw() {
    canvas.fillSprite(WHITE);
    canvas.setTextColor(BLACK);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.drawString("Hello", M5.Display.width()/2, M5.Display.height()/2);
    canvas.pushSprite(0, 0);     // single refresh
}
```

400 x 600 x 2 bytes = 480 KB — fits in PSRAM, won't fit in DRAM. The board's 8 MB PSRAM is enabled by `BOARD_HAS_PSRAM` + `qio_opi`.

## Drawing API (M5GFX subset)

```cpp
canvas.fillSprite(color);
canvas.fillScreen(color);                                 // alias of fillSprite for sprites
canvas.drawRect (x, y, w, h, color);
canvas.fillRect (x, y, w, h, color);
canvas.drawLine (x0, y0, x1, y1, color);
canvas.drawCircle(x, y, r, color);
canvas.fillCircle(x, y, r, color);
canvas.drawPixel (x, y, color);

canvas.setRotation(0..3);                                 // 0/2 = portrait 400x600, 1/3 = landscape 600x400
canvas.width(); canvas.height();
canvas.pushSprite(x, y);
canvas.pushSprite(&M5.Display, x, y);
```

## Text

```cpp
canvas.setTextColor(color, bg = TRANSPARENT);
canvas.setTextDatum(top_left | top_center | middle_center | bottom_right | ...);
canvas.setTextSize(1.0f);
canvas.setFont(&fonts::FreeSansBold24pt7b);
canvas.setTextFont(4);                                    // numbered builtin fonts (1..8)
canvas.drawString("text", x, y);
canvas.drawString(String, x, y);
int w = canvas.textWidth("text");
```

**Useful builtin fonts on this panel:**
- `fonts::FreeSansBold24pt7b` — big headings
- `fonts::FreeSansBold18pt7b` — body
- `fonts::FreeSansBold12pt7b` — small labels
- `fonts::FreeSansBold9pt7b` — fine print
- `fonts::FreeMonoBold24pt7b` — sensor readouts (digits line up)
- `fonts::Font4` / `setTextFont(4)` — fast Arial-like 26 px

**Fake-bold trick for stronger contrast** (E-Ink eats thin strokes):

```cpp
static void boldText(M5Canvas& c, const String& s, int x, int y, uint16_t color) {
    c.setTextColor(color);
    c.drawString(s, x,   y);
    c.drawString(s, x+1, y);
    c.drawString(s, x,   y+1);
    c.drawString(s, x+1, y+1);
}
```

## Drawing images from microSD

```cpp
#include <SD.h>
// ...after SD.begin() succeeds (see storage.md):
M5.Display.drawPngFile(SD, "/picture.png");
canvas.drawJpgFile(SD, "/photo.jpg", x, y, /*maxW*/0, /*maxH*/0, /*offX*/0, /*offY*/0, scale, scale);
canvas.drawBmpFile(SD, "/banner.bmp", x, y);
```

The slideshow pattern from the factory demo:

```cpp
int iw, ih;
get_image_size_from_file(path, &iw, &ih);
float scale = std::min((float)scr_w / iw, (float)scr_h / ih);
int dx = (scr_w - (int)(iw * scale)) / 2;
int dy = (scr_h - (int)(ih * scale)) / 2;
canvas.fillScreen(TFT_WHITE);
canvas.drawJpgFile(path, dx, dy, 0, 0, 0, 0, scale, scale);
canvas.pushSprite(0, 0);
```

## Complete demo — six-band display test

```cpp
#include <M5Unified.h>

M5Canvas canvas(&M5.Display);

void setup() {
    auto cfg = M5.config();
    cfg.clear_display = false;
    M5.begin(cfg);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    const int w = M5.Display.width();
    const int h = M5.Display.height();
    const uint16_t bands[6] = {YELLOW, RED, GREEN, BLUE, BLACK, WHITE};
    const int band_h = h / 6;

    canvas.fillSprite(WHITE);
    for (int i = 0; i < 6; ++i) {
        int y = i * band_h;
        int height = (i == 5) ? (h - y) : band_h;
        canvas.fillRect(0, y, w, height, bands[i]);
    }

    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(BLACK);
    canvas.drawString("E6 Spectra", w/2, 5*band_h + band_h/2);

    canvas.pushSprite(0, 0);
}

void loop() { M5.update(); delay(100); }
```

## Gotchas

- **Never call `fillScreen` on `M5.Display` directly in a loop** — it triggers a full slow refresh every iteration. Draw into a sprite, push once.
- **`setRotation(1)` or `(3)` is landscape 600 x 400.** `(0)` and `(2)` are portrait 400 x 600.
- **`setEpdMode` is sticky.** Set it once in `setup` (or before each redraw if you change modes).
- **No backlight, no touch.** Don't look for `M5.Lcd.setBrightness` or `M5.Touch`.
- **Refresh blocking:** `pushSprite` blocks the calling task while the panel ticks — for `epd_quality` that's many seconds.
