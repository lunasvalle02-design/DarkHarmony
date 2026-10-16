#!/bin/sh
# run_probe.sh <rom> <outdir> <script-in-outdir>  (Git Bash / Linux host)
ROM="$1"; OUT="$2"; SCRIPT="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
w() { if command -v cygpath >/dev/null; then cygpath -w "$1"; else echo "$1"; fi; }
MSYS_NO_PATHCONV=1 docker run --rm -v "$(w "$HERE"):/work" -v "$(w "$(dirname "$ROM")"):/romdir:ro" -v "$(w "$OUT"):/out" \
  hod-research sh -c "cp \"/romdir/$(basename "$ROM")\" /tmp/hod.gba && sh /work/probe.sh /out/$SCRIPT"
