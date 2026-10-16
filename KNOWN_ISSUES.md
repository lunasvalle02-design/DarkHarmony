# Known Issues & Compatibility Status

## Current Release Status (v0.1.0-preview)

- **Platform**: Windows x64 native binary.
- **Launcher**: Native `recomp-ui` Dear ImGui pre-boot launcher integrated.
- **ROM/BIOS Verification**: Strict SHA-1 and ROM size gates enforced (USA ACHE ROM and retail GBA BIOS).
- **Input**: Keyboard and SDL2 GameController mapping supported with persistence.
- **Video**: 1x–6x integer window scale, aspect ratio preservation (3:2, 240x160), and fullscreen toggles.
- **Audio**: Master volume slider and audio synthesis fully active.
- **Save**: Flash 512 Kbit (64 KiB, `CASTLEVANIA1.00` signature) supported.

## Limitations & Verification Scope

1. **Boot and Intro Verified**: Boot sequence (LLE BIOS intro), Konami logo, title screen, menu, new game intro, and initial castle exploration have been verified in static recompiled mode (`FULLY_STATIC` execution path). Full game completion has not been exhaustively tested across all routes, endings, or Boss Rush.
2. **Audio Verification**: Automated smoke tests verify audio subsystem initialization and synthesis routing. Real-time listening and acoustic balance remain for player on-hardware trial.
3. **Save Compatibility**: Saves are loaded from and stored into standard 64 KiB Flash512 format (`type = "flash512"`). While matching the official GBA save structure, universal byte-for-byte emulator equivalence is not guaranteed across all third-party emulators.
4. **Offline Player Package**: The release archive is completely self-contained and requires no developer tools (no GCC, MinGW, CMake, Python, or Ninja).
