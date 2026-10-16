# Research harness: runs the user's own ROM in libmgba (HLE BIOS) with scripted
# input to locate and validate RAM fields used by the dual-screen port.
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential libmgba-dev python3 python3-pil \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
