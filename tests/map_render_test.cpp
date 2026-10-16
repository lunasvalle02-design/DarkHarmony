// Host test for the castle map renderer: renders the map from a ROM and an
// EWRAM dump (tools/research/hod_probe.c `dump`) into a PPM, so it can be
// compared with the game's own map screen.
//
//   map_render_test <rom.gba> <dump.ewram> <out.ppm> [view 0..3]
#include "hod_map.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static std::vector<std::uint8_t> load(const char* path) {
    std::vector<std::uint8_t> v;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END);
        v.resize(static_cast<std::size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
        std::fclose(f);
    }
    return v;
}

int main(int argc, char** argv) {
    if (argc < 4) return 2;
    const auto rom = load(argv[1]);
    const auto ewram = load(argv[2]);
    if (rom.size() < 0x800000 || ewram.size() < 0x40000) {
        std::fprintf(stderr, "bad inputs\n");
        return 1;
    }
    const auto view = static_cast<hod::MapView>(argc > 4 ? std::atoi(argv[4]) : 0);
    hod::MapSources src{rom.data(), ewram.data()};
    std::vector<std::uint8_t> px(hod::kMapPixelsW * hod::kMapPixelsH);
    hod::render_map(src, view, px.data());
    static const unsigned pal[16] = {0x000000, 0xFFFFFF, 0xFF0000, 0xFFFF00, 0x414141, 0x8B8B8B,
                                     0x830000, 0x838300, 0x838383, 0x0000E6, 0x00E600, 0x00E6E6};
    hod::MapPosition pos;
    const bool have_pos = hod::player_map_position(src, &pos);
    std::printf("completion=%d.%d%% pos=%s %d,%d castle=%c sig=%08x\n",
                hod::map_completion_tenths(src, view) / 10, hod::map_completion_tenths(src, view) % 10,
                have_pos ? "yes" : "no", pos.cell_x, pos.cell_y, pos.castle_b ? 'B' : 'A',
                hod::map_signature(src));
    FILE* f = std::fopen(argv[3], "wb");
    std::fprintf(f, "P6 %d %d 255\n", hod::kMapPixelsW, hod::kMapPixelsH);
    for (int i = 0; i < hod::kMapPixelsW * hod::kMapPixelsH; ++i) {
        unsigned c = pal[px[i] & 15];
        if (have_pos && i / hod::kMapPixelsW / 4 == pos.cell_y && i % hod::kMapPixelsW / 4 == pos.cell_x)
            c = 0xFF00FF;
        const unsigned char rgb[3] = {static_cast<unsigned char>(c >> 16),
                                      static_cast<unsigned char>(c >> 8),
                                      static_cast<unsigned char>(c)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return 0;
}
