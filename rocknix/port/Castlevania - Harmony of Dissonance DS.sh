#!/bin/bash
# Castlevania: Harmony of Dissonance DS — launcher for ROCKNIX (Anbernic RG DS / RG DS Plus).
# Lives in /storage/roms/ports/ next to the hod/ folder.
#
# Top screen: the game. Bottom screen: castle map, status and settings (touch);
# the game's pause menu and map screen move there while open.
# ROCKNIX (sway) moves to the second screen, and powers on, any window whose title
# contains "Bottom", "Secondary", "Screen 2"... (see 111-sway-init), hence the fixed title.

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

# PortMaster is optional: if installed, use its control.txt (SDL controller mapping).
for d in /opt/system/Tools/PortMaster /opt/tools/PortMaster "$XDG_DATA_HOME/PortMaster" /storage/roms/ports/PortMaster /roms/ports/PortMaster; do
  [ -f "$d/control.txt" ] && { controlfolder="$d"; break; }
done
if [ -n "$controlfolder" ]; then
  # shellcheck disable=SC1091
  source "$controlfolder/control.txt"
  [ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"
  get_controls
  export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi

GAMEDIR="$(cd "$(dirname "$0")" && pwd)/hod"
cd "$GAMEDIR" || exit 1
exec > "$GAMEDIR/log.txt" 2>&1
echo "Castlevania: Harmony of Dissonance DS — $(date)"

if ! ls "$GAMEDIR"/*.gba >/dev/null 2>&1; then
  echo "ERROR: no .gba ROM in $GAMEDIR"
  echo "Copy your Castlevania - Harmony of Dissonance (USA) ROM here (8 MiB, SHA-1 b90da0d9...)."
  exit 1
fi
# Builds with the port BIOS ship port_bios.bin; retail-BIOS builds need your dump.
if [ ! -f port_bios.bin ] && [ ! -f gba_bios.bin ] && [ ! -f bios/gba_bios.bin ]; then
  echo "ERROR: gba_bios.bin is missing from $GAMEDIR"
  exit 1
fi

mkdir -p saves
chmod +x ./hod.aarch64

# Wayland app_id, for sway rules.
export SDL_VIDEO_WAYLAND_WMCLASS=hod
# Guest call chains use the host stack (the binary also raises this itself).
ulimit -s 65536 2>/dev/null

# Performance line in log.txt every ~5 s (fps and bottom-screen time), and a
# per-frame breakdown (guest, render, present, audio, input, pacing) written to
# frame_phase.csv on exit. Both are cheap; comment them out once all is well.
export HOD_PERF_LOG=1
export GBARECOMP_FRAME_PHASE="$GAMEDIR/frame_phase.csv"
# Audio bridge health every ~2 s: underruns, concealed gaps, ring fill.
export GBARECOMP_AUDIO_PROBE=1
echo "cpu: governor=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null) cur=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null) max=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null) video=${SDL_VIDEODRIVER:-auto}"

# Bottom screen renderer: GPU by default. Uncomment to force software rendering.
# export HOD_BOTTOM_RENDERER=software

# Single-screen devices: run without the bottom window.
# export HOD_SINGLE_SCREEN=1

# The recompiled game needs the big clocks: run on the performance governor
# and put the previous one back on exit. HOD_KEEP_GOVERNOR=1 leaves it alone.
saved_govs=""
if [ "${HOD_KEEP_GOVERNOR:-0}" != "1" ]; then
  for g in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
    [ -w "$g" ] || continue
    saved_govs="$saved_govs $g=$(cat "$g")"
    echo performance > "$g" 2>/dev/null
  done
  echo "cpu: governor now $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)"
fi

./hod.aarch64
rc=$?
echo "hod exited with code $rc"

for entry in $saved_govs; do
  echo "${entry#*=}" > "${entry%%=*}" 2>/dev/null
done

[ -n "$controlfolder" ] && unset SDL_GAMECONTROLLERCONFIG
exit $rc
