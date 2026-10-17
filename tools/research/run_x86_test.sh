#!/bin/sh
# Native x86_64 build of the ROCKNIX port and a headless end-to-end run with
# the user's ROM, the port BIOS and a scripted new game. Dumps the
# top and bottom screens (PPM -> PNG) to /out. Run in the hod-x86-test image:
#   docker run --rm -v <repo>:/work -v <rom dir>:/romdir:ro -v <out>:/out hod-x86-test \
#     sh tools/research/run_x86_test.sh
set -eu
cmake -S rocknix -B ${XBUILD:-build/x86} -G Ninja -DHOD_CPU_FLAGS="-march=x86-64-v2" -DHOD_GUEST_OPT=-O1 \
  -DGBARECOMP_TOMLPP_INCLUDE_DIR=/work/external/tomlplusplus -DHOD_SPLIT_DIR=/work/build/rocknix/guest -DHOD_GENERATED_BIOS_DIR= >/dev/null
cmake --build ${XBUILD:-build/x86} --target hod --parallel "${JOBS:-4}" | tail -2

P=/tmp/port; rm -rf $P; mkdir -p $P
cp ${XBUILD:-build/x86}/hod.x86_64 game.toml $P/
cp /romdir/*.gba $P/
cp rocknix/bios/port_bios.bin $P/
python3 tools/research/make_replay.py $P/replay.txt
rm -f /out/*.ppm /out/top_*.png /out/bottom_*.png

cd $P
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy GBARECOMP_ENABLE_TEST_INPUT=1 \
GBARECOMP_INPUT_REPLAY=$P/replay.txt HOD_DEBUG_TURBO=${TURBO:-0} \
HOD_DEBUG_DUMP_DIR=/out HOD_DEBUG_DUMP_EVERY=${DUMP_EVERY:-120} \
HOD_DEBUG_TAPS="${TAPS:-}" \
timeout "${RUN_SECONDS:-400}" ./hod.x86_64 --frames "${FRAMES:-9700}" > /out/run.log 2>&1 || echo "exit $?"
tail -15 /out/run.log
cd /out && python3 -c "
import glob
from PIL import Image
for p in glob.glob('*.ppm'):
    im = Image.open(p); im.resize((im.width*2, im.height*2), Image.NEAREST).save(p[:-4]+'.png')
" && rm -f *.ppm
ls /out | wc -l
