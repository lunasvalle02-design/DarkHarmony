#!/usr/bin/env python3
"""Split the generated guest-code shards into smaller translation units.

The four cart_recompiled_*.cpp shards are ~50 MB each. Compiling one of them
needs several GB of RAM and runs on a single core; split at function
boundaries they build in parallel, with bounded memory, and can afford -O2.
The output is byte-for-byte the same functions, only redistributed, so the
generated code itself is never edited.

    split_shards.py <src/game dir> <out dir> [--chunk-mb 6]
"""
import argparse
import os
import sys

HEADER = '#include "runtime_arm.h"\n#include "recompiled_cart.h"\n\n'
SHARDS = [
    "cart_recompiled_core.cpp",
    "cart_recompiled_engine.cpp",
    "cart_recompiled_system.cpp",
    "cart_recompiled_support.cpp",
]


def split(path, out_dir, chunk_bytes):
    base = os.path.splitext(os.path.basename(path))[0]
    written = []
    part = 0
    size = 0
    out = None

    def open_next():
        nonlocal out, part, size
        if out:
            out.close()
        name = os.path.join(out_dir, "%s_%02d.cpp" % (base, part))
        written.append(name)
        out = open(name + ".tmp", "w", encoding="utf-8", newline="\n")
        out.write("// Split from %s by tools/split_shards.py (part %d).\n" % (os.path.basename(path), part))
        out.write(HEADER)
        part += 1
        size = 0

    with open(path, "r", encoding="utf-8") as src:
        in_body = False
        for line in src:
            # Function headers are the only column-0 "/* 0x" comments.
            if line.startswith("/* 0x"):
                if out is None or size >= chunk_bytes:
                    open_next()
                in_body = True
            if not in_body:
                continue  # file preamble: replaced by HEADER
            out.write(line)
            size += len(line)
    if out:
        out.close()
    # Replace atomically so an interrupted run never leaves half a chunk.
    for name in written:
        tmp = name + ".tmp"
        if os.path.exists(name):
            with open(name, "rb") as a, open(tmp, "rb") as b:
                if a.read() == b.read():
                    os.remove(tmp)
                    continue
        os.replace(tmp, name)
    return written


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src_dir")
    ap.add_argument("out_dir")
    ap.add_argument("--chunk-mb", type=float, default=6.0)
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    files = []
    for shard in SHARDS:
        files += split(os.path.join(args.src_dir, shard), args.out_dir, int(args.chunk_mb * 1024 * 1024))
    keep = set(os.path.abspath(f) for f in files)
    for name in os.listdir(args.out_dir):
        full = os.path.abspath(os.path.join(args.out_dir, name))
        if name.endswith(".cpp") and full not in keep:
            os.remove(full)
    for f in files:
        print(f)
    return 0


if __name__ == "__main__":
    sys.exit(main())
