#!/bin/sh
# Assembles the PortMaster-style package in dist/ports and dist/hod_ds.zip.
# Copies your ROM/BIOS into the package only when INCLUDE_GAME_FILES=1 (local
# installs); never distribute a package that contains them.
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/build/rocknix/hod.aarch64.stripped"
DIST="$ROOT/dist"
PORT="$DIST/ports/hod"
[ -f "$BIN" ] || { echo "build first: $BIN missing" >&2; exit 1; }

rm -rf "$DIST/ports"
mkdir -p "$PORT/saves"
cp "$ROOT/rocknix/port/Castlevania - Harmony of Dissonance DS.sh" "$DIST/ports/"
cp "$ROOT/rocknix/port/hod/"* "$PORT/"
cp "$BIN" "$PORT/hod.aarch64"
# Port BIOS image, unless this build links a BIOS recompiled from your dump.
[ -d "$ROOT/build/rocknix/generated_bios" ] || cp "$ROOT/rocknix/bios/port_bios.bin" "$PORT/port_bios.bin"
cp "$ROOT/game.toml" "$PORT/game.toml"
cp "$ROOT/LICENSE" "$PORT/LICENSE.txt"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$PORT/THIRD_PARTY_NOTICES.md"
cp "$ROOT/rocknix/port/port.json" "$PORT/port.json"
cp "$ROOT/rocknix/port/gameinfo.xml" "$PORT/gameinfo.xml"
chmod +x "$PORT/hod.aarch64" "$DIST/ports/Castlevania - Harmony of Dissonance DS.sh"

if [ "${INCLUDE_GAME_FILES:-0}" = "1" ]; then
  for f in "$ROOT"/rom/*.gba "$ROOT"/rom/gba_bios.bin; do
    [ -f "$f" ] && cp "$f" "$PORT/" && echo "included $(basename "$f") (local install only)"
  done
fi

(cd "$DIST/ports" && rm -f "$DIST/hod_ds.zip" && zip -qr "$DIST/hod_ds.zip" hod "Castlevania - Harmony of Dissonance DS.sh")
echo "package: $DIST/ports  ($(du -sh "$PORT" | cut -f1))"
