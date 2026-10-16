#!/usr/bin/env python3
"""Input trace (gbarecomp-keyinput-v1) that takes a new game from boot to
Juste's first steps, then opens the pause menu and the map. Timings follow
the mGBA probe runs (hod_probe.c) with generous margins.

    make_replay.py <out.txt>   -> prints the frames of interest
"""
import sys

KEYS = {"A": 0, "B": 1, "SELECT": 2, "START": 3, "RIGHT": 4, "LEFT": 5, "UP": 6, "DOWN": 7, "R": 8, "L": 9}
events = []  # (frame, mask_pressed)
def press(frame, *keys, hold=3):
    m = 0
    for k in keys:
        m |= 1 << KEYS[k]
    events.append((frame, m))
    events.append((frame + hold, 0))

f = 600
for k in ("START", "START", "A", "A", "A", "START", "A"):   # title -> file -> name -> intro
    press(f, k)
    f += 132
f += 200
while f < 15600:             # intro text, title card and Maxim's dialogue
    press(f, "A")
    f += 42
f += 240
gameplay = f
events.append((f, 1 << KEYS["RIGHT"]))   # walk right for 4 s
f += 240
events.append((f, 0))
f += 60
pause = f
press(f, "START")
f += 240
press(f, "START")            # close the menu
f += 120
mapscreen = f
press(f, "SELECT")
f += 240
press(f, "SELECT")
f += 180
end = f

with open(sys.argv[1], "w") as out:
    out.write("# gbarecomp-keyinput-v1\n# frame,keyinput_active_low\n")
    for frame, mask in sorted(events):
        out.write("%d,0x%03X\n" % (frame, (~mask) & 0x3FF))
print("gameplay=%d pause=%d map=%d end=%d" % (gameplay, pause, mapscreen, end))
