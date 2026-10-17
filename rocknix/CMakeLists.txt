# Castlevania: Harmony of Dissonance — ROCKNIX / PortMaster build (aarch64).
#
# Builds hod.aarch64: the recompiled game, the patched GBARecomp runtime
# (no launcher, no debug UI) and the dual-screen front end in src/port.
# Normally driven by scripts/build.sh inside the Docker image; see README.
cmake_minimum_required(VERSION 3.20)
project(HarmonyOfDissonanceRocknix C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

get_filename_component(HOD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GBARECOMP_ROOT "${HOD_ROOT}/external/gbarecomp" CACHE PATH "Patched GBARecomp checkout")
set(HOD_SPLIT_DIR "${CMAKE_BINARY_DIR}/guest" CACHE PATH "Split guest-code units (tools/split_shards.py)")

# Target CPU: RK3566/RK3568 (Cortex-A55). Override for other ROCKNIX devices.
set(HOD_CPU_FLAGS "-march=armv8.2-a+crc+crypto+dotprod -mtune=cortex-a55" CACHE STRING "Target CPU flags")
# Guest code is ~200 MB of generated C++: -O2 on the split units; drop to -O1
# if a build machine runs out of memory or time.
set(HOD_GUEST_OPT "-O2" CACHE STRING "Optimization level for the recompiled game code")

separate_arguments(_cpu_flags UNIX_COMMAND "${HOD_CPU_FLAGS}")
# Non-PIE: generated code touches g_cpu, g_rt and the wait table on every
# guest instruction; PIE would reach each through the GOT (two loads).
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
add_compile_options(${_cpu_flags} -fno-plt -g0 -fno-pie)
add_link_options(-no-pie)
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
add_link_options(-Wl,--gc-sections -Wl,-O1 -Wl,--as-needed)

# ---------------------------------------------------------------------------
# GBARecomp runtime (patched: patches/gbarecomp). SDL2 from the target sysroot.
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_path(SDL2_INCLUDE_DIR SDL.h PATHS ${SDL2_INCLUDE_DIRS} NO_DEFAULT_PATH)
find_library(SDL2_LIBRARY NAMES SDL2 PATHS ${SDL2_LIBRARY_DIRS})
set(GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT OFF CACHE BOOL "" FORCE)
set(GBARECOMP_ENABLE_MODS OFF CACHE BOOL "" FORCE)
# Optional: BIOS recompiled from the player's own dump (scripts/build.sh does it
# when rocknix/bios/gba_bios.bin exists). It turns the BIOS code that still runs
# under HLE — the IRQ vector, Halt and (VBlank)IntrWait — into native code
# instead of interpreting it every frame. The result embeds BIOS-derived code:
# never redistribute a binary built this way.
set(HOD_GENERATED_BIOS_DIR "" CACHE PATH "gba_recompile --bios output directory")
# Without it the port BIOS (rocknix/bios/port_bios.s, written for this port)
# is linked instead: the same calls, native, with no BIOS dump needed at all.
if(HOD_GENERATED_BIOS_DIR)
    set(GBARECOMP_GENERATED_BIOS_DIR "${HOD_GENERATED_BIOS_DIR}" CACHE PATH "" FORCE)
    set(HOD_PORT_BIOS 0)
else()
    set(GBARECOMP_GENERATED_BIOS_DIR "${HOD_ROOT}/rocknix/bios/generated" CACHE PATH "" FORCE)
    set(HOD_PORT_BIOS 1)
endif()
message(STATUS "hod: native BIOS = ${GBARECOMP_GENERATED_BIOS_DIR}")
add_subdirectory("${GBARECOMP_ROOT}" gbarecomp-build EXCLUDE_FROM_ALL)

# Runtime libraries are speed-critical too (PPU, scheduler, bus): keep them at
# -O2 with the target flags above, and without frame pointers.
foreach(lib gbarecomp_runtime gbarecomp_gba gbarecomp_armv4t gbarecomp_debug
            gbarecomp_recompile_core gbarecomp_heal_gate)
    target_compile_options(${lib} PRIVATE -fomit-frame-pointer -Wno-unused-parameter)
endforeach()

# ---------------------------------------------------------------------------
# Recompiled game code, split into ~6 MB units at configure time.
# ---------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "hod: splitting guest shards into ${HOD_SPLIT_DIR}")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${HOD_ROOT}/tools/split_shards.py"
            "${HOD_ROOT}/src/game" "${HOD_SPLIT_DIR}"
    OUTPUT_VARIABLE _split_out
    RESULT_VARIABLE _split_rc)
if(NOT _split_rc EQUAL 0)
    message(FATAL_ERROR "split_shards.py failed")
endif()
string(REPLACE "\n" ";" HOD_GUEST_UNITS "${_split_out}")
list(FILTER HOD_GUEST_UNITS INCLUDE REGEX "\\.cpp$")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${HOD_ROOT}/tools/split_shards.py"
    "${HOD_ROOT}/src/game/cart_recompiled_core.cpp"
    "${HOD_ROOT}/src/game/cart_recompiled_engine.cpp"
    "${HOD_ROOT}/src/game/cart_recompiled_system.cpp"
    "${HOD_ROOT}/src/game/cart_recompiled_support.cpp")

add_library(hod_guest_code STATIC
    ${HOD_GUEST_UNITS}
    "${HOD_ROOT}/src/game/cart_dispatch_table.cpp"
    "${HOD_ROOT}/src/game/cart_symbol_map.cpp")
target_include_directories(hod_guest_code PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${HOD_ROOT}/src/game")
separate_arguments(_guest_opt UNIX_COMMAND "${HOD_GUEST_OPT}")
target_compile_options(hod_guest_code PRIVATE ${_guest_opt} -fomit-frame-pointer
    -fno-exceptions -fno-asynchronous-unwind-tables -w)
# Inline fast paths for the per-instruction runtime calls (patch 0003,
# armv4t/runtime_guest_fastpath.h): the main CPU saving on the Cortex-A55.
target_compile_definitions(hod_guest_code PRIVATE GBARECOMP_INLINE_FASTPATH=1)

# ---------------------------------------------------------------------------
# Port front end + executable.
# ---------------------------------------------------------------------------
add_executable(hod
    "${HOD_ROOT}/src/port/port_main.cpp"
    "${HOD_ROOT}/src/port/dual_screen.cpp"
    "${HOD_ROOT}/src/port/hod_map.cpp"
    "${HOD_ROOT}/src/port/canvas.cpp"
    "${HOD_ROOT}/src/port/port_strings.cpp"
    "${HOD_ROOT}/src/port/port_config.cpp"
    "${HOD_ROOT}/src/port/rocknix_sleep.cpp")
set_target_properties(hod PROPERTIES OUTPUT_NAME "hod.${CMAKE_SYSTEM_PROCESSOR}")
target_include_directories(hod PRIVATE
    "${HOD_ROOT}/src/port"
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    ${SDL2_INCLUDE_DIRS})
target_compile_definitions(hod PRIVATE GBARECOMP_HAVE_SDL2 HOD_PORT_BIOS=${HOD_PORT_BIOS})
target_compile_options(hod PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(hod PRIVATE
    -Wl,--whole-archive hod_guest_code -Wl,--no-whole-archive
    -Wl,--start-group
    gbarecomp_runtime gbarecomp_gba gbarecomp_armv4t gbarecomp_debug
    gbarecomp_recompile_core gbarecomp_heal_gate
    -Wl,--end-group
    ${SDL2_LINK_LIBRARIES}
    dl pthread)

# Host test of the map renderer (native compiler builds only).
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(map_render_test EXCLUDE_FROM_ALL
        "${HOD_ROOT}/tests/map_render_test.cpp"
        "${HOD_ROOT}/src/port/hod_map.cpp")
    target_include_directories(map_render_test PRIVATE "${HOD_ROOT}/src/port")
endif()
