# Optional: native BIOS

Copy **your own** GBA BIOS dump here as `gba_bios.bin` before building and
`scripts/build.sh` recompiles it into `hod.aarch64` (`gba_recompile --bios`).

Why: the runtime services most BIOS calls natively (HLE), but the IRQ vector,
`Halt` and `VBlankIntrWait` — every frame — still run real BIOS code. Without a
recompiled BIOS that code goes through the interpreter; recompiled it is native.
In an x86 profile this took the game from 148 to 239 uncapped fps.

The binary then contains BIOS-derived code: use it on your own device only and
never share it. `gba_bios.bin` is ignored by git.
