# ROCKNIX dual-screen port (Anbernic RG DS / RG DS Plus)

This folder of the project builds **`hod.aarch64`**, a native ROCKNIX port of
the static recompilation, packaged PortMaster-style. The game runs on the top
screen; the bottom touch screen shows the castle map, Juste's status, the port
settings and — while they are open — the game's own pause menu and map screen,
like a Nintendo DS Castlevania.

| Screen | Content |
|:--|:--|
| **Top** | The game (integer 4x = 960x640 by default, or fit / stretch) |
| **Bottom (touch)** | `MAPA` live castle map · `ESTADO` status · `AJUSTES` settings · the game's pause menu and map screen while open |

## Getting started

### Requirements

| | |
|:--|:--|
| Host | Docker (Docker Desktop with WSL2 on Windows). Builds are cross-compiled in `rocknix/docker`. |
| Game files | Your **Castlevania - Harmony of Dissonance (USA)** ROM (8 MiB, SHA-1 `b90da0d9be0b3a0893cd9e2c399056bcf9579e21`). No BIOS dump is needed: the port ships its own (see [Native BIOS](#native-bios)). |
| Device | ROCKNIX on an RG DS / RG DS Plus, with SSH enabled for `deploy` |

### Build and install (Windows / PowerShell)

```powershell
.\rocknix.ps1 image                 # once: toolchain image (Debian bookworm, SDL2 2.26)
.\rocknix.ps1 all                   # fetch GBARecomp + patch, cross-compile, package
.\rocknix.ps1 deploy -DeviceIp 192.168.1.50 -DevicePass <root password>
```

Linux / WSL / macOS: `make -f rocknix.mk image all deploy DEVICE_IP=192.168.1.50`.

Then copy your `.gba` into `/storage/roms/ports/hod/` (Samba share or SD card) and refresh the game list. Without SSH, copy `dist/ports/*`
into the `ports` folder by hand. `deploy` never overwrites saves, `hod.cfg` or
your game files.

> [!IMPORTANT]
> `dist/` never contains the ROM or BIOS unless you build with
> `-IncludeGameFiles` (`INCLUDE_GAME_FILES=1`) for a local install. Do not share
> such a package.

## Controls

| Action | Input |
|:--|:--|
| Game | D-Pad, A jump, B attack, L/R dashes, Start menu, Select map (as on GBA) |
| Quit | **L1 + Start + Select**, or `SALIR DEL JUEGO` in settings (tap twice) |
| Bottom tabs | `MAPA` · `ESTADO` · `AJUSTES` (tap) |
| Map | Tap the map: whole castle / zoom on Juste. Tap the title: current castle → A → B → A+B |

### Settings (`AJUSTES`)

| Option | Effect |
|:--|:--|
| `PANTALLA SUPERIOR` | `ENTERA` (integer 4x, sharpest), `AJUSTADA` (fit 1024x683), `ESTIRADA` (fill 4:3) |
| `FILTRO` | `NÍTIDO` (nearest) / `SUAVE` (linear) |
| `VOLUMEN` | 0–100 % (left half of the row lowers, right half raises) |
| `MENÚS ABAJO` | Pause menu and map screen on the bottom screen (top holds the last gameplay frame) |
| `INTERCAMBIAR A/B` | Swap the face buttons if your controller mapping feels reversed |
| `SALTO DE FRAMES` | `AUTO` (default: skip drawing up to 2 frames in a row only when a frame runs late), `NO`, `1`, `2`. The game itself always runs every frame |
| `TURBO` | Uncapped speed while on (not saved) |
| `MOSTRAR FPS` | Frame rate in the bottom header |
| `REANUDAR TRAS SUSPENDER` | Also write a suspend state when the device sleeps (resumes there if the battery dies) |
| `CALIBRAR TÁCTIL` | Tap the centre of three crosses; the average offset is saved (`touch_dx`, `touch_dy` in `hod.cfg`, bottom-canvas pixels) and added to every tap. The default lifts taps by 4 canvas px (16 screen px). `HOD_TOUCH_DEBUG=1` logs every tap |
| `GUARDAR/CARGAR ESTADO` | Save state slot 1 (tap twice) |
| `IDIOMA` | Español / English |

Settings are stored in `ports/hod/hod.cfg`.

## How it works

### Dual screen

ROCKNIX's sway configuration moves any window whose title contains `Bottom`
(also `Secondary`, `Sub`, `Screen 2`…) to the second output and powers that
panel on. The runtime opens the game window as usual; `src/port/dual_screen.cpp`
opens a second fullscreen window titled **"Harmony of Dissonance Bottom
Screen"** a few frames later, with its own accelerated SDL renderer that never
waits for vsync (the game window owns pacing). The bottom UI is drawn into a
256x192 canvas — DS resolution, an exact 4x on the 1024x768 panel — and only
re-uploaded when what it shows changes (≤ 30 Hz); the mirrored game menu is a
240x160 texture drawn at 4x at the same position.

### Runtime seams (`patches/gbarecomp/`)

GBARecomp is fetched at a pinned commit and patched with small, game-agnostic
hooks:

- `RunOptions::present_filter` — sees every frame before it is presented and may
  return a replacement (used to hold the gameplay frame on top while a menu is
  shown below, and to drive the bottom window once per frame).
- `RunOptions::host_event` — sees every SDL event first; events of the bottom
  window are consumed so touches never reach the game.
- `RunOptions::scale_mode` and `host_request()` — thread-safe requests applied
  between frames: quit, save/load state, volume, scale mode, filter, turbo,
  A/B swap, save flush, suspend state, resume and audio pause.
- `0002`: frameskip (`HostRequest::SetFrameSkip`, `GbaPpu::set_frame_skip`,
  `FramePacer::late()`), and `runtime_tick` tests CPSR.I before the
  out-of-line IRQ-pending call it runs after every guest instruction.

### Game knowledge (`src/port/hod_ram.h`, `hod_map.cpp`)

Every address was confirmed on the running game with `tools/research/hod_probe.c`
(libmgba, scripted input) or read from the code that uses it
(BeautifulHarmony decompilation):

| What | Where |
|:--|:--|
| In-castle mode: 1 play, 4 map screen, 6 pause menu | `0x02000048` |
| Room origin + castle (bit 0) | `0x02000070` |
| Exploration bits, A/B interleaved (64x40 cells) | `0x0200008C` |
| Player cell = room + (camera + entity pos) / 256 | `sub_0800AD6C` |
| Map cell flags / walls / doors | ROM `0x080DAD94`, `0x080DC194`, `0x080DC694` |
| HP, MP, hearts, level, stats, EXP, gold, books, relics | `0x0201854E…0x0201883F` |

The map is drawn by a C++ transcription of the game's own map routines
(`sub_0800A508` and helpers), reading the tables from **your** ROM at run time,
so rooms, walls, doors, save (red) and warp (yellow) rooms and the completion
percentage match the in-game map exactly. `tests/map_render_test.cpp` renders it
from a RAM dump for comparison with the game's map screen.

### Performance

- Native aarch64, `-march=armv8.2-a -mtune=cortex-a55`, `-O2` everywhere, non-PIE.
- The four ~50 MB generated shards are split at function boundaries
  (`tools/split_shards.py`) into 6 MB units: parallel builds with bounded memory
  and -O2 for the game code (`GUEST_OPT=-O1` lowers it if a build machine
  struggles). The generated code itself is never edited.
- **Native BIOS — the biggest single win.** See below.
- **Inline guest fast paths** (patch `0003`, `armv4t/runtime_guest_fastpath.h`,
  compiled only into the game code): the generated code calls
  `runtime_should_yield()` and `runtime_tick()` after every GBA instruction, a
  bus helper plus `runtime_mem_cycles()` per access and a flag helper for most
  Thumb instructions. They are now inline: the tick only adds to the cycle
  budget until the next device event (then the full `runtime_tick` runs: DMA
  steal, IRQ), EWRAM/IWRAM/ROM accesses go straight to memory, and the
  per-instruction trace checks the port never arms are compiled out. Anything
  that ticks devices, writes MMIO, pumps input or loads a state re-arms the
  full path, so behaviour is unchanged.
  The binary is linked non-PIE so this state, `g_cpu` and the wait table are
  addressed directly instead of through the GOT. On x86 the game time per
  frame went from 1.54 to 1.12 ms (PPU thread included); captures of a
  17 000-frame scripted run are identical with and without these paths.
- **PPU on a second core** (`GBARECOMP_PPU_THREAD=1`, set by the port): each
  visible line is composed by a worker thread from a copy of that line's
  display registers, palette and OAM taken at its HBlank (VRAM is read live).
  The frame is latched only after the worker finished it. On the device the
  PPU was ~3 ms of each drawn frame.
- **Audio never blocks the game**: the SDL callback resamples under the bridge
  mutex (up to 7 ms on the device); the producer now uses a try-lock and
  carries the block over to the next frame instead of waiting.
- **Full coverage**: 11 PCs the first on-device session interpreted (a game
  area at ~21 fps) were added to `game.toml`; `src/game` was regenerated with
  the pinned `gba_recompile` (314 more functions, 18 825 in total).
- **Frameskip** (`SALTO DE FRAMES`, patch `0002`): a skipped frame runs the
  game normally but is neither rasterized by the PPU nor presented. `AUTO`
  skips only after the frame pacer finds a frame late, at most two in a row
  (never below 20 drawn fps). In an x86 run, PPU plus presentation was half
  of each frame: uncapped speed went from 367 to 686 fps with `2`.
- The launcher switches the CPU governor to `performance` while the game runs
  and restores the previous one on exit (`HOD_KEEP_GOVERNOR=1` to skip).
- Diagnostics in `ports/hod/`: `log.txt` gets the governor and a `perf:` line
  every ~5 s (`HOD_PERF_LOG=1`); `frame_phase.csv` (written on exit,
  `GBARECOMP_FRAME_PHASE`) has a per-frame breakdown in microseconds: guest
  code (incl. PPU), render copy, present (incl. the bottom screen), audio,
  input and pacer wait. Both are on in this build and cost nothing measurable.

### Native BIOS

BIOS HLE (`--bios-hle`) runs the math, copy and decompression SWIs in C++, but
the IRQ vector, `Halt`, `VBlankIntrWait`, `SoftReset` and `RegisterRamReset`
still need BIOS code every frame. Without a recompiled BIOS that code — and
everything it calls, including the game's own interrupt handler — ran through
the interpreter bridge: about 75 000 interpreted instructions per frame, each
syncing the CPU state, which was the first device report (~5 fps; `bridged
0x00000018 / 0x00000138 / 0x00000008` in `log.txt`).

`rocknix/bios/port_bios.s` is a small replacement BIOS written for this port
from GBATEK's description (no Nintendo code) that implements exactly those
calls. It is recompiled with `gba_recompile --bios` (`scripts/gen-port-bios.sh`;
the output in `rocknix/bios/generated/` is committed) and linked into every
build, and the package ships the matching `port_bios.bin`. The run is now
`FULLY_STATIC` with 0 interpreted instructions, and x86 uncapped speed went
from 148 to 367 fps.

To use your own BIOS instead, put the dump at `rocknix/bios/gba_bios.bin`
before building: `build.sh` recompiles it and the game then expects
`gba_bios.bin` in `ports/hod/`. That binary contains BIOS-derived code — keep
it for your own device.

## Testing without the device

`tools/research/` holds the tooling used to build and check the port:

| Tool | Purpose |
|:--|:--|
| `hod_probe.c` + `run_probe.sh` | libmgba runner with scripted input, RAM dumps, screenshots and watchpoints (how the RAM layout and map routines were found) |
| `run_x86_test.sh` | Native x86_64 build + headless run (SDL offscreen) of the real port with your ROM, the port BIOS, a scripted new game (`make_replay.py`) and simulated taps; dumps both screens every N frames |
| `tests/map_render_test.cpp` | Renders the castle map from a ROM + RAM dump |

Development environment variables (all off by default): `HOD_DEBUG_TAPS`,
`HOD_DEBUG_DUMP_DIR`, `HOD_DEBUG_DUMP_EVERY`, `HOD_DEBUG_TURBO`,
`HOD_BOTTOM_RENDERER=software`, `HOD_BOTTOM_DISPLAY=<n>`, `HOD_SINGLE_SCREEN=1`.

Verified this way (x86_64 and the aarch64 binary under QEMU): boot, title, new
game, intro, gameplay, live map and zoom, status, settings, language switch,
pause menu and map screen moved to the bottom screen with the held top frame.

### Suspend

`src/port/rocknix_sleep.cpp` (shared with the Zelda3 DS port) holds a logind
delay inhibitor. On `PrepareForSleep` the port asks the runtime to flush the
Flash save (and write a suspend state if enabled) and pauses audio, then
releases the inhibitor; on resume it clears the suspend marker and re-anchors
frame pacing.

### Versions

`rocknix/config/versions.env` pins GBARecomp **5e89943**. `src/game` was regenerated
with that commit's `gba_recompile` (`rocknix/scripts/regen-game.sh`, the Linux
counterpart of `scripts/recompile.ps1`). `docs/FRAMEWORK_PIN.json`
names `d11a362`, but the generated code in `src/game` uses the tail-call macros
(`GBARECOMP_TAIL_CALL`, …) that only exist in later commits.

## Pending on-device verification

- [ ] Window placement and touch mapping on the RG DS Plus (log: `dual_screen: bottom window on display …`).
- [ ] Locked 60 fps with the port BIOS (`perf:` lines in `log.txt`, `frame_phase.csv`). The first build, without a native BIOS, ran at about 5 fps.
- [ ] Controller face-button mapping under PortMaster's `control.txt` (`INTERCAMBIAR A/B` exists in case it is reversed).
- [ ] Real suspend/resume (`suspend: going to sleep` / `suspend: resumed` in `log.txt`).
