# Cross-compilation environment for ROCKNIX (aarch64) — Anbernic RG DS / RG DS Plus.
#
# x86_64 host (Windows + Docker Desktop/WSL2, Linux or Intel macOS) -> aarch64 binary.
# Debian bookworm: glibc 2.36 (ROCKNIX ships a newer one, so the binary is compatible)
# and SDL2 2.26.x, the branch PortMaster recommends for ports.
FROM debian:bookworm

ARG DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential crossbuild-essential-arm64 cmake ninja-build ccache pkg-config \
      libsdl2-dev:arm64 \
      git ca-certificates file zip unzip patch python3 \
      openssh-client rsync sshpass \
 && rm -rf /var/lib/apt/lists/*

# The volume mounted from Windows belongs to another user: avoid "dubious ownership".
RUN git config --system --add safe.directory "*"

ENV CCACHE_DIR=/work/.cache/ccache \
    CCACHE_MAXSIZE=8G
WORKDIR /work
