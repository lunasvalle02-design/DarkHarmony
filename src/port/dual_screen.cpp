#include "dual_screen.h"

#include "canvas.h"
#include "hod_map.h"
#include "hod_ram.h"
#include "rocknix_sleep.h"
#include "port_strings.h"

#include "gba_bus.h"
#include "host_window.h"
#include "runtime_bus_bridge.h"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace port {
namespace {

using gbarecomp::HostRequest;
using gbarecomp::host_request;

constexpr int kGbaW = 240;
constexpr int kGbaH = 160;
constexpr int kHeaderH = 16;
constexpr int kTabBarY = 176;
constexpr int kContentY = kHeaderH;
constexpr int kContentH = kTabBarY - kHeaderH;  // 160: exactly the castle map

// Colours of the bottom-screen UI, in the blue/gold of the game's menus.
constexpr Rgb kBgTop = rgb(10, 14, 44);
constexpr Rgb kBgBottom = rgb(4, 6, 18);
constexpr Rgb kPanelTop = rgb(36, 52, 128);
constexpr Rgb kPanelBottom = rgb(14, 20, 64);
constexpr Rgb kGold = rgb(232, 196, 96);
constexpr Rgb kBorder = rgb(150, 160, 210);
constexpr Rgb kText = rgb(236, 236, 244);
constexpr Rgb kDim = rgb(140, 148, 180);
constexpr Rgb kHpColor = rgb(214, 44, 52);
constexpr Rgb kMpColor = rgb(56, 100, 232);
constexpr Rgb kHeart = rgb(236, 90, 120);

Rgb map_color(std::uint8_t index) {
    switch (index) {
        case hod::kMapWall: return rgb(236, 236, 244);
        case hod::kMapSaveRoom: return rgb(222, 40, 44);
        case hod::kMapWarpRoom: return rgb(244, 212, 40);
        case hod::kMapRevealed: return rgb(64, 66, 84);
        case hod::kMapRevealedWall: return rgb(140, 142, 156);
        case hod::kMapExplored: return rgb(118, 124, 148);
        case hod::kMapCastleA: return rgb(40, 64, 214);
        case hod::kMapCastleB: return rgb(28, 150, 70);
        case hod::kMapBothCastles: return rgb(24, 168, 190);
        default: return rgb(6, 8, 24);
    }
}

enum class Tab { Map, Status, Settings };

enum class Setting {
    TopScale, Filter, Volume, MenusBelow, SwapAB, Turbo, ShowFps, ResumeAfterSleep,
    SaveState, LoadState, Language, Quit, Count
};

struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

struct State {
    DualScreenOptions opts;
    PortConfig cfg;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* ui_tex = nullptr;
    SDL_Texture* game_tex = nullptr;
    Uint32 window_id = 0;
    bool failed = false;
    bool needs_layout = true;
    int out_w = 0, out_h = 0;
    SDL_Rect ui_dest{};      // canvas on the bottom output
    float ui_scale = 1.0f;

    Canvas canvas;
    Tab tab = Tab::Map;
    bool dirty = true;
    std::uint64_t frame = 0;

    // Map.
    std::vector<std::uint8_t> map_px = std::vector<std::uint8_t>(hod::kMapPixelsW * hod::kMapPixelsH);
    std::uint32_t map_sig = 0;
    int map_view_choice = 0;  // 0 follow current castle, 1 A, 2 B, 3 both
    hod::MapView drawn_view = hod::MapView::Any;
    bool map_valid = false;

    // Game menu on the bottom screen.
    bool mirroring = false;
    std::uint8_t mirror_mode = 0;
    std::vector<std::uint8_t> last_gameplay = std::vector<std::uint8_t>(kGbaW * kGbaH * 3);
    std::vector<std::uint8_t> held_top = std::vector<std::uint8_t>(kGbaW * kGbaH * 3);
    bool have_gameplay = false;

    // Status snapshot for redraw decisions.
    std::uint32_t status_hash = 0;

    // Settings interaction.
    Setting armed = Setting::Count;   // double-tap confirmation
    Uint32 armed_until = 0;
    char toast[64] = {};
    Uint32 toast_until = 0;
    bool turbo = false;

    // Controller (exit combo).
    SDL_GameController* pad = nullptr;

    // Sleep.
    int sleep_release_in = 0;

    // FPS / perf.
    Uint32 fps_window_start = 0;
    int fps_frames = 0;
    float fps = 0.0f;
    bool perf_log = false;
    double perf_bottom_ms = 0.0;
    double perf_bottom_max_ms = 0.0;
    int perf_samples = 0;
};

State* g = nullptr;

const char* t(Str id) { return tr(id, g->cfg.language); }

void save_config() {
    if (!g->opts.config_path.empty() && !g->cfg.save(g->opts.config_path))
        std::fprintf(stderr, "dual_screen: could not write %s\n", g->opts.config_path.c_str());
}

void show_toast(const char* text) {
    std::snprintf(g->toast, sizeof(g->toast), "%s", text);
    g->toast_until = SDL_GetTicks() + 2000;
    g->dirty = true;
}

hod::MapSources sources() {
    hod::MapSources src;
    if (auto* bus = gbarecomp::active_bus()) {
        src.rom = bus->rom_ptr();
        src.ewram = bus->ewram_ptr();
    }
    return src;
}

// ---- window -------------------------------------------------------------

bool create_window() {
    const int displays = SDL_GetNumVideoDisplays();
    SDL_Window* top = SDL_GetWindowFromID(1);
    int top_display = top ? SDL_GetWindowDisplayIndex(top) : 0;
    if (top_display < 0) top_display = 0;
    int target = top_display;
    for (int i = 0; i < displays; ++i)
        if (i != top_display) { target = i; break; }
    if (const char* env = std::getenv("HOD_BOTTOM_DISPLAY")) {
        const int forced = std::atoi(env);
        if (forced >= 0 && forced < displays) target = forced;
    }
    SDL_Rect bounds{0, 0, 1024, 768};
    SDL_GetDisplayBounds(target, &bounds);

    g->window = SDL_CreateWindow(g->opts.window_title.c_str(),
                                 SDL_WINDOWPOS_CENTERED_DISPLAY(target),
                                 SDL_WINDOWPOS_CENTERED_DISPLAY(target), bounds.w, bounds.h,
                                 SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS);
    if (!g->window) {
        std::fprintf(stderr, "dual_screen: CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    // The bottom screen never waits for vsync: the game window owns pacing.
    const char* mode = std::getenv("HOD_BOTTOM_RENDERER");
    const bool software = mode && std::strcmp(mode, "software") == 0;
    if (!software) g->renderer = SDL_CreateRenderer(g->window, -1, SDL_RENDERER_ACCELERATED);
    if (!g->renderer) g->renderer = SDL_CreateRenderer(g->window, -1, SDL_RENDERER_SOFTWARE);
    if (!g->renderer) {
        std::fprintf(stderr, "dual_screen: CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g->window);
        g->window = nullptr;
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    g->ui_tex = SDL_CreateTexture(g->renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
                                  kCanvasW, kCanvasH);
    g->game_tex = SDL_CreateTexture(g->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                    kGbaW, kGbaH);
    g->window_id = SDL_GetWindowID(g->window);

    SDL_RendererInfo info{};
    SDL_GetRendererInfo(g->renderer, &info);
    std::printf("dual_screen: bottom window on display %d of %d (%dx%d), renderer %s%s\n", target,
                displays, bounds.w, bounds.h, info.name ? info.name : "?",
                (info.flags & SDL_RENDERER_ACCELERATED) ? " (accelerated)" : " (software)");
    std::fflush(stdout);
    return g->ui_tex && g->game_tex;
}

void update_layout() {
    g->needs_layout = false;
    SDL_GetRendererOutputSize(g->renderer, &g->out_w, &g->out_h);
    if (g->out_w <= 0 || g->out_h <= 0) { g->out_w = kCanvasW; g->out_h = kCanvasH; }
    // Integer scale when it fills the panel (4x on 1024x768), otherwise fit.
    const float fit = std::min(g->out_w / static_cast<float>(kCanvasW), g->out_h / static_cast<float>(kCanvasH));
    const int whole = static_cast<int>(fit);
    const float scale = (whole >= 1 && fit - whole < 0.05f) ? static_cast<float>(whole) : fit;
    g->ui_scale = scale;
    g->ui_dest.w = static_cast<int>(kCanvasW * scale);
    g->ui_dest.h = static_cast<int>(kCanvasH * scale);
    g->ui_dest.x = (g->out_w - g->ui_dest.w) / 2;
    g->ui_dest.y = (g->out_h - g->ui_dest.h) / 2;
    g->dirty = true;
    std::printf("dual_screen: output %dx%d, canvas %dx%d at %d,%d\n", g->out_w, g->out_h, g->ui_dest.w,
                g->ui_dest.h, g->ui_dest.x, g->ui_dest.y);
    std::fflush(stdout);
}

// ---- drawing ------------------------------------------------------------

void draw_tabs() {
    static const Str names[] = {Str::TabMap, Str::TabStatus, Str::TabSettings};
    const int w = kCanvasW / 3;
    for (int i = 0; i < 3; ++i) {
        const bool active = static_cast<int>(g->tab) == i;
        const int x = i * w + (i == 2 ? 1 : 0);
        const int width = i == 2 ? kCanvasW - x : w;
        g->canvas.panel(x, kTabBarY, width, kCanvasH - kTabBarY,
                        active ? rgb(70, 90, 190) : rgb(28, 34, 80),
                        active ? rgb(30, 40, 110) : rgb(12, 14, 40), active ? kGold : kBorder);
        g->canvas.text(x + width / 2, kTabBarY + 4, t(names[i]), active ? kGold : kDim, Align::Center);
    }
}

void draw_header(const char* title, const char* right) {
    g->canvas.panel(0, 0, kCanvasW, kHeaderH, kPanelTop, kPanelBottom, kBorder);
    const bool toast = SDL_GetTicks() < g->toast_until;
    g->canvas.text(5, 4, toast ? g->toast : title, toast ? kGold : kText);
    char buf[48];
    if (g->cfg.show_fps) {
        std::snprintf(buf, sizeof(buf), "%s%s%.0f FPS", right ? right : "", right ? "  " : "", g->fps);
        right = buf;
    }
    if (right) g->canvas.text(kCanvasW - 5, 4, right, kText, Align::Right);
}

hod::MapView effective_view(const hod::MapPosition* pos) {
    switch (g->map_view_choice) {
        case 1: return hod::MapView::CastleA;
        case 2: return hod::MapView::CastleB;
        case 3: return hod::MapView::Both;
        default: return pos && pos->castle_b ? hod::MapView::CastleB : hod::MapView::CastleA;
    }
}

void draw_map_tab() {
    const hod::MapSources src = sources();
    hod::MapPosition pos;
    const bool in_castle = hod::map_tables_available(src) && hod::player_map_position(src, &pos);
    const hod::MapView view = effective_view(in_castle ? &pos : nullptr);

    char right[32];
    const int tenths = in_castle ? hod::map_completion_tenths(src, view) : 0;
    std::snprintf(right, sizeof(right), "%d.%d%%", tenths / 10, tenths % 10);
    const Str title = view == hod::MapView::CastleB ? Str::CastleB
                    : view == hod::MapView::Both    ? Str::BothCastles
                                                    : Str::CastleA;
    draw_header(t(title), in_castle ? right : nullptr);

    g->canvas.fill(0, kContentY, kCanvasW, kContentH, map_color(0));
    if (!in_castle) {
        g->canvas.text(kCanvasW / 2, kContentY + kContentH / 2 - 4, t(Str::NotInCastle), kDim, Align::Center);
        return;
    }
    const std::uint32_t sig = hod::map_signature(src);
    if (!g->map_valid || sig != g->map_sig || view != g->drawn_view) {
        hod::render_map(src, view, g->map_px.data());
        g->map_sig = sig;
        g->drawn_view = view;
        g->map_valid = true;
    }

    // Zoom 1 shows the whole 256x160 castle; zoom 2 follows the player.
    const int zoom = g->cfg.map_zoom;
    int ox = 0, oy = 0;
    if (zoom == 2) {
        ox = std::clamp(pos.cell_x * 4 + 2 - kCanvasW / 4, 0, hod::kMapPixelsW - kCanvasW / 2);
        oy = std::clamp(pos.cell_y * 4 + 2 - kContentH / 4, 0, hod::kMapPixelsH - kContentH / 2);
    }
    std::uint32_t* dst = g->canvas.pixels();
    for (int y = 0; y < kContentH; ++y) {
        const int my = oy + y / zoom;
        for (int x = 0; x < kCanvasW; ++x) {
            const int mx = ox + x / zoom;
            dst[(kContentY + y) * kCanvasW + x] = map_color(g->map_px[my * hod::kMapPixelsW + mx]);
        }
    }
    // Blinking player marker, only on the map of the castle the player is in.
    const bool marker_castle = view == hod::MapView::Both ||
                               (view == hod::MapView::CastleB) == pos.castle_b;
    if (marker_castle && ((g->frame / 16) & 1) == 0) {
        const int mx = (pos.cell_x * 4 - ox) * zoom;
        const int my = kContentY + (pos.cell_y * 4 - oy) * zoom;
        g->canvas.fill(mx, my, 5 * zoom, 5 * zoom, rgb(255, 255, 255));
        g->canvas.fill(mx + zoom, my + zoom, 3 * zoom, 3 * zoom, rgb(230, 30, 140));
    }
}

void draw_bar(int x, int y, int w, int value, int max, Rgb color) {
    g->canvas.fill(x, y, w, 7, rgb(20, 20, 30));
    g->canvas.frame(x - 1, y - 1, w + 2, 9, kBorder);
    if (max > 0) {
        const int fill = std::clamp(value * w / max, 0, w);
        g->canvas.fill(x, y, fill, 7, color);
        g->canvas.fill(x, y, fill, 2, rgb(std::min(255, static_cast<int>((color >> 16) & 255) + 60),
                                          std::min(255, static_cast<int>((color >> 8) & 255) + 60),
                                          std::min(255, static_cast<int>(color & 255) + 60)));
    }
}

void draw_flags(int x, int y, unsigned bits, int count, Rgb on) {
    for (int i = 0; i < count; ++i) {
        const int bx = x + i * 10;
        g->canvas.frame(bx, y, 8, 8, kBorder);
        if (bits & (1u << i)) g->canvas.fill(bx + 1, y + 1, 6, 6, on);
    }
}

void draw_status_tab() {
    const hod::MapSources src = sources();
    const std::uint8_t* ew = src.ewram;
    auto r16 = [&](std::uint32_t a) { return ew ? hod::rd16(ew, a - hod::kEwramBase) : 0u; };
    auto r32 = [&](std::uint32_t a) { return ew ? hod::rd32(ew, a - hod::kEwramBase) : 0u; };
    auto r8 = [&](std::uint32_t a) { return ew ? hod::rd8(ew, a - hod::kEwramBase) : 0u; };
    const bool in_castle = ew && r8(hod::kGameState) == hod::kGameStateCastle;

    const std::uint32_t frames = r32(hod::kPlayFrames);
    const std::uint32_t secs = frames / 60;
    char clock[32];
    std::snprintf(clock, sizeof(clock), "%u:%02u:%02u", secs / 3600, (secs / 60) % 60, secs % 60);
    draw_header(t(Str::Status), in_castle ? clock : nullptr);
    g->canvas.panel(0, kContentY, kCanvasW, kContentH, kBgTop, kBgBottom, rgb(30, 36, 80));
    if (!in_castle) {
        g->canvas.text(kCanvasW / 2, kContentY + kContentH / 2 - 4, t(Str::NotInCastle), kDim, Align::Center);
        return;
    }

    char buf[64];
    int y = kContentY + 8;
    // HP / MP bars and hearts.
    const unsigned hp = r16(hod::kHp), max_hp = r16(hod::kMaxHp);
    const unsigned mp = r16(hod::kMp), max_mp = r16(hod::kMaxMp);
    g->canvas.text(8, y, "HP", kText);
    draw_bar(28, y, 140, static_cast<int>(hp), static_cast<int>(max_hp), kHpColor);
    std::snprintf(buf, sizeof(buf), "%u/%u", hp, max_hp);
    g->canvas.text(kCanvasW - 8, y, buf, kText, Align::Right);
    y += 14;
    g->canvas.text(8, y, "MP", kText);
    draw_bar(28, y, 140, static_cast<int>(mp), static_cast<int>(max_mp), kMpColor);
    std::snprintf(buf, sizeof(buf), "%u/%u", mp, max_mp);
    g->canvas.text(kCanvasW - 8, y, buf, kText, Align::Right);
    y += 14;
    std::snprintf(buf, sizeof(buf), "♥ %u/%u", r16(hod::kHearts), r16(hod::kMaxHearts));
    g->canvas.text(8, y, buf, kHeart);
    std::snprintf(buf, sizeof(buf), "%s %u", t(Str::Level), r8(hod::kLevel));
    g->canvas.text(kCanvasW - 8, y, buf, kGold, Align::Right);
    y += 16;

    g->canvas.fill(8, y - 4, kCanvasW - 16, 1, rgb(50, 60, 120));
    std::snprintf(buf, sizeof(buf), "%s %u", t(Str::Exp), r32(hod::kExp));
    g->canvas.text(8, y, buf, kText);
    std::snprintf(buf, sizeof(buf), "%s %u", t(Str::Gold), r32(hod::kGold));
    g->canvas.text(kCanvasW - 8, y, buf, kGold, Align::Right);
    y += 14;

    static const struct { const char* name; std::uint32_t addr; } stats[] = {
        {"STR", hod::kStr}, {"DEF", hod::kDef}, {"INT", hod::kInt}, {"LCK", hod::kLck}};
    for (int i = 0; i < 4; ++i) {
        const int col = i % 2, row = i / 2;
        const int sx = 8 + col * 124;
        const int sy = y + row * 12;
        g->canvas.text(sx, sy, stats[i].name, kDim);
        std::snprintf(buf, sizeof(buf), "%u", r16(stats[i].addr));
        g->canvas.text(sx + 100, sy, buf, kText, Align::Right);
    }
    y += 28;

    g->canvas.fill(8, y - 4, kCanvasW - 16, 1, rgb(50, 60, 120));
    const int a = hod::map_completion_tenths(src, hod::MapView::CastleA);
    const int b = hod::map_completion_tenths(src, hod::MapView::CastleB);
    std::snprintf(buf, sizeof(buf), "%s  A %d.%d%%  B %d.%d%%", t(Str::MapPercent), a / 10, a % 10, b / 10, b % 10);
    g->canvas.text(8, y, buf, kText);
    y += 14;
    g->canvas.text(8, y, t(Str::Books), kDim);
    draw_flags(84, y - 1, r8(hod::kSpellBooks), 5, kGold);
    y += 13;
    g->canvas.text(8, y, t(Str::Relics), kDim);
    draw_flags(84, y - 1, r16(hod::kRelics), 12, rgb(120, 200, 255));
}

struct SettingRow {
    Setting id;
    Str label;
};

constexpr SettingRow kSettingRows[] = {
    {Setting::TopScale, Str::TopScreen},   {Setting::Filter, Str::Filter},
    {Setting::Volume, Str::Volume},        {Setting::MenusBelow, Str::MenusBelow},
    {Setting::SwapAB, Str::SwapAB},        {Setting::Turbo, Str::Turbo},
    {Setting::ShowFps, Str::ShowFps},      {Setting::ResumeAfterSleep, Str::ResumeAfterSleep},
    {Setting::SaveState, Str::SaveState},  {Setting::LoadState, Str::LoadState},
    {Setting::Language, Str::Language},    {Setting::Quit, Str::Quit},
};
constexpr int kRowH = 13;
constexpr int kRowsY = kContentY + 2;

Rect setting_rect(int index) { return {2, kRowsY + index * kRowH, kCanvasW - 4, kRowH - 1}; }

void setting_value(Setting id, char* out, std::size_t n) {
    const PortConfig& c = g->cfg;
    auto yes_no = [&](bool v) { std::snprintf(out, n, "%s", t(v ? Str::Yes : Str::No)); };
    switch (id) {
        case Setting::TopScale: {
            const Str s = c.top_scale == TopScale::Fit ? Str::ScaleFit
                        : c.top_scale == TopScale::Stretch ? Str::ScaleStretch
                                                           : Str::ScaleInteger;
            std::snprintf(out, n, "%s", t(s));
            break;
        }
        case Setting::Filter: std::snprintf(out, n, "%s", t(c.smooth_filter ? Str::Smooth : Str::Sharp)); break;
        case Setting::Volume: std::snprintf(out, n, "%d%%", c.volume); break;
        case Setting::MenusBelow: yes_no(c.menus_on_bottom); break;
        case Setting::SwapAB: yes_no(c.swap_ab); break;
        case Setting::Turbo: yes_no(g->turbo); break;
        case Setting::ShowFps: yes_no(c.show_fps); break;
        case Setting::ResumeAfterSleep: yes_no(c.resume_after_sleep); break;
        case Setting::Language: std::snprintf(out, n, "%s", t(Str::LanguageName)); break;
        default: out[0] = 0; break;
    }
}

void draw_settings_tab() {
    draw_header(t(Str::TabSettings), nullptr);
    g->canvas.panel(0, kContentY, kCanvasW, kContentH, kBgTop, kBgBottom, rgb(30, 36, 80));
    const Uint32 now = SDL_GetTicks();
    for (int i = 0; i < static_cast<int>(Setting::Count); ++i) {
        const SettingRow& row = kSettingRows[i];
        const Rect r = setting_rect(i);
        const bool armed = g->armed == row.id && now < g->armed_until;
        if (i & 1) g->canvas.blend(r.x, r.y, r.w, r.h, rgb(60, 80, 170), 40);
        const bool action = row.id == Setting::SaveState || row.id == Setting::LoadState ||
                            row.id == Setting::Quit;
        if (armed) g->canvas.blend(r.x, r.y, r.w, r.h, rgb(200, 60, 60), 110);
        g->canvas.text(r.x + 4, r.y + 3, t(row.label), action ? kGold : kText);
        if (armed) {
            g->canvas.text(r.x + r.w - 4, r.y + 3, t(Str::TapAgain), kText, Align::Right);
        } else if (!action) {
            char value[32];
            setting_value(row.id, value, sizeof(value));
            g->canvas.text(196, r.y + 3, value, kGold, Align::Center);
            g->canvas.text(160, r.y + 3, "◀", kDim);
            g->canvas.text(r.x + r.w - 9, r.y + 3, "▶", kDim);
        }
    }
}

void draw_mirror_frame() {
    // The game's menu at 4x on the 1024x768 panel: canvas (8,16) 240x160.
    g->canvas.panel(0, 0, kCanvasW, kCanvasH, kBgTop, kBgBottom, kBorder);
    const Str title = g->mirror_mode == hod::kModeMap ? Str::GameMap : Str::PauseMenu;
    g->canvas.text(kCanvasW / 2, 4, t(title), kGold, Align::Center);
    g->canvas.text(kCanvasW / 2, kCanvasH - 12, t(Str::CloseHint), kDim, Align::Center);
    g->canvas.frame(7, 15, kGbaW + 2, kGbaH + 2, kBorder);
}

void redraw_ui() {
    if (g->mirroring) {
        draw_mirror_frame();
        return;
    }
    switch (g->tab) {
        case Tab::Map: draw_map_tab(); break;
        case Tab::Status: draw_status_tab(); break;
        case Tab::Settings: draw_settings_tab(); break;
    }
    draw_tabs();
}

// Cheap fingerprint of everything the visible tab shows.
std::uint32_t ui_fingerprint() {
    const hod::MapSources src = sources();
    std::uint32_t h = 2166136261u;
    auto mix = [&](std::uint32_t v) { h = (h ^ v) * 16777619u; };
    mix(static_cast<std::uint32_t>(g->tab));
    mix(static_cast<std::uint32_t>(g->cfg.language));
    mix(g->cfg.show_fps ? static_cast<std::uint32_t>(g->fps) : 0u);
    mix(SDL_GetTicks() < g->toast_until ? 1u : 0u);
    mix(g->armed != Setting::Count && SDL_GetTicks() < g->armed_until ? 1u : 0u);
    if (!src.ewram) return h;
    const std::uint8_t* ew = src.ewram;
    mix(hod::rd8(ew, hod::kGameState - hod::kEwramBase));
    if (g->tab == Tab::Map) {
        hod::MapPosition pos;
        if (hod::player_map_position(src, &pos))
            mix(static_cast<std::uint32_t>(pos.cell_x | pos.cell_y << 8 | (pos.castle_b ? 1 << 16 : 0)));
        mix(hod::map_signature(src));
        mix(static_cast<std::uint32_t>((g->frame / 16) & 1));
        mix(static_cast<std::uint32_t>(g->cfg.map_zoom | g->map_view_choice << 4));
    } else if (g->tab == Tab::Status) {
        for (std::uint32_t a = hod::kHp; a < hod::kHp + 4; ++a) mix(hod::rd8(ew, a - hod::kEwramBase));
        for (std::uint32_t a = hod::kLevel; a < hod::kRelics + 2; ++a) mix(hod::rd8(ew, a - hod::kEwramBase));
        mix(hod::rd32(ew, hod::kPlayFrames - hod::kEwramBase) / 60);
        mix(hod::map_signature(src));
    }
    return h;
}

void present_bottom(const std::uint8_t* gba_frame) {
    const auto t0 = std::chrono::steady_clock::now();
    if (g->needs_layout) update_layout();

    // Redraw the canvas only when something visible changed (UI at <= 30 Hz).
    const std::uint32_t fp = ui_fingerprint();
    const bool ui_changed = g->dirty || fp != g->status_hash;
    if (!g->mirroring && !ui_changed) return;
    if (!g->mirroring && (g->frame & 1)) return;

    if (ui_changed) {
        redraw_ui();
        SDL_UpdateTexture(g->ui_tex, nullptr, g->canvas.pixels(), kCanvasW * 4);
        g->status_hash = fp;
        g->dirty = false;
    }
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 255);
    SDL_RenderClear(g->renderer);
    SDL_RenderCopy(g->renderer, g->ui_tex, nullptr, &g->ui_dest);
    if (g->mirroring && gba_frame) {
        SDL_UpdateTexture(g->game_tex, nullptr, gba_frame, kGbaW * 3);
        const SDL_Rect dst{g->ui_dest.x + static_cast<int>(8 * g->ui_scale),
                           g->ui_dest.y + static_cast<int>(16 * g->ui_scale),
                           static_cast<int>(kGbaW * g->ui_scale), static_cast<int>(kGbaH * g->ui_scale)};
        SDL_RenderCopy(g->renderer, g->game_tex, nullptr, &dst);
    }
    SDL_RenderPresent(g->renderer);

    if (g->perf_log) {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        g->perf_bottom_ms += ms;
        g->perf_bottom_max_ms = std::max(g->perf_bottom_max_ms, ms);
        ++g->perf_samples;
    }
}

// ---- input --------------------------------------------------------------

void apply_setting(Setting id, int dir) {
    PortConfig& c = g->cfg;
    switch (id) {
        case Setting::TopScale: {
            // Cycle integer -> fit -> stretch.
            static const TopScale order[] = {TopScale::Integer, TopScale::Fit, TopScale::Stretch};
            int i = 0;
            while (order[i] != c.top_scale) ++i;
            c.top_scale = order[(i + (dir > 0 ? 1 : 2)) % 3];
            host_request(HostRequest::SetScaleMode, static_cast<int>(c.top_scale));
            break;
        }
        case Setting::Filter:
            c.smooth_filter = !c.smooth_filter;
            host_request(HostRequest::SetLinearFilter, c.smooth_filter ? 1 : 0);
            break;
        case Setting::Volume:
            c.volume = std::clamp(c.volume + dir * 10, 0, 100);
            host_request(HostRequest::SetVolume, c.volume);
            break;
        case Setting::MenusBelow: c.menus_on_bottom = !c.menus_on_bottom; break;
        case Setting::SwapAB:
            c.swap_ab = !c.swap_ab;
            host_request(HostRequest::SwapAB, c.swap_ab ? 1 : 0);
            break;
        case Setting::Turbo:
            g->turbo = !g->turbo;
            host_request(HostRequest::FastForward, g->turbo ? 1 : 0);
            return;  // not persisted
        case Setting::ShowFps: c.show_fps = !c.show_fps; break;
        case Setting::ResumeAfterSleep: c.resume_after_sleep = !c.resume_after_sleep; break;
        case Setting::Language:
            c.language = c.language == Language::Spanish ? Language::English : Language::Spanish;
            break;
        default: return;
    }
    save_config();
}

void run_action(Setting id) {
    const Uint32 now = SDL_GetTicks();
    if (g->armed != id || now >= g->armed_until) {
        g->armed = id;
        g->armed_until = now + 2500;
        return;
    }
    g->armed = Setting::Count;
    switch (id) {
        case Setting::SaveState:
            host_request(HostRequest::SaveState, 1);
            show_toast(t(Str::StateSaved));
            break;
        case Setting::LoadState:
            host_request(HostRequest::LoadState, 1);
            show_toast(t(Str::StateLoaded));
            break;
        case Setting::Quit:
            show_toast(t(Str::Saving));
            host_request(HostRequest::Quit);
            break;
        default: break;
    }
}

void handle_tap(int x, int y) {
    g->dirty = true;
    if (g->mirroring) return;  // the game's own menu is driven by the buttons
    if (y >= kTabBarY) {
        const int i = std::clamp(x / (kCanvasW / 3), 0, 2);
        g->tab = static_cast<Tab>(i);
        g->armed = Setting::Count;
        return;
    }
    switch (g->tab) {
        case Tab::Map:
            if (y < kHeaderH) g->map_view_choice = (g->map_view_choice + 1) % 4;
            else {
                g->cfg.map_zoom = g->cfg.map_zoom == 1 ? 2 : 1;
                save_config();
            }
            break;
        case Tab::Status: break;
        case Tab::Settings:
            for (int i = 0; i < static_cast<int>(Setting::Count); ++i) {
                if (!setting_rect(i).contains(x, y)) continue;
                const Setting id = kSettingRows[i].id;
                if (id == Setting::SaveState || id == Setting::LoadState || id == Setting::Quit) run_action(id);
                else {
                    g->armed = Setting::Count;
                    apply_setting(id, x < 196 ? -1 : 1);
                }
                break;
            }
            break;
    }
}

bool to_canvas(float wx, float wy, int* cx, int* cy) {
    if (g->ui_dest.w <= 0 || g->ui_dest.h <= 0) return false;
    *cx = static_cast<int>((wx - g->ui_dest.x) * kCanvasW / g->ui_dest.w);
    *cy = static_cast<int>((wy - g->ui_dest.y) * kCanvasH / g->ui_dest.h);
    return *cx >= 0 && *cy >= 0 && *cx < kCanvasW && *cy < kCanvasH;
}

void poll_exit_combo() {
    if (!g->pad) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i)
            if (SDL_IsGameController(i) && (g->pad = SDL_GameControllerOpen(i))) break;
        if (!g->pad) return;
    }
    if (!SDL_GameControllerGetAttached(g->pad)) {
        SDL_GameControllerClose(g->pad);
        g->pad = nullptr;
        return;
    }
    static bool fired = false;
    const bool combo = SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) &&
                       SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_START) &&
                       SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_BACK);
    if (combo && !fired) {
        std::printf("dual_screen: exit combo (L1+Start+Select)\n");
        std::fflush(stdout);
        host_request(HostRequest::Quit);
    }
    fired = combo;
}

void poll_sleep() {
    if (g->sleep_release_in > 0 && --g->sleep_release_in == 0) {
        std::printf("suspend: save written, letting the device sleep\n");
        std::fflush(stdout);
        sleep_ready();
    }
    switch (sleep_poll()) {
        case SleepEvent::GoingToSleep:
            std::printf("suspend: going to sleep\n");
            std::fflush(stdout);
            host_request(g->cfg.resume_after_sleep ? HostRequest::SuspendState : HostRequest::FlushSave);
            host_request(HostRequest::AudioEnabled, 0);
            // The runtime handles the request at its next pump, before the
            // next present: release the inhibitor on the present after that.
            g->sleep_release_in = 2;
            break;
        case SleepEvent::Resumed:
            std::printf("suspend: resumed\n");
            std::fflush(stdout);
            host_request(HostRequest::Resumed);
            host_request(HostRequest::AudioEnabled, 1);
            g->dirty = true;
            break;
        default: break;
    }
}

void update_fps() {
    const Uint32 now = SDL_GetTicks();
    if (!g->fps_window_start) g->fps_window_start = now;
    ++g->fps_frames;
    const Uint32 span = now - g->fps_window_start;
    if (span >= 1000) {
        g->fps = g->fps_frames * 1000.0f / static_cast<float>(span);
        if (g->perf_log) {
            static Uint32 last_log = 0;
            if (now - last_log >= 5000) {
                last_log = now;
                std::printf("perf: %.1f fps, bottom %.2f ms avg %.2f ms max\n", g->fps,
                            g->perf_samples ? g->perf_bottom_ms / g->perf_samples : 0.0, g->perf_bottom_max_ms);
                std::fflush(stdout);
                g->perf_bottom_ms = 0.0;
                g->perf_bottom_max_ms = 0.0;
                g->perf_samples = 0;
            }
        }
        g->fps_window_start = now;
        g->fps_frames = 0;
    }
}

// Darken the held gameplay frame and label it, so the top screen explains
// where the menu went.
void build_held_top() {
    const std::uint8_t* src = g->have_gameplay ? g->last_gameplay.data() : nullptr;
    for (int i = 0; i < kGbaW * kGbaH * 3; ++i) g->held_top[i] = src ? static_cast<std::uint8_t>(src[i] * 2 / 5) : 0;
    const Str title = g->mirror_mode == hod::kModeMap ? Str::GameMap : Str::PauseMenu;
    draw_text_rgb888(g->held_top.data(), kGbaW, kGbaH, kGbaW / 2, 62, t(title), rgb(232, 196, 96), 2);
    draw_text_rgb888(g->held_top.data(), kGbaW, kGbaH, kGbaW / 2, 84, t(Str::OnBottomScreen), rgb(236, 236, 244), 1);
}

// ---- development aids (environment variables, all off by default) --------
//   HOD_DEBUG_TAPS="600:40,180;900:120,100"  tap canvas x,y at a frame
//   HOD_DEBUG_DUMP_DIR=/tmp/out HOD_DEBUG_DUMP_EVERY=120
//       write top_<frame>.ppm / bottom_<frame>.ppm (bottom at canvas size)
struct DebugTap {
    std::uint64_t frame;
    int x, y;
};
std::vector<DebugTap> g_debug_taps;
std::string g_debug_dump_dir;
std::uint64_t g_debug_dump_every = 0;

void debug_init() {
    if (const char* taps = std::getenv("HOD_DEBUG_TAPS")) {
        const char* s = taps;
        while (*s) {
            unsigned long long f = 0;
            int x = 0, y = 0, used = 0;
            if (std::sscanf(s, "%llu:%d,%d%n", &f, &x, &y, &used) != 3) break;
            g_debug_taps.push_back({f, x, y});
            s += used;
            if (*s == ';') ++s;
        }
    }
    if (const char* dir = std::getenv("HOD_DEBUG_DUMP_DIR")) g_debug_dump_dir = dir;
    if (const char* every = std::getenv("HOD_DEBUG_DUMP_EVERY")) g_debug_dump_every = std::strtoull(every, nullptr, 10);
    // Uncapped speed for automated runs.
    if (const char* turbo = std::getenv("HOD_DEBUG_TURBO"); turbo && turbo[0] == '1')
        host_request(HostRequest::FastForward, 1);
}

void write_ppm(const std::string& path, int w, int h, auto pixel) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6 %d %d 255\n", w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Rgb c = pixel(x, y);
            const unsigned char px[3] = {static_cast<unsigned char>(c >> 16), static_cast<unsigned char>(c >> 8),
                                         static_cast<unsigned char>(c)};
            std::fwrite(px, 1, 3, f);
        }
    std::fclose(f);
}

void debug_frame(const std::uint8_t* gba_frame, const std::uint8_t* top) {
    for (const DebugTap& tap : g_debug_taps)
        if (tap.frame == g->frame) handle_tap(tap.x, tap.y);
    if (g_debug_dump_dir.empty() || !g_debug_dump_every || g->frame % g_debug_dump_every) return;
    const std::string n = std::to_string(g->frame);
    auto rgb_at = [](const std::uint8_t* p, int x, int y) {
        const std::uint8_t* q = p + (y * kGbaW + x) * 3;
        return rgb(q[0], q[1], q[2]);
    };
    write_ppm(g_debug_dump_dir + "/top_" + n + ".ppm", kGbaW, kGbaH, [&](int x, int y) { return rgb_at(top, x, y); });
    // The canvas as last drawn, with the mirrored game menu pasted where the
    // GPU draws it.
    redraw_ui();
    write_ppm(g_debug_dump_dir + "/bottom_" + n + ".ppm", kCanvasW, kCanvasH, [&](int x, int y) {
        if (g->mirroring && x >= 8 && y >= 16 && x < 8 + kGbaW && y < 16 + kGbaH) return rgb_at(gba_frame, x - 8, y - 16);
        return g->canvas.pixels()[y * kCanvasW + x];
    });
}

}  // namespace

void dual_screen_init(const DualScreenOptions& options, const PortConfig& config) {
    g = new State();
    g->opts = options;
    g->cfg = config;
    g->failed = !options.enabled;
    g->perf_log = std::getenv("HOD_PERF_LOG") && std::getenv("HOD_PERF_LOG")[0] == '1';
    debug_init();
    sleep_init();
    // The runtime reads the controller itself; these only restore the saved state.
    host_request(HostRequest::SwapAB, config.swap_ab ? 1 : 0);
    host_request(HostRequest::SetLinearFilter, config.smooth_filter ? 1 : 0);
}

const std::uint8_t* dual_screen_present(const std::uint8_t* rgb888, int width, int height) {
    if (!g) return rgb888;
    ++g->frame;
    update_fps();
    poll_exit_combo();
    poll_sleep();

    // Which image goes where this frame.
    const hod::MapSources src = sources();
    std::uint8_t mode = 0;
    if (src.ewram && hod::rd8(src.ewram, hod::kGameState - hod::kEwramBase) == hod::kGameStateCastle)
        mode = hod::rd8(src.ewram, hod::kCastleMode - hod::kEwramBase);
    const bool menu = mode == hod::kModePauseMenu || mode == hod::kModeMap;
    const bool can_mirror = width == kGbaW && height == kGbaH && !g->failed && g->window;
    const bool mirror = can_mirror && g->cfg.menus_on_bottom && menu;
    if (mirror != g->mirroring || (mirror && mode != g->mirror_mode)) {
        g->mirroring = mirror;
        g->mirror_mode = mode;
        g->dirty = true;
        if (mirror) build_held_top();
    }
    if (!mirror && width == kGbaW && height == kGbaH) {
        std::memcpy(g->last_gameplay.data(), rgb888, g->last_gameplay.size());
        g->have_gameplay = true;
    }

    // The bottom window opens once the game window has shown a few frames:
    // creating a second fullscreen window during the first GL frames can make
    // the compositor resize the game window under its renderer.
    if (!g->window && !g->failed && g->frame >= 3) {
        if (!create_window()) {
            g->failed = true;
            std::fprintf(stderr, "dual_screen: running without the bottom screen\n");
        }
    }
    if (g->window) present_bottom(rgb888);

    const std::uint8_t* top = g->mirroring ? g->held_top.data() : rgb888;
    if (width == kGbaW && height == kGbaH) debug_frame(rgb888, top);
    return top;
}

int dual_screen_event(const void* sdl_event) {
    if (!g || !g->window) return 0;
    const auto* e = static_cast<const SDL_Event*>(sdl_event);
    switch (e->type) {
        case SDL_FINGERDOWN:
            if (e->tfinger.windowID != g->window_id) return 0;
            {
                int cx, cy;
                if (to_canvas(e->tfinger.x * static_cast<float>(g->out_w),
                              e->tfinger.y * static_cast<float>(g->out_h), &cx, &cy))
                    handle_tap(cx, cy);
            }
            return 1;
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            return e->tfinger.windowID == g->window_id ? 1 : 0;
        case SDL_MOUSEBUTTONDOWN:
            if (e->button.windowID != g->window_id) return 0;
            if (e->button.which != SDL_TOUCH_MOUSEID) {  // a real mouse (development)
                int cx, cy;
                int ww = 0, wh = 0;
                SDL_GetWindowSize(g->window, &ww, &wh);
                const float sx = ww > 0 ? static_cast<float>(g->out_w) / ww : 1.0f;
                const float sy = wh > 0 ? static_cast<float>(g->out_h) / wh : 1.0f;
                if (to_canvas(e->button.x * sx, e->button.y * sy, &cx, &cy)) handle_tap(cx, cy);
            }
            return 1;
        case SDL_MOUSEBUTTONUP:
            return e->button.windowID == g->window_id ? 1 : 0;
        case SDL_MOUSEMOTION:
            return e->motion.windowID == g->window_id ? 1 : 0;
        case SDL_MOUSEWHEEL:
            return e->wheel.windowID == g->window_id ? 1 : 0;
        case SDL_WINDOWEVENT:
            if (e->window.windowID != g->window_id) return 0;
            if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e->window.event == SDL_WINDOWEVENT_EXPOSED)
                g->needs_layout = true;
            // Closing the bottom window must not quit the game.
            return 1;
        default:
            return 0;
    }
}

void dual_screen_shutdown() {
    if (!g) return;
    sleep_shutdown();
    if (g->pad) SDL_GameControllerClose(g->pad);
    if (g->ui_tex) SDL_DestroyTexture(g->ui_tex);
    if (g->game_tex) SDL_DestroyTexture(g->game_tex);
    if (g->renderer) SDL_DestroyRenderer(g->renderer);
    if (g->window) SDL_DestroyWindow(g->window);
    delete g;
    g = nullptr;
}

}  // namespace port
