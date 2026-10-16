#!/bin/sh
# probe.sh <script> — build + run hod_probe inside the research image.
# Expects the ROM at /tmp/hod.gba and writes to /out.
set -e
cc -O2 -o /tmp/hod_probe /work/hod_probe.c -lmgba
/tmp/hod_probe /tmp/hod.gba "$1" /out
cd /out && for f in *.ppm; do [ -f "$f" ] && python3 -c "from PIL import Image;import sys;Image.open(sys.argv[1]).resize((480,320),Image.NEAREST).save(sys.argv[1][:-4]+'.png')" "$f" && rm "$f"; done; true
