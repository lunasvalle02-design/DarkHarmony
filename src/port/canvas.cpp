#include "canvas.h"

#include <cstring>

namespace port {
namespace {

// 5x7 glyphs, drawn for this port. Lower-case letters render as upper-case.
struct GlyphArt {
    char ch;
    const char* rows[7];
};

constexpr GlyphArt kGlyphs[] = {
    {' ', {".....", ".....", ".....", ".....", ".....", ".....", "....."}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {'"', {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."}},
    {'#', {".#.#.", "#####", ".#.#.", ".#.#.", ".#.#.", "#####", ".#.#."}},
    {'%', {"##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##"}},
    {'\'', {"..#..", "..#..", ".....", ".....", ".....", ".....", "....."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
    {'*', {".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {',', {".....", ".....", ".....", ".....", "..##.", "..#..", ".#..."}},
    {'-', {".....", ".....", ".....", "#####", ".....", ".....", "....."}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
    {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
    {':', {".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."}},
    {';', {".....", ".##..", ".##..", ".....", ".##..", "..#..", ".#..."}},
    {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
    {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
    {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
    {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
    {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {'D', {"###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."}},
    {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'N', {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"}},
    {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."}},
    {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
    {'[', {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."}},
    {']', {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."}},
    {'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    // Private glyphs: heart, left/right arrows, check mark, cross.
    {'\x01', {".#.#.", "#####", "#####", "#####", ".###.", "..#..", "....."}},
    {'\x02', {"...#.", "..##.", ".###.", "####.", ".###.", "..##.", "...#."}},
    {'\x03', {".#...", ".##..", ".###.", ".####", ".###.", ".##..", ".#..."}},
    {'\x04', {".....", "....#", "...##", "#.##.", "###..", ".#...", "....."}},
    {'\x05', {".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "....."}},
};

struct Font {
    std::uint8_t rows[128][7] = {};
    bool present[128] = {};
    Font() {
        for (const auto& g : kGlyphs) {
            const auto c = static_cast<unsigned char>(g.ch);
            present[c] = true;
            for (int r = 0; r < 7; ++r) {
                std::uint8_t bits = 0;
                for (int i = 0; i < 5; ++i)
                    if (g.rows[r][i] == '#') bits |= static_cast<std::uint8_t>(0x10 >> i);
                rows[c][r] = bits;
            }
        }
    }
};

const Font& font() {
    static const Font f;
    return f;
}

// Decodes one UTF-8 code point; returns the number of bytes consumed.
int decode(const char* s, std::uint32_t* cp) {
    const auto* u = reinterpret_cast<const unsigned char*>(s);
    if (u[0] < 0x80) { *cp = u[0]; return 1; }
    if ((u[0] & 0xE0) == 0xC0 && u[1]) { *cp = ((u[0] & 0x1Fu) << 6) | (u[1] & 0x3Fu); return 2; }
    if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) {
        *cp = ((u[0] & 0x0Fu) << 12) | ((u[1] & 0x3Fu) << 6) | (u[2] & 0x3Fu);
        return 3;
    }
    *cp = '?';
    return 1;
}

// Spanish letters: base glyph plus the mark drawn above it.
enum Mark { kNone, kAcute, kTilde, kDiaeresis };
std::uint32_t base_of(std::uint32_t cp, Mark* mark) {
    *mark = kNone;
    switch (cp) {
        case 0xE1: case 0xC1: *mark = kAcute; return 'A';
        case 0xE9: case 0xC9: *mark = kAcute; return 'E';
        case 0xED: case 0xCD: *mark = kAcute; return 'I';
        case 0xF3: case 0xD3: *mark = kAcute; return 'O';
        case 0xFA: case 0xDA: *mark = kAcute; return 'U';
        case 0xFC: case 0xDC: *mark = kDiaeresis; return 'U';
        case 0xF1: case 0xD1: *mark = kTilde; return 'N';
        case 0xBF: return '?';  // inverted question mark
        case 0xA1: return '!';
        case 0x2665: return '\x01';
        case 0x25C0: return '\x02';
        case 0x25B6: return '\x03';
        case 0x2713: return '\x04';
        case 0x2715: return '\x05';
        default: break;
    }
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    return cp < 128 ? cp : '?';
}

template <typename Plot>
void draw_glyph(std::uint32_t cp, int x, int y, int scale, Plot plot) {
    Mark mark;
    const std::uint32_t base = base_of(cp, &mark);
    const Font& f = font();
    if (base >= 128 || !f.present[base]) return;
    for (int r = 0; r < 7; ++r)
        for (int i = 0; i < 5; ++i)
            if (f.rows[base][r] & (0x10 >> i))
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx) plot(x + i * scale + sx, y + r * scale + sy);
    // Marks sit in the line gap above the cap height.
    auto dot = [&](int i, int r) {
        for (int sy = 0; sy < scale; ++sy)
            for (int sx = 0; sx < scale; ++sx) plot(x + i * scale + sx, y + r * scale + sy);
    };
    if (mark == kAcute) { dot(3, -3); dot(2, -2); }
    if (mark == kTilde) { dot(1, -2); dot(2, -3); dot(3, -2); dot(4, -3); }
    if (mark == kDiaeresis) { dot(1, -2); dot(3, -2); }
}

}  // namespace

void Canvas::clear(Rgb c) {
    for (auto& p : px_) p = c;
}

void Canvas::fill(int x, int y, int w, int h, Rgb c) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i) put(i, j, c);
}

void Canvas::frame(int x, int y, int w, int h, Rgb c) {
    fill(x, y, w, 1, c);
    fill(x, y + h - 1, w, 1, c);
    fill(x, y, 1, h, c);
    fill(x + w - 1, y, 1, h, c);
}

void Canvas::panel(int x, int y, int w, int h, Rgb top, Rgb bottom, Rgb border) {
    for (int j = 0; j < h; ++j) {
        const int t = h > 1 ? j * 256 / (h - 1) : 0;
        auto lerp = [t](int a, int b) { return a + (b - a) * t / 256; };
        const Rgb c = rgb(lerp((top >> 16) & 255, (bottom >> 16) & 255),
                          lerp((top >> 8) & 255, (bottom >> 8) & 255),
                          lerp(top & 255, bottom & 255));
        fill(x, y + j, w, 1, c);
    }
    frame(x, y, w, h, border);
}

void Canvas::blend(int x, int y, int w, int h, Rgb c, int alpha) {
    for (int j = y; j < y + h; ++j) {
        for (int i = x; i < x + w; ++i) {
            if (i < 0 || j < 0 || i >= kCanvasW || j >= kCanvasH) continue;
            Rgb& d = px_[j * kCanvasW + i];
            auto mix = [alpha](int a, int b) { return a + (b - a) * alpha / 255; };
            d = rgb(mix((d >> 16) & 255, (c >> 16) & 255), mix((d >> 8) & 255, (c >> 8) & 255),
                    mix(d & 255, c & 255));
        }
    }
}

int Canvas::text_width(const char* utf8, int scale) {
    int n = 0;
    for (const char* s = utf8; *s;) {
        std::uint32_t cp;
        s += decode(s, &cp);
        ++n;
    }
    return n ? (n * 6 - 1) * scale : 0;
}

void Canvas::glyph(int x, int y, std::uint32_t code, Rgb c, int scale) {
    draw_glyph(code, x, y, scale, [&](int px, int py) { put(px, py, c); });
}

int Canvas::text(int x, int y, const char* utf8, Rgb c, Align align, int scale, bool shadow) {
    const int w = text_width(utf8, scale);
    if (align == Align::Center) x -= w / 2;
    else if (align == Align::Right) x -= w;
    int cx = x;
    for (const char* s = utf8; *s;) {
        std::uint32_t cp;
        s += decode(s, &cp);
        if (shadow) glyph(cx + scale, y + scale, cp, rgb(0, 0, 0), scale);
        glyph(cx, y, cp, c, scale);
        cx += 6 * scale;
    }
    return x + w;
}

void draw_text_rgb888(std::uint8_t* rgb_buf, int width, int height, int x, int y,
                      const char* utf8, Rgb c, int scale) {
    auto plot_color = [&](Rgb color) {
        return [=](int px, int py) {
            if (px < 0 || py < 0 || px >= width || py >= height) return;
            std::uint8_t* p = rgb_buf + (py * width + px) * 3;
            p[0] = static_cast<std::uint8_t>(color >> 16);
            p[1] = static_cast<std::uint8_t>(color >> 8);
            p[2] = static_cast<std::uint8_t>(color);
        };
    };
    int cx = x - Canvas::text_width(utf8, scale) / 2;
    for (const char* s = utf8; *s;) {
        std::uint32_t cp;
        s += decode(s, &cp);
        draw_glyph(cp, cx + scale, y + scale, scale, plot_color(0));
        draw_glyph(cp, cx, y, scale, plot_color(c));
        cx += 6 * scale;
    }
}

}  // namespace port
