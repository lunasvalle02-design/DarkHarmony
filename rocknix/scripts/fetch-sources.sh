#!/bin/sh
# Clones the pinned GBARecomp runtime into external/ and applies patches/gbarecomp.
# Safe to re-run: an existing checkout is reset to the pinned commit first.
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
. "$ROOT/rocknix/config/versions.env"
EXT="$ROOT/external"
mkdir -p "$EXT"

if [ ! -d "$EXT/gbarecomp/.git" ]; then
  git clone --quiet "$GBARECOMP_REPO" "$EXT/gbarecomp"
fi
cd "$EXT/gbarecomp"
git -c advice.detachedHead=false fetch --quiet origin "$GBARECOMP_COMMIT" 2>/dev/null || git fetch --quiet origin
git reset --quiet --hard "$GBARECOMP_COMMIT"
git clean -qfd src
git submodule update --init --quiet external/arm-recomp-core
for p in "$ROOT"/patches/gbarecomp/*.patch; do
  [ -f "$p" ] || continue
  echo "applying $(basename "$p")"
  git apply --whitespace=nowarn "$p"
done

if [ ! -f "$EXT/tomlplusplus/toml.hpp" ]; then
  rm -rf "$EXT/tomlplusplus"
  git clone --quiet "$TOMLPP_REPO" "$EXT/tomlplusplus"
  git -C "$EXT/tomlplusplus" -c advice.detachedHead=false checkout --quiet "$TOMLPP_COMMIT"
fi
echo "sources ready: gbarecomp $(git -C "$EXT/gbarecomp" rev-parse --short HEAD) (patched)"
