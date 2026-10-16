// Castle map for the bottom screen, decoded at runtime from the player's own
// ROM and the live save RAM — the same tables and drawing rules the game's map
// screen uses (sub_0800A508 and helpers), so explored rooms, doors, save/warp
// rooms and per-castle colouring match what the game itself shows.
//
// Nothing ROM-derived is stored in this repository: the tables are read from
// the cartridge image at run time.
#pragma once

#include <cstdint>

namespace hod {

// Castle map grid: 64 x 40 cells, each cell drawn as 4 x 4 pixels.
constexpr int kMapCellsW = 64;
constexpr int kMapCellsH = 40;
constexpr int kMapPixelsW = kMapCellsW * 4;  // 256
constexpr int kMapPixelsH = kMapCellsH * 4;  // 160

// Palette indices produced by the renderer (the game's map palette slots).
enum MapColor : std::uint8_t {
    kMapEmpty = 0,
    kMapWall = 1,
    kMapSaveRoom = 2,      // cell flag 0x8000
    kMapWarpRoom = 3,      // cell flags 0x4000 (castle A) / 0x2000 (castle B)
    kMapRevealed = 4,      // shown by a map item, not explored yet
    kMapRevealedWall = 5,
    kMapExplored = 8,      // generic explored cell
    kMapCastleA = 9,
    kMapCastleB = 10,
    kMapBothCastles = 11,
};

enum class MapView : std::uint8_t {
    Any = 0,      // gray cells, either castle
    CastleA = 1,  // blue
    Both = 2,     // overlay: blue A, green B, cyan both
    CastleB = 3,  // green
};

// Read-only views of guest memory needed by the map.
struct MapSources {
    const std::uint8_t* rom = nullptr;    // 0x08000000, size >= 0x00800000
    const std::uint8_t* ewram = nullptr;  // 0x02000000, 256 KiB
};

// True when the ROM looks like the USA release the tables were found in.
bool map_tables_available(const MapSources& src);

// Renders the map into `out` (kMapPixelsW * kMapPixelsH palette indices).
void render_map(const MapSources& src, MapView view, std::uint8_t* out);

// Explored cells counted the way the map screen's percentage does, in tenths
// of a percent (the game divides by 674 rooms).
int map_completion_tenths(const MapSources& src, MapView view);

// Explored-cell signature, cheap to compare between frames to know when the
// map must be redrawn.
std::uint32_t map_signature(const MapSources& src);

// The player's current cell. Returns false outside the castle (title, intro).
struct MapPosition {
    int cell_x = 0;
    int cell_y = 0;
    bool castle_b = false;
};
bool player_map_position(const MapSources& src, MapPosition* pos);

}  // namespace hod
