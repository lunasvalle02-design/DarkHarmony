# Castlevania: Harmony of Dissonance Recomp

[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue)](#system-requirements)
[![Language](https://img.shields.io/badge/language-C%2B%2B20-orange)](#architecture)
[![License](https://img.shields.io/badge/license-PolyForm--Noncommercial--1.0.0-blue)](LICENSE)

Native C++ static recompilation of **Castlevania: Harmony of Dissonance** (Game Boy Advance, USA) for Windows x64, built on the [GBARecomp](https://github.com/mstan/gbarecomp) framework with the integrated native [recomp-ui](https://github.com/mstan/recomp-ui) Dear ImGui player launcher.

---

## Architecture

This project adopts an Ahead-of-Time (AOT) static recompiler architecture:

* **CPU Code Translation**: ARM7TDMI (ARM / Thumb) cartridge instructions are translated offline into native C++ compilation units (`src/game/`).
* **Silicon Emulation**: The shared GBARecomp runtime handles hardware peripherals:
  * **PPU**: Tile modes, OBJ sprites, window layers, and color blending.
  * **DMA / Timers / Interrupts / Keypad**
  * **APU / Direct Sound** audio synthesis.
  * **Flash512 Cartridge Save**: 64 KiB flash device emulation (`CASTLEVANIA1.00`).
* **Compilerless Player Runtime**: Runtime on-the-fly compiler is disabled by default (`GBARECOMP_SELFHEAL_RECOMPILE=0`). Execution paths not yet statically mapped gracefully fall back to the built-in Stage-1 interpreter without requiring user development tools.

> [!NOTE]
> **Verification Scope**: Verification covers BIOS boot, Konami logo, title screen, opening intro cutscene, character movement, status/equipment menus, and initial castle exploration in `FULLY_STATIC` recompiled mode. Full game completion across all endings and Boss Rush has not been exhaustively tested.

---

## ROCKNIX dual-screen port (RG DS / RG DS Plus)

A native aarch64 build for ROCKNIX, packaged PortMaster-style: the game on the
top screen; castle map, status, settings and the game's own pause menu and map
screen on the bottom touch screen. Build with Docker:

```powershell
.\rocknix.ps1 image
.\rocknix.ps1 all
.\rocknix.ps1 deploy -DeviceIp 192.168.1.50
```

See [docs/ROCKNIX.md](docs/ROCKNIX.md) for controls, settings, the optional
native BIOS and how the port works.

---

## Verified ROM & BIOS Requirements

This project is an open-source engine reimplementation and does **not** contain copyrighted game ROMs or BIOS files. You must provide your own legally obtained dumps:

| Field | Value |
|---|---|
| **Game** | Castlevania: Harmony of Dissonance (USA) |
| **Game Code** | `ACHE` |
| **ROM Size** | `8,388,608` bytes (8 MiB) |
| **ROM SHA-1** | `b90da0d9be0b3a0893cd9e2c399056bcf9579e21` |
| **ROM CRC32** | `88C1B562` |
| **BIOS Size** | `16,384` bytes (16 KiB) |
| **BIOS SHA-1** | `300c20df6731a33952ded8c436f7f186d25d3492` |
| **Save Type** | Flash 512 Kbit (64 KiB, signature `CASTLEVANIA1.00`) |

Strict SHA-1 validation is enforced on both the ROM and the BIOS before launch.

---

## recomp-ui Graphical Launcher

Double-click `CastlevaniaHarmonyOfDissonanceRecomp.exe` (or `hod.exe`) to launch the native Dear ImGui interface:

1. **Game Page**:
   - File pickers for ROM and GBA BIOS.
   - Real-time SHA-1 hash validation and file size checks.
   - **Play**, **Skip Launcher**, and **Reset Settings** buttons.
2. **Input Page**:
   - Keyboard and SDL2 GameController controller support.
   - Hotplugging and real-time button remapping.
   - **Default Controls**:
     - Movement: Arrow keys / WASD
     - Jump: `Z` (GBA A)
     - Attack / Confirm: `X` (GBA B)
     - Backdash: `Q` / `A` (GBA L)
     - Forward Dash / Subweapon: `E` / `S` (GBA R)
     - Pause Menu: `Enter` (GBA Start)
     - Map View: `Right Shift` / `Backspace` (GBA Select)
     - Fullscreen Toggle: `Alt + Enter`
3. **Video Page**:
   - 1x to 6x integer window scaling.
   - Aspect ratio preservation (3:2, native 240×160 resolution).
   - Windowed and fullscreen mode toggles.
4. **Audio Page**:
   - Master volume slider with instant feedback and persistent storage.
5. **Configuration & Persistence**:
   - Settings are stored portably in `config.ini`, `keybinds.ini`, `rom.cfg`, and `bios.cfg`.
   - Pass `--launcher` from the command line anytime to reopen the launcher.
   - Headless CLI parameters supported: `--rom <path>`, `--bios <path>`, `--save <path>`, `--config <path>`.

---

## Flash512 Save Support

* This game uses a **Flash 512 Kbit (65,536 bytes)** chip (`type = "flash512"`). It does **not** use SRAM.
* New saves default to `saves/hod.sav`.
* The launcher automatically discovers and preserves existing legacy saves in the executable directory (`Castlevania - Harmony of Dissonance(US)(Konami)(64Mb).sav`, `hod.sav`).
* The `--save <path>` parameter has highest priority.

---

## Developer Build Instructions

To build the executable from source using MinGW-w64:

```powershell
# 1. Setup SDL2 SDK (optional if _sdl2 is present)
.\scripts\setup_sdl2.ps1

# 2. Build host executable with 2 parallel jobs (default)
.\scripts\build.ps1 -Jobs 2

# 3. Run launcher contract test
.\scripts\build.ps1 -BuildContractTest
```

---

## Third-Party Notices & Licenses

* Castlevania: Harmony of Dissonance is a registered trademark of Konami Digital Entertainment Co., Ltd.
* This recompilation project is released under the **PolyForm Noncommercial License 1.0.0** (see [LICENSE](LICENSE)).
* Third-party components (GBARecomp, recomp-ui, Dear ImGui, SDL2, toml++) are attributed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
