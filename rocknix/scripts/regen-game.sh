#!/bin/sh
# Regenerates src/game from your ROM and game.toml with the pinned GBARecomp's
# gba_recompile (the Linux counterpart of scripts/recompile.ps1). Run it in the
# x86 test image (tools/research/Dockerfile.x86test), with the ROM folder
# mounted at /romdir:
#   docker run --rm -v <repo>:/work -v <rom dir>:/romdir:ro hod-x86-test \
#     sh rocknix/scripts/regen-game.sh
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${ROM:-$(ls /romdir/*.gba | head -1)}"
OUT="$ROOT/build/regen"

if [ ! -x "$ROOT/build/host-tools/gba_recompile" ]; then
  cmake -S "$ROOT/external/gbarecomp" -B "$ROOT/build/host-tools" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_TOMLPP_INCLUDE_DIR="$ROOT/external/tomlplusplus" >/dev/null
  cmake --build "$ROOT/build/host-tools" --target gba_recompile >/dev/null
fi
rm -rf "$OUT"
mkdir -p "$OUT"
"$ROOT/build/host-tools/gba_recompile" --rom "$ROM" --config "$ROOT/game.toml" --out "$OUT" \
  --codegen-shards 4 > "$OUT.log" 2>&1 || { tail -20 "$OUT.log"; exit 1; }
grep -E "discovered|TOTAL emitted" "$OUT.log"

G="$ROOT/src/game"
sed 's/#include "recompiled.h"/#include "recompiled_cart.h"/' "$OUT/dispatch_table.cpp" > "$G/cart_dispatch_table.cpp"
sed 's/#include "recompiled.h"/#include "recompiled_cart.h"/' "$OUT/symbol_map.cpp" > "$G/cart_symbol_map.cpp"
cp "$OUT/recompiled.h" "$G/recompiled_cart.h"
for pair in 000:core 001:engine 002:system 003:support; do
  sed 's/#include "recompiled.h"/#include "recompiled_cart.h"/' "$OUT/recompiled_${pair%%:*}.cpp" \
    > "$G/cart_recompiled_${pair#*:}.cpp"
done
echo "src/game updated"
