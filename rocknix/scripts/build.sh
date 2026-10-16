#!/bin/sh
# Cross-compiles hod.aarch64 (run inside the rocknix/docker image).
#   JOBS=n      parallel jobs (default: nproc, capped by memory: ~1.5 GB per job)
#   GUEST_OPT   optimization of the recompiled game code (default -O2)
#
# Optional native BIOS: put your own dump at rocknix/bios/gba_bios.bin and it is
# recompiled into the binary (gba_recompile --bios). The BIOS code that HLE
# leaves running every frame (IRQ vector, Halt, VBlankIntrWait) then runs as
# native code instead of through the interpreter: ~60% less CPU per frame in
# testing. A binary built this way contains BIOS-derived code — keep it for
# your own device and never redistribute it.
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build/rocknix"
export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig

if [ ! -f "$ROOT/external/gbarecomp/CMakeLists.txt" ]; then
  sh "$ROOT/rocknix/scripts/fetch-sources.sh"
fi

mem_kb=$(awk '/MemAvailable/ {print $2}' /proc/meminfo 2>/dev/null || echo 0)
mem_jobs=$(( mem_kb / 1500000 ))
[ "$mem_jobs" -ge 1 ] || mem_jobs=1
JOBS="${JOBS:-$(nproc)}"
[ "$JOBS" -le "$mem_jobs" ] || JOBS=$mem_jobs

BIOS="$ROOT/rocknix/bios/gba_bios.bin"
GEN_BIOS=""
if [ -f "$BIOS" ]; then
  echo "== native BIOS: recompiling $BIOS"
  # gba_recompile runs on the build host, so it gets its own native build.
  env -u PKG_CONFIG_LIBDIR cmake -S "$ROOT/external/gbarecomp" -B "$ROOT/build/host-tools" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_TOMLPP_INCLUDE_DIR="$ROOT/external/tomlplusplus" >/dev/null
  cmake --build "$ROOT/build/host-tools" --target gba_recompile --parallel "$JOBS" >/dev/null
  GEN_BIOS="$BUILD/generated_bios"
  mkdir -p "$GEN_BIOS"
  # The upstream symbol config only describes the retail BIOS.
  CONFIG=""
  if [ "$(sha1sum "$BIOS" | cut -d' ' -f1)" = "300c20df6731a33952ded8c436f7f186d25d3492" ]; then
    CONFIG="--config $ROOT/external/gbarecomp/bios/gba_bios.toml"
  fi
  # shellcheck disable=SC2086
  "$ROOT/build/host-tools/gba_recompile" --bios "$BIOS" $CONFIG --out "$GEN_BIOS" | tail -2
fi

cmake -S "$ROOT/rocknix" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/rocknix/cmake/aarch64-linux-gnu.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGBARECOMP_TOMLPP_INCLUDE_DIR="$ROOT/external/tomlplusplus" \
  -DHOD_GENERATED_BIOS_DIR="$GEN_BIOS" \
  -DHOD_GUEST_OPT="${GUEST_OPT:--O2}"
cmake --build "$BUILD" --target hod --parallel "$JOBS"
aarch64-linux-gnu-strip -o "$BUILD/hod.aarch64.stripped" "$BUILD/hod.aarch64"
file "$BUILD/hod.aarch64.stripped"
ls -la "$BUILD/hod.aarch64.stripped"
