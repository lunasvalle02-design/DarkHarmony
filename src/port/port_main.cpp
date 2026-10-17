// Castlevania: Harmony of Dissonance (USA, ACHE) — ROCKNIX / PortMaster entry
// point for dual-screen handhelds (Anbernic RG DS / RG DS Plus).
//
// No launcher: the ROM is looked up next to the executable (the BIOS is the
// port BIOS shipped with it, or your dump in a retail-BIOS build), the game
// runs fullscreen on the primary display and the bottom screen is driven by
// dual_screen.cpp. Settings live in hod.cfg (edited from the bottom screen).
//
//   hod.aarch64 [--rom <file.gba>] [--bios <gba_bios.bin>] [--single-screen]
#include "runtime.h"

#include "dual_screen.h"
#include "port_config.h"
#include "sha1.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sys/resource.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr const char* kRomSha1 = "b90da0d9be0b3a0893cd9e2c399056bcf9579e21";
constexpr std::uintmax_t kRomSize = 8388608;
constexpr std::uintmax_t kBiosSize = 16384;

fs::path executable_dir(const char* argv0) {
    std::error_code ec;
    fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (ec) self = fs::absolute(argv0, ec);
    return self.parent_path();
}

std::string sha1_of_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return gba::sha1(bytes.data(), bytes.size()).hex();
}

bool is_our_rom(const fs::path& path) {
    std::error_code ec;
    if (fs::file_size(path, ec) != kRomSize || ec) return false;
    std::ifstream in(path, std::ios::binary);
    char code[4] = {};
    in.seekg(0xAC);
    in.read(code, 4);
    return in && std::memcmp(code, "ACHE", 4) == 0;
}

// Any 8 MiB "ACHE" .gba in the port folder (or roms/) is a candidate; the
// SHA-1 decides.
fs::path find_rom(const fs::path& dir) {
    for (const fs::path& d : {dir, dir / "roms", dir / "rom"}) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(d, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext == ".gba" && is_our_rom(entry.path())) return entry.path();
        }
    }
    return {};
}

#if !HOD_PORT_BIOS
fs::path find_bios(const fs::path& dir) {
    for (const fs::path& p : {dir / "gba_bios.bin", dir / "bios" / "gba_bios.bin", dir / "bios.bin",
                              dir / "gba_bios.BIN"}) {
        std::error_code ec;
        if (fs::is_regular_file(p, ec)) return p;
    }
    return {};
}
#endif

// Guest call chains run on the host stack; the desktop build reserves 64 MiB.
// On Linux the main thread's stack follows RLIMIT_STACK at exec time, so raise
// the limit and re-exec once when it is lower.
void ensure_stack(char** argv) {
    constexpr rlim_t kWanted = 64ull * 1024 * 1024;
    rlimit rl{};
    if (getrlimit(RLIMIT_STACK, &rl) != 0) return;
    if (rl.rlim_cur == RLIM_INFINITY || rl.rlim_cur >= kWanted) return;
    if (std::getenv("HOD_STACK_REEXEC")) return;  // already tried
    const rlim_t target = (rl.rlim_max == RLIM_INFINITY || rl.rlim_max >= kWanted) ? kWanted : rl.rlim_max;
    if (target <= rl.rlim_cur) return;
    rl.rlim_cur = target;
    if (setrlimit(RLIMIT_STACK, &rl) != 0) return;
    setenv("HOD_STACK_REEXEC", "1", 1);
    execv("/proc/self/exe", argv);
}

void quiet_runtime_diagnostics() {
    setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 0);
    setenv("GBARECOMP_MISS_FRAG", "/dev/null", 0);
    setenv("GBARECOMP_COVERAGE_JSON", "/dev/null", 0);
    setenv("GBARECOMP_SESSION_DIAGNOSTICS", "0", 0);
    setenv("GBARECOMP_PRESENT_IN_PLACE", "0", 0);
    setenv("GBARECOMP_HANG_WATCHDOG", "0", 0);
    // Compose scanlines on a second core (patch 0003); 0 renders inline.
    setenv("GBARECOMP_PPU_THREAD", "1", 0);
    // Touch on the bottom screen must not turn into mouse input for the game
    // window, and the fullscreen game must not minimize when the bottom
    // window takes focus.
    setenv("SDL_TOUCH_MOUSE_EVENTS", "0", 0);
    setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0", 0);
}

}  // namespace

int main(int argc, char** argv) {
    ensure_stack(argv);
    quiet_runtime_diagnostics();
    const fs::path dir = executable_dir(argc > 0 ? argv[0] : "hod");
    std::error_code ec;
    fs::current_path(dir, ec);

    fs::path rom, bios;
    bool single_screen = std::getenv("HOD_SINGLE_SCREEN") && std::getenv("HOD_SINGLE_SCREEN")[0] == '1';
    std::vector<std::string> passthrough;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--rom" && i + 1 < argc) rom = argv[++i];
        else if (a == "--bios" && i + 1 < argc) bios = argv[++i];
        else if (a == "--single-screen") single_screen = true;
        else passthrough.push_back(a);
    }

    if (rom.empty()) rom = find_rom(dir);
    if (rom.empty()) {
        std::fprintf(stderr,
                     "[hod] ROM not found. Copy your Castlevania - Harmony of Dissonance (USA) .gba "
                     "(8 MiB, game code ACHE) into %s\n", dir.c_str());
        return 1;
    }
    const std::string rom_sha1 = sha1_of_file(rom);
    if (rom_sha1 != kRomSha1) {
        std::fprintf(stderr, "[hod] %s is not the supported USA ROM (SHA-1 %s, expected %s)\n",
                     rom.c_str(), rom_sha1.c_str(), kRomSha1);
        return 1;
    }
#if HOD_PORT_BIOS
    // The BIOS code linked into this binary is the port BIOS
    // (rocknix/bios/port_bios.s): map the matching image, not a retail dump.
    if (bios.empty()) bios = dir / "port_bios.bin";
#else
    if (bios.empty()) bios = find_bios(dir);
#endif
    if (bios.empty() || fs::file_size(bios, ec) != kBiosSize) {
#if HOD_PORT_BIOS
        std::fprintf(stderr, "[hod] port_bios.bin is missing from %s (reinstall the port)\n", dir.c_str());
#else
        std::fprintf(stderr, "[hod] GBA BIOS not found: copy your 16 KiB gba_bios.bin into %s\n", dir.c_str());
#endif
        return 1;
    }
    const std::string bios_sha1 = sha1_of_file(bios);

    fs::create_directories(dir / "saves", ec);
    const fs::path save = dir / "saves" / "hod.sav";
    const fs::path config_path = dir / "hod.cfg";

    port::PortConfig config;
    if (!config.load(config_path.string())) config.save(config_path.string());

    std::printf("[hod] rom=%s bios=%s save=%s\n", rom.c_str(), bios.c_str(), save.c_str());
    std::fflush(stdout);

    port::DualScreenOptions ds;
    ds.config_path = config_path.string();
    ds.enabled = !single_screen;
    port::dual_screen_init(ds, config);

    gbarecomp::RunOptions options;
    options.builtin_game_name = "Castlevania: Harmony of Dissonance";
    options.builtin_rom_sha1 = kRomSha1;
    options.builtin_rom_crc32 = 0x88C1B562;
    options.max_view_width = 240;
    options.freely_resizable_window = true;
    options.expose_assist_tools = false;
    options.resume_suspend_state_on_launch = config.resume_after_sleep;
    options.present_filter = port::dual_screen_present;
    options.host_event = port::dual_screen_event;
    options.scale_mode = static_cast<int>(config.top_scale);

    // game.toml carries the Flash512 save type; it ships next to the binary.
    std::vector<std::string> args = {argv[0]};
    if (fs::is_regular_file(dir / "game.toml", ec)) {
        args.push_back("--config");
        args.push_back((dir / "game.toml").string());
    }
    args.insert(args.end(), {"--rom", rom.string(),
                                     "--bios", bios.string(),
                                     "--bios-sha1", bios_sha1,
                                     "--rom-sha1", kRomSha1,
                                     "--save", save.string(),
                                     "--volume", std::to_string(config.volume),
                                     // SWIs run natively (HLE); IRQ/Halt/IntrWait
                                     // run in the recompiled BIOS linked in.
                                     "--bios-hle",
                                     "--window",
                                     "--fullscreen"});
    args.insert(args.end(), passthrough.begin(), passthrough.end());
    std::vector<char*> native;
    for (auto& a : args) native.push_back(a.data());

    const int rc = gbarecomp::run_game(static_cast<int>(native.size()), native.data(), options);
    port::dual_screen_shutdown();
    std::printf("[hod] exited with code %d\n", rc);
    return rc;
}
