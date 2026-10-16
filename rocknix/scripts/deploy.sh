#!/bin/sh
# Copies dist/ports to a ROCKNIX device over SSH (Network Settings -> Enable SSH).
#   DEVICE_IP=192.168.1.50 [DEVICE_PASS=...] [DEVICE_USER=root]
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${DEVICE_IP:?set DEVICE_IP}"
USER_="${DEVICE_USER:-root}"
DEST=/storage/roms/ports
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
if [ -n "${DEVICE_PASS:-}" ]; then
  export SSHPASS="$DEVICE_PASS"
  SSH="sshpass -e $SSH"
fi
# Never overwrite the player's saves, settings or game files on the device.
rsync -rlt --info=progress2 -e "$SSH" \
  --exclude 'saves/' --exclude 'hod.cfg' --exclude '*.gba' --exclude 'gba_bios.bin' \
  --exclude '*.state*' --exclude 'log.txt' \
  "$ROOT/dist/ports/" "$USER_@$DEVICE_IP:$DEST/"
echo "deployed to $DEVICE_IP:$DEST — refresh the game list in EmulationStation"
