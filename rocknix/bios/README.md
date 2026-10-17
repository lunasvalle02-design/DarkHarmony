# Native BIOS

`hod.aarch64` always runs its BIOS code natively. BIOS HLE (`--bios-hle`)
services the math, copy and decompression calls in C++; what is left for a
BIOS — the IRQ vector and `SoftReset`, `RegisterRamReset`, `Halt`,
`IntrWait`, `VBlankIntrWait` — comes from one of two places.

## Port BIOS (default)

`port_bios.s` is a small replacement BIOS written for this port from GBATEK's
description; it contains no Nintendo code. `scripts/gen-port-bios.sh`
assembles it into `port_bios.bin` and recompiles it with
`gba_recompile --bios` into `generated/`. All three are committed, so a
normal build needs no ARM assembler, and the package ships `port_bios.bin`:
**players do not need a BIOS dump.**

Without a native BIOS the IRQ vector ran through the interpreter bridge every
frame (about 75 000 interpreted instructions per frame, logged as `bridged
0x00000018 / 0x00000138 / 0x00000008` in `log.txt`), which made the game
unplayable on the RG DS Plus.

## Your own BIOS (optional)

Copy **your own** dump here as `gba_bios.bin` before building and
`scripts/build.sh` recompiles it into the binary instead. The game then
looks for `gba_bios.bin` in `ports/hod/` again. Such a binary contains
BIOS-derived code: use it on your own device only and never share it.
`gba_bios.bin` is ignored by git.
