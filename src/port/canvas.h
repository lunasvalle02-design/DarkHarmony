// Small software canvas for the bottom screen: a fixed 256x192 XRGB8888
// surface (the Nintendo DS resolution, so it scales exactly 4x on the
// RG DS Plus 1024x768 panel) with rectangles, panels and a built-in 5x7 font.
#pragma once

#include <cstdint>

namespace port {

constexpr int kCanvasW = 256;
constexpr int kCanvasH = 192;

using Rgb = std::uint32_t;  // 0x00RRGGBB

constexpr Rgb rgb(int r, int g, int b) {
    return (static_cast<Rgb>(r) << 16) | (static_cast<Rgb>(g) << 8) | static_cast<Rgb>(b);
}

enum class Align { Left, Center, Right };

class Canvas {
public:
    std::uint32_t* pixels() { return px_; }
    const std::uint32_t* pixels() const { return px_; }

    void clear(Rgb c);
    void fill(int x, int y, int w, int h, Rgb c);
    void frame(int x, int y, int w, int h, Rgb c);
    // Dark gradient panel with a one-pixel bevel, in the style of the game's
    // menu windows.
    void panel(int x, int y, int w, int h, Rgb top, Rgb bottom, Rgb border);
    void blend(int x, int y, int w, int h, Rgb c, int alpha /*0..255*/);
    void put(int x, int y, Rgb c) {
        if (x >= 0 && y >= 0 && x < kCanvasW && y < kCanvasH) px_[y * kCanvasW + x] = c;
    }

    // UTF-8 text (ASCII plus Spanish accented letters, drawn upper-case).
    // Glyphs are 5x7 in a 6 px advance; `scale` multiplies both.
    int text(int x, int y, const char* utf8, Rgb c, Align align = Align::Left,
             int scale = 1, bool shadow = true);
    static int text_width(const char* utf8, int scale = 1);

private:
    void glyph(int x, int y, std::uint32_t code, Rgb c, int scale);

    std::uint32_t px_[kCanvasW * kCanvasH] = {};
};

// Draws `utf8` into an RGB888 buffer (used to label the held top-screen frame).
void draw_text_rgb888(std::uint8_t* rgb, int width, int height, int x, int y,
                      const char* utf8, Rgb c, int scale);

}  // namespace port
