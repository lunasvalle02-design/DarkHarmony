// Castlevania: Harmony of Dissonance (USA, ACHE) — guest memory layout used by
// the dual-screen port. Every address below was confirmed against the running
// game (tools/research/hod_probe.c) or read from the code that uses it (the
// BeautifulHarmony decompilation); the comment names the evidence.
#pragma once

#include <cstdint>
#include <cstring>

namespace hod {

constexpr std::uint32_t kEwramBase = 0x02000000;
constexpr std::uint32_t kRomBase = 0x08000000;
constexpr std::uint32_t kRomSize = 0x00800000;

// ---- GameWork (EWRAM 0x02000000, pointer at ROM 0x08494580) ---------------
// Top-level state (sub_08000518): 12 boot, 0 logo, 1 title, 2 file select,
// 10 intro, 3 = in the castle (gameplay, menus and map all live here).
constexpr std::uint32_t kGameState = 0x0200000C;
constexpr std::uint8_t kGameStateCastle = 3;

// Map/menu work area at GameWork+0x48 (sub_0800A508 receives this pointer).
constexpr std::uint32_t kMapWork = 0x02000048;
// +0x00 in-castle mode: 1 playing, 4 map screen, 6 pause menu (probed).
constexpr std::uint32_t kCastleMode = kMapWork + 0x00;
constexpr std::uint8_t kModePlaying = 1;
constexpr std::uint8_t kModeMap = 4;
constexpr std::uint8_t kModePauseMenu = 6;
// +0x28: bit 0 castle (0 A, 1 B), bits 1-7 room origin X; +0x29 bits 0-5 Y.
constexpr std::uint32_t kRoomPos = kMapWork + 0x28;
// +0x40: play time in frames (pause menu clock).
constexpr std::uint32_t kPlayFrames = kMapWork + 0x40;
// +0x44: exploration bits. For x in [0,32) and [32,64): 40 rows of
// {u32 castle A, u32 castle B}, bit = x & 31 (sub_0800AD6C).
constexpr std::uint32_t kExplored = kMapWork + 0x44;
// +0x2C5 bits 6-7: map view the map screen opens with (MapView).
constexpr std::uint32_t kMapViewByte = kMapWork + 0x2C5;
// Event flags used by doors that open on the map (sub_0800B0F8).
constexpr std::uint32_t kMapDoorFlags = 0x02000310;
// Bit 19: an extra block of revealed cells (sub_0800A508).
constexpr std::uint32_t kMapSpecialFlags = 0x0200031C;
// Map items: non-zero byte = that region of the map is revealed.
constexpr std::uint32_t kMapRevealFlags = 0x020187AC;

// Player entity pointer and BG layer 1 camera (sub_08001964/84).
constexpr std::uint32_t kPlayerEntityPtr = 0x0201848C;
constexpr std::uint32_t kCameraXInt = 0x0200A446;
constexpr std::uint32_t kCameraYInt = 0x0200A44A;
constexpr std::uint32_t kEntityXInt = 0x4A;
constexpr std::uint32_t kEntityYInt = 0x4E;

// ---- Player status (Data Crystal RAM map, probed on a new game) -----------
constexpr std::uint32_t kHp = 0x0201854E;       // u16
constexpr std::uint32_t kMp = 0x02018550;       // u16
constexpr std::uint32_t kLevel = 0x0201877C;    // u8
constexpr std::uint32_t kMaxHp = 0x02018786;    // u16
constexpr std::uint32_t kMaxMp = 0x02018788;    // u16
constexpr std::uint32_t kMaxHearts = 0x0201878A;
constexpr std::uint32_t kStr = 0x0201878C;
constexpr std::uint32_t kDef = 0x0201878E;
constexpr std::uint32_t kInt = 0x02018790;
constexpr std::uint32_t kLck = 0x02018792;
constexpr std::uint32_t kHearts = 0x02018794;   // u16
constexpr std::uint32_t kExp = 0x02018798;      // u32
constexpr std::uint32_t kGold = 0x0201879C;     // u32
constexpr std::uint32_t kSpellBooks = 0x0201883E;  // u8 bit flags (5 books)
constexpr std::uint32_t kRelics = 0x0201883F;      // u16 bit flags (12 relics)

// ---- ROM tables (USA) ----------------------------------------------------
constexpr std::uint32_t kRomMapCells = 0x080DAD94;  // u16[40][64]
constexpr std::uint32_t kRomMapWalls = 0x080DC194;  // u8[40][32], nibble/cell
constexpr std::uint32_t kRomMapDoors = 0x080DC694;  // s8 {x, y} pairs, x < 0 ends
constexpr std::uint32_t kRomGameCode = 0x080000AC;  // "ACHE"

// Little-endian reads from host copies of guest memory.
inline std::uint8_t rd8(const std::uint8_t* base, std::uint32_t off) { return base[off]; }
inline std::uint16_t rd16(const std::uint8_t* base, std::uint32_t off) {
    return static_cast<std::uint16_t>(base[off] | (base[off + 1] << 8));
}
inline std::uint32_t rd32(const std::uint8_t* base, std::uint32_t off) {
    return static_cast<std::uint32_t>(base[off]) |
           (static_cast<std::uint32_t>(base[off + 1]) << 8) |
           (static_cast<std::uint32_t>(base[off + 2]) << 16) |
           (static_cast<std::uint32_t>(base[off + 3]) << 24);
}

}  // namespace hod
