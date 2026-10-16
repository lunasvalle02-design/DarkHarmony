// Castlevania: Harmony of Dissonance (USA, ACHE) — native static recompilation host.
// Windows GUI subsystem entry point: no console window, player-facing recomp-ui launcher.

#include "runtime.h"

#include <clocale>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

int game_launcher_preboot(std::vector<std::string>&, const gbarecomp::RunOptions&);
int game_validate_inputs(std::vector<std::string>&, const gbarecomp::RunOptions&);

namespace {

void remove_if_exists(const char* path) {
    if (path && path[0]) {
        std::remove(path);
    }
}

void cleanup_runtime_diagnostics() {
    const char* frag = std::getenv("GBARECOMP_MISS_FRAG");
    const char* cov = std::getenv("GBARECOMP_COVERAGE_JSON");
    remove_if_exists(frag);
    remove_if_exists(cov);
    remove_if_exists("recomp_master_misses_ACHE.toml.frag");
    remove_if_exists("recomp_coverage.json");
    remove_if_exists("recomp_seed_proposals.toml");
}

}  // namespace

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, ".UTF-8");
    // Suppress debug diagnostic dumps and on-the-fly recompile cache in normal runs
#ifdef _WIN32
    if (!std::getenv("GBARECOMP_SELFHEAL_RECOMPILE"))
        _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE", "0");
    if (!std::getenv("GBARECOMP_MISS_FRAG"))
        _putenv_s("GBARECOMP_MISS_FRAG", "NUL");
    if (!std::getenv("GBARECOMP_COVERAGE_JSON"))
        _putenv_s("GBARECOMP_COVERAGE_JSON", "NUL");
    if (!std::getenv("GBARECOMP_SESSION_DIAGNOSTICS"))
        _putenv_s("GBARECOMP_SESSION_DIAGNOSTICS", "0");
    if (!std::getenv("GBARECOMP_PRESENT_IN_PLACE"))
        _putenv_s("GBARECOMP_PRESENT_IN_PLACE", "0");
    if (!std::getenv("GBARECOMP_HANG_WATCHDOG"))
        _putenv_s("GBARECOMP_HANG_WATCHDOG", "0");

    const char* test_input = std::getenv("GBARECOMP_ENABLE_TEST_INPUT");
    if (!test_input || std::strcmp(test_input, "1") != 0) {
        _putenv_s("GBARECOMP_INPUT_REPLAY", "");
    }
    const char* debug_dumps = std::getenv("GBARECOMP_DEBUG_DUMPS");
    if (!debug_dumps || std::strcmp(debug_dumps, "1") != 0) {
        for (const char* name : {"GBARECOMP_INPUT_RECORD",
                                 "GBARECOMP_DUMP_STATE",
                                 "GBARECOMP_AUDIO_WAV",
                                 "GBARECOMP_FP_SAVE",
                                 "GBARECOMP_IRQ_LOG",
                                 "GBARECOMP_SWI_LOG",
                                 "GBARECOMP_WRAM_TRACE",
                                 "GBARECOMP_IWRAM_DUMP",
                                 "GBARECOMP_FRAME_PHASE"}) {
            _putenv_s(name, "");
        }
    }
#else
    setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 0);
    setenv("GBARECOMP_MISS_FRAG", "/dev/null", 0);
    setenv("GBARECOMP_COVERAGE_JSON", "/dev/null", 0);
    setenv("GBARECOMP_SESSION_DIAGNOSTICS", "0", 0);
    setenv("GBARECOMP_PRESENT_IN_PLACE", "0", 0);
    setenv("GBARECOMP_HANG_WATCHDOG", "0", 0);
#endif

    std::atexit(cleanup_runtime_diagnostics);

    // Recomp runtime options for Castlevania: Harmony of Dissonance
    gbarecomp::RunOptions options;
    options.builtin_game_name = "Castlevania: Harmony of Dissonance";
    options.builtin_rom_sha1  = "b90da0d9be0b3a0893cd9e2c399056bcf9579e21";
    options.builtin_rom_crc32 = 0x88C1B562;
    options.max_view_width = 240;
    options.freely_resizable_window = true;
    options.expose_assist_tools = false;
    options.launcher_region = "USA";
    options.launcher_game_config = "game.toml";
    options.launcher_expose_widescreen = false;
    options.launcher_expose_adaptive_view = false;

    try {
        std::vector<std::string> args(argv, argv + argc);
        const bool player_launch = argc == 1 || (argc == 2 && args[1] == "--launcher");

#ifdef _WIN32
        wchar_t exe_buf[MAX_PATH] = {0};
        GetModuleFileNameW(NULL, exe_buf, MAX_PATH);
        const std::filesystem::path exe_path(exe_buf);
        const std::filesystem::path exe_dir = exe_path.parent_path();
#else
        std::error_code ec;
        auto exe_path = std::filesystem::absolute(argv[0], ec);
        auto exe_dir = ec ? std::filesystem::current_path() : exe_path.parent_path();
#endif

        // Set current working directory to exe_dir so relative paths resolve correctly
        std::error_code ec;
        std::filesystem::current_path(exe_dir, ec);

        // Normalize args[0] and argv[0] to ASCII executable name to protect framework internals
        static char safe_exe_name[] = "CastlevaniaHarmonyOfDissonanceRecomp.exe";
        argv[0] = safe_exe_name;
        if (!args.empty()) args[0] = safe_exe_name;

        // Save path resolution: preserve legacy root-level save if present,
        // otherwise use saves/hod.sav cleanly without clobbering or deleting.
        std::string save_path;
        bool has_cli_save = false;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if ((args[i] == "--save" || args[i] == "--save-path") && i + 1 < args.size()) {
                has_cli_save = true;
                save_path = args[i + 1];
                break;
            }
        }

        if (!has_cli_save) {
            const std::filesystem::path legacy_rom_save = exe_dir / "Castlevania - Harmony of Dissonance(US)(Konami)(64Mb).sav";
            const std::filesystem::path legacy_usa_save = exe_dir / "Castlevania - Harmony of Dissonance (USA).sav";
            const std::filesystem::path root_hod_save = exe_dir / "hod.sav";
            const std::filesystem::path sub_hod_save = exe_dir / "saves" / "hod.sav";
            const std::filesystem::path sub_rom_save = exe_dir / "saves" / "Castlevania - Harmony of Dissonance(US)(Konami)(64Mb).sav";

            if (std::filesystem::exists(legacy_rom_save)) {
                save_path = legacy_rom_save.string();
            } else if (std::filesystem::exists(legacy_usa_save)) {
                save_path = legacy_usa_save.string();
            } else if (std::filesystem::exists(root_hod_save)) {
                save_path = root_hod_save.string();
            } else if (std::filesystem::exists(sub_hod_save)) {
                save_path = sub_hod_save.string();
            } else if (std::filesystem::exists(sub_rom_save)) {
                save_path = sub_rom_save.string();
            } else {
                std::filesystem::create_directories(exe_dir / "saves", ec);
                save_path = sub_hod_save.string();
            }
        }
        options.launcher_save_path = save_path.c_str();

        // Run the recomp-ui GUI launcher preboot
        if (game_launcher_preboot(args, options)) return 0;
        // Strict ROM and BIOS gate
        if (game_validate_inputs(args, options)) return 1;

        if (player_launch) {
            if (!has_cli_save) {
                args.emplace_back("--save");
                args.emplace_back(options.launcher_save_path);
            }
            args.emplace_back("--window");
        }

        std::vector<char*> native_args;
        native_args.reserve(args.size());
        for (auto& arg : args) native_args.push_back(arg.data());

        const int result = gbarecomp::run_game(static_cast<int>(native_args.size()), native_args.data(), options);

#ifdef _WIN32
        if (result != 0 && player_launch) {
            MessageBoxA(nullptr,
                "The game could not start. Check your ROM and GBA BIOS dumps.",
                "Castlevania: Harmony of Dissonance Recomp", MB_OK | MB_ICONERROR);
        }
#endif
        return result;
    } catch (const std::filesystem::filesystem_error& e) {
        std::fprintf(stderr, "[FATAL] filesystem_error: %s (code=%d)\n", e.what(), e.code().value());
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[FATAL] exception: %s\n", e.what());
        return 1;
    }
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv_w) return 1;

    char** argv = static_cast<char**>(HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(char*) * (argc + 1)));
    if (!argv) {
        LocalFree(argv_w);
        return 1;
    }

    for (int i = 0; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, nullptr, 0, nullptr, nullptr);
        argv[i] = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, n ? n : 1));
        if (argv[i] && n > 0) {
            WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, argv[i], n, nullptr, nullptr);
        }
    }

    int rc = main(argc, argv);

    for (int i = 0; i < argc; ++i) {
        if (argv[i]) HeapFree(GetProcessHeap(), 0, argv[i]);
    }
    HeapFree(GetProcessHeap(), 0, argv);
    LocalFree(argv_w);

    return rc;
}
#endif
