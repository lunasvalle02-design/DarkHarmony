Castlevania: Harmony of Dissonance DS — ROCKNIX dual-screen port
=================================================================

Copy into this folder (ports/hod/):
  * your Castlevania - Harmony of Dissonance (USA) ROM, any name ending in .gba
    (8 MiB, SHA-1 b90da0d9be0b3a0893cd9e2c399056bcf9579e21)
  (no BIOS dump is needed: the port ships its own, port_bios.bin)

Controls
  D-Pad move, A jump, B attack, R/L dash/backdash, Start menu, Select map
  L1 + Start + Select   quit to EmulationStation (the save is written first)

Bottom screen (touch)
  MAPA / MAP        castle map; tap the map to zoom, tap the title to switch castle
  ESTADO / STATUS   HP, MP, hearts, level, stats, play time, map completion
  AJUSTES / SETTINGS
                    top-screen scaling and filter, volume, menus on the bottom
                    screen, swap A/B, frameskip, turbo, FPS, touch calibration,
                    save/load state, language, quit

Files written here: saves/hod.sav (in-game save), hod.cfg (settings), log.txt,
*.state1 (save state) and *.suspend.state (only while the device sleeps).
Performance: frame_phase.csv (written on exit) and the "perf:" lines in
log.txt help diagnose slowdowns; please include both when reporting one.
