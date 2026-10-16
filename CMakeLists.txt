# Castlevania: Harmony of Dissonance (USA) — static recompilation host.
# Builds CastlevaniaHarmonyOfDissonanceRecomp.exe (and hod.exe) from src/ + recomp-ui + GBARecomp runtime.

cmake_minimum_required(VERSION 3.20)
project(CastlevaniaHarmonyOfDissonanceRecomp C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# GBARecomp framework
# ---------------------------------------------------------------------------
if(NOT GBARECOMP_ROOT)
    if(DEFINED ENV{GBARECOMP_ROOT})
        set(GBARECOMP_ROOT "$ENV{GBARECOMP_ROOT}")
    else()
        set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../gbarecomp-main")
    endif()
endif()
get_filename_component(GBARECOMP_ROOT "${GBARECOMP_ROOT}" ABSOLUTE)

if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "GBARECOMP_ROOT not found: ${GBARECOMP_ROOT}\n"
        "Clone https://github.com/mstan/gbarecomp (with submodules) next to this project.")
endif()

# Locate toml++
if(NOT GBARECOMP_TOMLPP_INCLUDE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/tomlplusplus")
        set(GBARECOMP_TOMLPP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/tomlplusplus" CACHE PATH "Pinned toml++ headers" FORCE)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../GoldenSunTheLostAgeRecomp/.deps/tomlplusplus")
        set(GBARECOMP_TOMLPP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../GoldenSunTheLostAgeRecomp/.deps/tomlplusplus" CACHE PATH "Pinned toml++ headers" FORCE)
    endif()
endif()

set(GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT OFF CACHE BOOL "Preview needs no runtime compiler" FORCE)
set(GBARECOMP_COMPILER_CACHE OFF CACHE STRING "Keep compilation output inside this project")

# ---------------------------------------------------------------------------
# recomp-ui launcher
# ---------------------------------------------------------------------------
if(NOT RECOMP_UI_ROOT)
    if(DEFINED ENV{RECOMP_UI_ROOT})
        set(RECOMP_UI_ROOT "$ENV{RECOMP_UI_ROOT}")
    else()
        set(RECOMP_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui")
    endif()
endif()
get_filename_component(RECOMP_UI_ROOT "${RECOMP_UI_ROOT}" ABSOLUTE)

if(NOT EXISTS "${RECOMP_UI_ROOT}/recomp_ui.cmake")
    message(FATAL_ERROR "recomp-ui not found at ${RECOMP_UI_ROOT}")
endif()

include("${RECOMP_UI_ROOT}/recomp_ui.cmake")

# ---------------------------------------------------------------------------
# SDL2 (window / input / audio)
# ---------------------------------------------------------------------------
if(NOT SDL2_ROOT)
    if(DEFINED ENV{SDL2_ROOT})
        set(SDL2_ROOT "$ENV{SDL2_ROOT}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.32.8/x86_64-w64-mingw32")
        set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.32.8/x86_64-w64-mingw32")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.30.11/x86_64-w64-mingw32")
        set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.30.11/x86_64-w64-mingw32")
    elseif(EXISTS "D:/Project/GameRecomp/_sdl2/SDL2-2.32.8/x86_64-w64-mingw32")
        set(SDL2_ROOT "D:/Project/GameRecomp/_sdl2/SDL2-2.32.8/x86_64-w64-mingw32")
    endif()
endif()

if(SDL2_ROOT)
    get_filename_component(SDL2_ROOT "${SDL2_ROOT}" ABSOLUTE)
    if(NOT SDL2_INCLUDE_DIR)
        set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include/SDL2")
    endif()
    if(NOT SDL2_LIBRARY)
        set(SDL2_LIBRARY "${SDL2_ROOT}/lib/libSDL2.dll.a")
    endif()
    if(NOT SDL2_DLL AND EXISTS "${SDL2_ROOT}/bin/SDL2.dll")
        set(SDL2_DLL "${SDL2_ROOT}/bin/SDL2.dll" CACHE FILEPATH "SDL2 runtime DLL")
    endif()
    set(GBARECOMP_MINGW_PREFIX_UNIX "${SDL2_ROOT}" CACHE PATH "SDL2 MinGW prefix" FORCE)
endif()

if(MINGW AND NOT GBARECOMP_MINGW_RUNTIME_BIN)
    get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(GBARECOMP_MINGW_RUNTIME_BIN "${_mingw_bin}" CACHE PATH "MinGW runtime DLLs")
endif()

add_subdirectory("${GBARECOMP_ROOT}" gbarecomp-build EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Launcher UI library
# ---------------------------------------------------------------------------
add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
target_include_directories(game_launcher_ui PRIVATE
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/gba"
    ${SDL2_INCLUDE_DIR}
)
recomp_target_launcher_ui(game_launcher_ui CONSOLE gba)
target_link_libraries(game_launcher_ui PUBLIC ${SDL2_LIBRARY})

# ---------------------------------------------------------------------------
# Recompiled guest code (ROM-derived C++ shards)
# ---------------------------------------------------------------------------
set(GAME_SOURCES
    src/game/cart_recompiled_core.cpp
    src/game/cart_recompiled_engine.cpp
    src/game/cart_recompiled_system.cpp
    src/game/cart_recompiled_support.cpp
    src/game/cart_dispatch_table.cpp
    src/game/cart_symbol_map.cpp
)

foreach(src ${GAME_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(FATAL_ERROR "Missing guest code shard: ${src}")
    endif()
endforeach()

add_library(hod_guest_code STATIC ${GAME_SOURCES})
target_include_directories(hod_guest_code PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
)
set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-g0")

# ---------------------------------------------------------------------------
# Desktop player executable
# ---------------------------------------------------------------------------
add_executable(CastlevaniaHarmonyOfDissonanceRecomp WIN32
    src/main.cpp
)

target_include_directories(CastlevaniaHarmonyOfDissonanceRecomp PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
    ${SDL2_INCLUDE_DIR}
)

target_compile_definitions(CastlevaniaHarmonyOfDissonanceRecomp PRIVATE
    GBARECOMP_WINDOW_TITLE="Castlevania Harmony of Dissonance"
    GBARECOMP_DEFAULT_GAME_CONFIG="game.toml"
    RECOMP_LAUNCHER
)

target_link_libraries(CastlevaniaHarmonyOfDissonanceRecomp PRIVATE
    game_launcher_ui
    -Wl,--whole-archive hod_guest_code -Wl,--no-whole-archive
    -Wl,--start-group
    gbarecomp_runtime
    gbarecomp_gba
    gbarecomp_armv4t
    gbarecomp_debug
    gbarecomp_recompile_core
    gbarecomp_heal_gate
    -Wl,--end-group
    ${SDL2_LIBRARY}
)

if(WIN32)
    target_link_options(CastlevaniaHarmonyOfDissonanceRecomp PRIVATE
        "-mwindows"
        "LINKER:--subsystem,windows"
        "LINKER:--stack,67108864"
        "LINKER:--allow-multiple-definition"
    )
    target_link_libraries(CastlevaniaHarmonyOfDissonanceRecomp PRIVATE
        opengl32 user32 gdi32 shell32 comdlg32 winmm ws2_32 imm32 version ole32 oleaut32 uuid advapi32 setupapi
    )
endif()

recomp_stage_launcher_assets(CastlevaniaHarmonyOfDissonanceRecomp CONSOLE gba)

if(SDL2_DLL)
    add_custom_command(TARGET CastlevaniaHarmonyOfDissonanceRecomp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SDL2_DLL}"
            "$<TARGET_FILE_DIR:CastlevaniaHarmonyOfDissonanceRecomp>/SDL2.dll"
        VERBATIM
    )
endif()

add_custom_command(TARGET CastlevaniaHarmonyOfDissonanceRecomp POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/game.toml"
        "$<TARGET_FILE_DIR:CastlevaniaHarmonyOfDissonanceRecomp>/game.toml"
    VERBATIM
)

# Compatibility alias target and executable copy for hod / hod.exe
add_custom_target(hod ALL DEPENDS CastlevaniaHarmonyOfDissonanceRecomp)
add_custom_command(TARGET CastlevaniaHarmonyOfDissonanceRecomp POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:CastlevaniaHarmonyOfDissonanceRecomp>"
        "$<TARGET_FILE_DIR:CastlevaniaHarmonyOfDissonanceRecomp>/hod.exe"
    VERBATIM
)

# ---------------------------------------------------------------------------
# Launcher contract test target
# ---------------------------------------------------------------------------
add_executable(launcher_contract EXCLUDE_FROM_ALL tests/launcher_contract.cpp)
target_compile_definitions(launcher_contract PRIVATE RECOMP_LAUNCHER)
target_include_directories(launcher_contract PRIVATE
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/gba"
    "${RECOMP_UI_ROOT}/src"
    "${RECOMP_UI_ROOT}/src/common"
    ${SDL2_INCLUDE_DIR}
)
target_link_libraries(launcher_contract PRIVATE game_launcher_ui gbarecomp_gba ${SDL2_LIBRARY})
if(WIN32)
    target_link_libraries(launcher_contract PRIVATE
        opengl32 user32 gdi32 shell32 comdlg32 winmm ws2_32 imm32 version ole32 oleaut32 uuid advapi32 setupapi
    )
endif()

if(SDL2_DLL)
    add_custom_command(TARGET launcher_contract POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SDL2_DLL}"
            "$<TARGET_FILE_DIR:launcher_contract>/SDL2.dll"
        VERBATIM
    )
endif()
