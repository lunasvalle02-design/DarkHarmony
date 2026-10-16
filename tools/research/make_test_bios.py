#!/usr/bin/env python3
"""Assembles test_bios.s into a 16 KiB stand-in BIOS (needs arm-none-eabi-as).
Test-only: players always provide their own BIOS dump."""
import os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
with tempfile.TemporaryDirectory() as d:
    subprocess.check_call(["arm-none-eabi-as", "-mcpu=arm7tdmi", "-o", d + "/b.o", here + "/test_bios.s"])
    subprocess.check_call(["arm-none-eabi-objcopy", "-O", "binary", d + "/b.o", d + "/b.bin"])
    img = open(d + "/b.bin", "rb").read()
open(sys.argv[1], "wb").write(img + bytes(16384 - len(img)))
