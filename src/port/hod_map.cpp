// Castle map renderer — a C++ transcription of the game's own map drawing
// (sub_0800A508 -> sub_0800A3F4 -> sub_0800A1FC / sub_0800A2E4), producing the
// same 4bpp palette indices into a host buffer instead of VRAM tiles.
#include "hod_map.h"

#include "hod_ram.h"

#include <cstring>

namespace hod {
namespace {

constexpr std::uint32_t ew(std::uint32_t addr) { return addr - kEwramBase; }
constexpr std::uint32_t ro(std::uint32_t addr) { return addr - kRomBase; }

class MapPainter {
public:
    MapPainter(const MapSources& src, std::uint8_t* out) : src_(src), px_(out) {}

    std::uint16_t cell(int x, int y) const {
        return rd16(src_.rom, ro(kRomMapCells) + static_cast<std::uint32_t>((y * kMapCellsW + x) * 2));
    }

    // Wall nibble of a cell: high nibble for even x, low for odd x. Bits 2-3
    // are the top edge, bits 0-1 the left edge.
    int walls(int x, int y) const {
        if (x < 0 || y < 0 || x >= kMapCellsW || y >= kMapCellsH) return 0;
        const std::uint8_t b = rd8(src_.rom, ro(kRomMapWalls) + static_cast<std::uint32_t>(y * 32 + (x >> 1)));
        return (x & 1) ? (b & 0xF) : (b >> 4);
    }

    // Doors listed in the ROM open (2) or stay closed (3) with an event flag.
    int dynamic_edge(int x, int y) const {
        int index = 0;
        for (std::uint32_t off = ro(kRomMapDoors);; off += 2, ++index) {
            const auto dx = static_cast<std::int8_t>(rd8(src_.rom, off));
            if (dx < 0) { index = -1; break; }
            const auto dy = static_cast<std::int8_t>(rd8(src_.rom, off + 1));
            if (dx == x && dy == y) break;
        }
        const std::uint32_t word = rd32(src_.ewram,
            ew(kMapDoorFlags) + static_cast<std::uint32_t>((index >> 5) * 4));
        return (word >> (index & 31)) & 1u ? 2 : 3;
    }

    std::uint8_t get(int x, int y) const {
        if (x < 0 || y < 0 || x >= kMapPixelsW || y >= kMapPixelsH) return 0;
        return px_[y * kMapPixelsW + x];
    }
    void put(int x, int y, std::uint8_t c) {
        if (x < 0 || y < 0 || x >= kMapPixelsW || y >= kMapPixelsH) return;
        px_[y * kMapPixelsW + x] = c;
    }

    // sub_0800A1FC: the top edge (row 0) of cell (x, y).
    void top_edge(int x, int y, std::uint8_t color, std::uint8_t wall, int nibble) {
        int type = nibble >> 2;
        if (type == 1) type = dynamic_edge(x, y);
        const int px = x * 4, py = y * 4;
        switch (type) {
            case 0: put(px + 1, py, color); put(px + 2, py, color); put(px + 3, py, color); break;
            case 2: put(px + 1, py, wall); put(px + 2, py, color); put(px + 3, py, wall); break;
            case 3: put(px + 1, py, wall); put(px + 2, py, wall); put(px + 3, py, wall); break;
            default: break;
        }
        if (get(px, py) == wall || (type & 2)) put(px, py, wall);
        if (type & 2) put(px + 4, py, wall);  // corner shared with the cell to the right
    }

    // sub_0800A2E4: the left edge (column 0) of cell (x, y) and the corner
    // below it.
    void left_edge(int x, int y, std::uint8_t color, std::uint8_t wall, int nibble) {
        int type = nibble & 3;
        if (type == 1) type = dynamic_edge(x, -y);
        const bool solid = (type & 2) != 0;
        const int px = x * 4, py = y * 4;
        for (int row = 0; row < 4; ++row) {
            const std::uint8_t existing = get(px, py + row);
            std::uint8_t c = color;
            if (row == 0) c = (solid || existing == kMapRevealedWall) ? wall : color;
            else if (row == 2) c = type == 3 ? wall : color;
            else c = solid ? wall : color;
            if (existing != wall) put(px, py + row, c);
        }
        const std::uint8_t below = get(px, py + 4);
        std::uint8_t c = color;
        if (below == 0) c = solid ? wall : color;
        else if (below == kMapRevealedWall || below == kMapWall) c = wall;
        put(px, py + 4, c);
    }

    // sub_0800A3F4: one cell with its four edges.
    void draw_cell(int x, int y, std::uint8_t color) {
        const std::uint8_t wall = color == kMapRevealed ? kMapRevealedWall : kMapWall;
        const int px = x * 4, py = y * 4;
        for (int row = 1; row < 4; ++row)
            for (int col = 0; col < 4; ++col) put(px + col, py + row, color);
        top_edge(x, y, color, wall, walls(x, y));
        left_edge(x, y, color, wall, walls(x, y));
        top_edge(x, y + 1, color, wall, walls(x, y + 1));
        left_edge(x + 1, y, color, wall, walls(x + 1, y));
    }

private:
    const MapSources& src_;
    std::uint8_t* px_;
};

bool explored(const MapSources& src, int x, int y, bool castle_b) {
    const std::uint32_t off = ew(kExplored) + static_cast<std::uint32_t>((x >> 5) * 320 + y * 8 + (castle_b ? 4 : 0));
    return (rd32(src.ewram, off) >> (x & 31)) & 1u;
}

}  // namespace

bool map_tables_available(const MapSources& src) {
    return src.rom && src.ewram && std::memcmp(src.rom + ro(kRomGameCode), "ACHE", 4) == 0;
}

void render_map(const MapSources& src, MapView view, std::uint8_t* out) {
    std::memset(out, 0, kMapPixelsW * kMapPixelsH);
    if (!map_tables_available(src)) return;
    MapPainter p(src, out);

    // Regions revealed by map items, drawn first so explored cells overwrite them.
    for (int y = 0; y < kMapCellsH; ++y) {
        for (int x = 0; x < kMapCellsW; ++x) {
            const std::uint16_t v = p.cell(x, y);
            if (v == 0xFFFF || (v & 0xC0) == 0) continue;
            if (rd8(src.ewram, ew(kMapRevealFlags) + ((v & 0xC0) >> 6)) == 0) continue;
            p.draw_cell(x, y, kMapRevealed);
        }
    }
    if (rd32(src.ewram, ew(kMapSpecialFlags)) & 0x80000u) {
        p.draw_cell(31, 22, kMapRevealed);
        p.draw_cell(31, 23, kMapRevealed);
        for (int y = 24; y <= 26; ++y)
            for (int x = 30; x <= 32; ++x) p.draw_cell(x, y, kMapRevealed);
    }

    for (int y = 0; y < kMapCellsH; ++y) {
        for (int x = 0; x < kMapCellsW; ++x) {
            const std::uint16_t v = p.cell(x, y);
            const bool a = explored(src, x, y, false);
            const bool b = explored(src, x, y, true);
            std::uint8_t color = 0;
            switch (view) {
                case MapView::CastleA:
                    if (!a) continue;
                    color = (v & 0x4000) ? kMapWarpRoom : kMapCastleA;
                    break;
                case MapView::CastleB:
                    if (!b) continue;
                    color = (v & 0x2000) ? kMapWarpRoom : kMapCastleB;
                    break;
                case MapView::Both:
                    color = static_cast<std::uint8_t>((a ? kMapCastleA : 0) | (b ? kMapCastleB : 0));
                    if (!color) continue;
                    if ((v & 0x6000) == 0x6000) color = kMapWarpRoom;
                    break;
                case MapView::Any:
                default:
                    if (!a && !b) continue;
                    color = (v & 0x6000) ? kMapWarpRoom : kMapExplored;
                    break;
            }
            if (v & 0x8000) color = kMapSaveRoom;
            p.draw_cell(x, y, color);
        }
    }
}

int map_completion_tenths(const MapSources& src, MapView view) {
    if (!map_tables_available(src)) return 0;
    int count = 0;
    for (int y = 0; y < kMapCellsH; ++y) {
        for (int x = 0; x < kMapCellsW; ++x) {
            if (view != MapView::CastleB && explored(src, x, y, false)) ++count;
            if (view != MapView::CastleA && explored(src, x, y, true)) ++count;
        }
    }
    return count * 1000 / 674;
}

std::uint32_t map_signature(const MapSources& src) {
    // FNV-1a over the exploration bits, door flags, map items and the special
    // block flag.
    std::uint32_t h = 2166136261u;
    auto mix = [&](std::uint32_t from, std::uint32_t len) {
        for (std::uint32_t i = 0; i < len; ++i) {
            h ^= src.ewram[ew(from) + i];
            h *= 16777619u;
        }
    };
    mix(kExplored, 640);
    mix(kMapDoorFlags, 12);
    mix(kMapSpecialFlags, 4);
    mix(kMapRevealFlags, 4);
    return h;
}

bool player_map_position(const MapSources& src, MapPosition* pos) {
    if (!src.ewram || rd8(src.ewram, ew(kGameState)) != kGameStateCastle) return false;
    const std::uint8_t r0 = rd8(src.ewram, ew(kRoomPos));
    const std::uint8_t r1 = rd8(src.ewram, ew(kRoomPos) + 1);
    int x = r0 >> 1;
    int y = r1 & 0x3F;
    const std::uint32_t entity = rd32(src.ewram, ew(kPlayerEntityPtr));
    // sub_0800AD6C: room origin plus the player's map position in 256 px steps
    // (except for the one room the game special-cases).
    if (!(x == 31 && y == 25) && entity >= kEwramBase && entity < kEwramBase + 0x40000 - 0x80) {
        const auto px = static_cast<std::int16_t>(
            rd16(src.ewram, ew(kCameraXInt)) + rd16(src.ewram, ew(entity) + kEntityXInt));
        const auto py = static_cast<std::int16_t>(
            rd16(src.ewram, ew(kCameraYInt)) + rd16(src.ewram, ew(entity) + kEntityYInt));
        x += static_cast<std::int8_t>(static_cast<std::uint16_t>(px) >> 8);
        y += static_cast<std::int8_t>(static_cast<std::uint16_t>(py) >> 8);
    }
    if (x < 0 || y < 0 || x >= kMapCellsW || y >= kMapCellsH) return false;
    pos->cell_x = x;
    pos->cell_y = y;
    pos->castle_b = (r0 & 1) != 0;
    return true;
}

}  // namespace hod
