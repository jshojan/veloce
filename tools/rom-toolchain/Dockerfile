# tools/rom-toolchain/Dockerfile — SH-3a
#
# One image, grouped by assembler, that builds every pinned assembler/toolchain
# the test-ROM plan (section 5) needs to rebuild upstream test ROMs from source
# and apply Veloce's patches. Every tool is built from a pinned commit/tag/tarball
# so "what built this ROM" is reproducible; `tests/roms-src/build.py` reads the
# tool versions this image records (see `tools/rom-toolchain/build-image.sh`,
# which runs each tool's version check and both LABELs the image and writes
# `versions.json` for `rom_manifest.json`).
#
# Build: tools/rom-toolchain/build-image.sh (wraps `docker build` + labeling).
# Digest-pin note: `devkitpro/devkitarm:20260610` and the `debian:trixie-slim`
# base are pinned by digest below; re-verify the digest before bumping the tag.

ARG DEBIAN_DIGEST=sha256:a99cfc517144bc59b1978475ec53b46ecabec7e43635402ee5b77cc54cd1b20a
ARG DEVKITARM_DIGEST=sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5

# ---------------------------------------------------------------------------
# Shared build base: git-clone-at-SHA helper deps only. Every source-built
# tool stage starts here so the "clone at a pinned SHA" step is one pattern:
#   git init && git remote add origin <url> && git fetch --depth 1 origin <sha> \
#     && git checkout FETCH_HEAD
# which works against GitHub for an arbitrary reachable commit, not just a tag.
# ---------------------------------------------------------------------------
FROM debian@${DEBIAN_DIGEST} AS build-base
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates git curl build-essential pkg-config \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# cc65 V2.19 (source, 555282497c3ecf8b313d87d5973093af19c35bd5)
# Serves: NES blargg modern, bisqwit, read_joy3, dmc_dma_during_read4,
# Holy Mapperel, all new NES ROMs; SNES gilyon cputest/spctest (S-CPU side),
# blargg SNES shell, Sour, lidnariq/tepples.
# ---------------------------------------------------------------------------
FROM build-base AS cc65
ARG CC65_SHA=555282497c3ecf8b313d87d5973093af19c35bd5
RUN mkdir -p /src/cc65 && cd /src/cc65 && git init -q \
    && git remote add origin https://github.com/cc65/cc65.git \
    && git fetch -q --depth 1 origin ${CC65_SHA} \
    && git checkout -q FETCH_HEAD
# "bin" target: builds src/ binaries only (ca65,cl65,cc65,ld65,od65,...),
# skips libsrc/doc/samples which cross-build C runtimes for ~30 8-bit targets.
RUN cd /src/cc65 && make -C src -j"$(nproc)" >/build.log 2>&1 || { tail -100 /build.log; exit 1; }
RUN mkdir -p /opt/rom-toolchain/versions \
    && /src/cc65/bin/ca65 --version >/opt/rom-toolchain/versions/cc65.txt 2>&1 || true

# ---------------------------------------------------------------------------
# freem/asm6f fd03633e7bf2612d7485280b291f9089197ae74b
# Serves: NES 2005-era ports (sprite_hit, sprite_overflow, ppu_2005,
# apu_2005 09-11, cpu_timing_test6).
# ---------------------------------------------------------------------------
FROM build-base AS asm6f
ARG ASM6F_SHA=fd03633e7bf2612d7485280b291f9089197ae74b
RUN mkdir -p /src/asm6f && cd /src/asm6f && git init -q \
    && git remote add origin https://github.com/freem/asm6f.git \
    && git fetch -q --depth 1 origin ${ASM6F_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/asm6f && gcc -O2 -Wall -Wno-unused-result -o asm6f asm6f.c
RUN mkdir -p /opt/rom-toolchain/versions \
    && (/src/asm6f/asm6f || true) >/opt/rom-toolchain/versions/asm6f.txt 2>&1

# ---------------------------------------------------------------------------
# ClusterM/nesasm v3.6 (2b39ec669082566aae043a04ed8f14d78824d3b9)
# Serves: AccuracyCoin.
# ---------------------------------------------------------------------------
FROM build-base AS nesasm
ARG NESASM_SHA=2b39ec669082566aae043a04ed8f14d78824d3b9
RUN mkdir -p /src/nesasm && cd /src/nesasm && git init -q \
    && git remote add origin https://github.com/ClusterM/nesasm.git \
    && git fetch -q --depth 1 origin ${NESASM_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/nesasm/source && make -j"$(nproc)"
RUN mkdir -p /opt/rom-toolchain/versions \
    && /src/nesasm/nesasm --version >/opt/rom-toolchain/versions/nesasm.txt 2>&1 || true

# ---------------------------------------------------------------------------
# nesasm-wine fallback (section 5: "+ wine fallback stage, built only if
# byte-identity fails"). NOT part of the default build graph or the runtime
# stage: only reachable with `docker build --target nesasm-wine`. It builds
# the Windows nesasm.exe path under Wine so a byte-identity check against the
# native Linux build (above) can be re-run if the native build ever disagrees
# with upstream's shipped/expected output.
# ---------------------------------------------------------------------------
FROM debian@${DEBIAN_DIGEST} AS nesasm-wine
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates git curl build-essential mingw-w64 wine wine32 \
    && rm -rf /var/lib/apt/lists/*
ARG NESASM_SHA=2b39ec669082566aae043a04ed8f14d78824d3b9
RUN mkdir -p /src/nesasm && cd /src/nesasm && git init -q \
    && git remote add origin https://github.com/ClusterM/nesasm.git \
    && git fetch -q --depth 1 origin ${NESASM_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/nesasm/source && make -j"$(nproc)" CC=i686-w64-mingw32-gcc TARGNES=../nesasm.exe \
    || (echo "wine fallback build failed; only used when byte-identity check requests it" && exit 1)

# ---------------------------------------------------------------------------
# vhelin/wla-dx 89a90a56be5c2b8cf19a9afa3e1b32384ddb1a97 (Mooneye's pinned CI
# commit). Serves: GB Mooneye, wilbertpol, Blargg GB, GBMicrotest;
# NES MMC1_A12 (P2); SNES cx4test (P2). Builds wla-6502/wla-65816/wla-gb/wlalink.
# ---------------------------------------------------------------------------
FROM build-base AS wla-dx
RUN apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*
ARG WLADX_SHA=89a90a56be5c2b8cf19a9afa3e1b32384ddb1a97
RUN mkdir -p /src/wla-dx && cd /src/wla-dx && git init -q \
    && git remote add origin https://github.com/vhelin/wla-dx.git \
    && git fetch -q --depth 1 origin ${WLADX_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/wla-dx && mkdir build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DGEN_DOC=OFF .. \
    && cmake --build . -- -j"$(nproc)"
RUN mkdir -p /opt/rom-toolchain/versions \
    && (/src/wla-dx/build/binaries/wla-gb -v || true) >/opt/rom-toolchain/versions/wla-dx.txt 2>&1

# ---------------------------------------------------------------------------
# RGBDS, source-built at two pins (section 1 decision #10): v0.6.1 for
# pre-1.0 syntax suites, v0.7.0 for suites that need the newer toolchain.
# Serves: GB SameSuite, Mealybug (visual, build-only), acid2, AGE, rtc3test,
# new GB ROMs (selected per suite by tests/roms-src recipes).
# ---------------------------------------------------------------------------
FROM build-base AS rgbds
RUN apt-get update && apt-get install -y --no-install-recommends \
      bison libpng-dev pkg-config \
    && rm -rf /var/lib/apt/lists/*
ARG RGBDS061_SHA=69a573923f208d625df589c7a54a18738b07969c
ARG RGBDS070_SHA=08f3e360c9525b65291db9cee66fc5eb6e4a45e4
RUN mkdir -p /src/rgbds-0.6.1 && cd /src/rgbds-0.6.1 && git init -q \
    && git remote add origin https://github.com/gbdev/rgbds.git \
    && git fetch -q --depth 1 origin ${RGBDS061_SHA} \
    && git checkout -q FETCH_HEAD \
    && make -j"$(nproc)" Q= all \
    && mkdir -p /opt/rgbds-0.6.1/bin \
    && cp rgbasm rgblink rgbfix rgbgfx /opt/rgbds-0.6.1/bin/
RUN mkdir -p /src/rgbds-0.7.0 && cd /src/rgbds-0.7.0 && git init -q \
    && git remote add origin https://github.com/gbdev/rgbds.git \
    && git fetch -q --depth 1 origin ${RGBDS070_SHA} \
    && git checkout -q FETCH_HEAD \
    && make -j"$(nproc)" Q= \
    && mkdir -p /opt/rgbds-0.7.0/bin \
    && cp rgbasm rgblink rgbfix rgbgfx /opt/rgbds-0.7.0/bin/
RUN mkdir -p /opt/rom-toolchain/versions \
    && /opt/rgbds-0.6.1/bin/rgbasm --version >/opt/rom-toolchain/versions/rgbds-0.6.1.txt 2>&1 \
    && /opt/rgbds-0.7.0/bin/rgbasm --version >/opt/rom-toolchain/versions/rgbds-0.7.0.txt 2>&1

# ---------------------------------------------------------------------------
# FASMARM 1.44, SHA256-pinned tarball (arm.flatassembler.net) — a Win32/Linux
# binary distribution, not a source build: FASMARM is a self-hosting fasm
# add-on that bootstraps from a prebuilt fasm/fasmarm binary, and the upstream
# package ships pre-built Linux x86/x86-64 executables alongside the source.
# Serves: GBA jsmolka, alyosha, FuzzARM.
# ---------------------------------------------------------------------------
FROM build-base AS fasmarm
ARG FASMARM_URL=http://arm.flatassembler.net/FASMARM_full.ZIP
ARG FASMARM_SHA256=3b424490014303584c4f8bcc8a1a665cdb7f52f37186059a0ced07f4b357adb7
RUN apt-get update && apt-get install -y --no-install-recommends unzip && rm -rf /var/lib/apt/lists/*
RUN mkdir -p /opt/fasmarm && cd /tmp \
    && curl -sSL -o fasmarm.zip "${FASMARM_URL}" \
    && echo "${FASMARM_SHA256}  fasmarm.zip" | sha256sum -c - \
    && unzip -q fasmarm.zip -d /opt/fasmarm \
    && chmod +x /opt/fasmarm/fasmarm.x64 /opt/fasmarm/fasmarm
RUN mkdir -p /opt/rom-toolchain/versions \
    && (/opt/fasmarm/fasmarm.x64 2>&1 | head -1 || true) >/opt/rom-toolchain/versions/fasmarm.txt

# ---------------------------------------------------------------------------
# devkitARM, image tag 20260610, digest-pinned after first pull. We copy the
# toolchain tree out of the published image rather than rebuild it: it's
# devkitPro's own reproducible release artifact.
# Serves: GBA mGBA suite, nba hw-test, new GBA C ROMs.
# ---------------------------------------------------------------------------
FROM devkitpro/devkitarm@${DEVKITARM_DIGEST} AS devkitarm-src
RUN mkdir -p /opt/rom-toolchain/versions \
    && /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc --version >/opt/rom-toolchain/versions/devkitarm.txt 2>&1 || true

# ---------------------------------------------------------------------------
# ARM9/bass v18 a847dfcb852c35b9736436c2f698a0c8cde51913
# Serves: SNES PeterLemon CPU/SPC700, GSU (P2); NES mmc5test_v2 (P2).
# ---------------------------------------------------------------------------
FROM build-base AS bass
ARG BASS_SHA=a847dfcb852c35b9736436c2f698a0c8cde51913
RUN mkdir -p /src/bass && cd /src/bass && git init -q \
    && git remote add origin https://github.com/ARM9/bass.git \
    && git fetch -q --depth 1 origin ${BASS_SHA} \
    && git checkout -q FETCH_HEAD
# nall (2013-era header library) is missing a few C++17 transitive includes
# that newer libstdc++ no longer pulls in implicitly (e.g. std::runtime_error);
# -include papers over that without patching vendored upstream source. `obj`/
# `out` aren't tracked by git (empty dirs) so a fresh checkout needs them made.
# (Overriding `compiler.cpp`, not `flags`: GNU make ignores a makefile's `+=`
# onto a variable that was set on the command line, and `flags += -I..` inside
# GNUmakefile is what makes `#include <nall/nall.hpp>` resolve at all.)
RUN cd /src/bass/bass && mkdir -p obj out \
    && make -j"$(nproc)" 'compiler.cpp=g++ -include stdexcept -include cstdint -x c++ -std=c++17 -fno-operator-names'
RUN mkdir -p /opt/rom-toolchain/versions \
    && (/src/bass/bass/out/bass || true) >/opt/rom-toolchain/versions/bass.txt 2>&1

# ---------------------------------------------------------------------------
# undisbeliever/bass-untech 9db6088a378061afc7b82f50997a5b9a1d951175
# Serves: SNES undisbeliever measurement tests.
# ---------------------------------------------------------------------------
FROM build-base AS bass-untech
ARG BASS_UNTECH_SHA=9db6088a378061afc7b82f50997a5b9a1d951175
RUN mkdir -p /src/bass-untech && cd /src/bass-untech && git init -q \
    && git remote add origin https://github.com/undisbeliever/bass-untech.git \
    && git fetch -q --depth 1 origin ${BASS_UNTECH_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/bass-untech/bass && mkdir -p obj out \
    && make -j"$(nproc)" 'compiler.cpp=g++ -include stdexcept -include cstdint -x c++ -std=c++17 -fno-operator-names'
RUN mkdir -p /opt/rom-toolchain/versions \
    && (/src/bass-untech/bass/out/bass-untech || true) >/opt/rom-toolchain/versions/bass-untech.txt 2>&1

# ---------------------------------------------------------------------------
# RPGHacker/asar v1.91 6a6dbdaed8cb773e583052ce4f66375bd896c3a1
# Serves: SNES byuu/jonasquinn xkas sources, VitorVilela7 (P2).
# ---------------------------------------------------------------------------
FROM build-base AS asar
RUN apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*
ARG ASAR_SHA=6a6dbdaed8cb773e583052ce4f66375bd896c3a1
RUN mkdir -p /src/asar && cd /src/asar && git init -q \
    && git remote add origin https://github.com/RPGHacker/asar.git \
    && git fetch -q --depth 1 origin ${ASAR_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/asar && mkdir build && cd build \
    && cmake ../src && cmake --build . -- -j"$(nproc)"
RUN mkdir -p /opt/rom-toolchain/versions \
    && find /src/asar/build -maxdepth 4 -iname 'asar' -type f -executable -exec cp {} /opt/rom-toolchain/asar \; \
    && test -x /opt/rom-toolchain/asar \
    && /opt/rom-toolchain/asar --version >/opt/rom-toolchain/versions/asar.txt 2>&1

# ---------------------------------------------------------------------------
# kleinesfilmroellchen/spcasm v1.2.0-rc1 1cab9d34f402d267b8d8b09d1cffb7d23c15b556
# rust-toolchain.toml pins `nightly-2024-09-12`; we install exactly that
# channel via rustup rather than the image's rustc.
# Serves: SNES gilyon spctest (SPC side), new spc_timer_dsp.
# ---------------------------------------------------------------------------
FROM build-base AS spcasm
ARG SPCASM_SHA=1cab9d34f402d267b8d8b09d1cffb7d23c15b556
ARG RUST_TOOLCHAIN=nightly-2024-09-12
ENV RUSTUP_HOME=/opt/rustup CARGO_HOME=/opt/cargo PATH=/opt/cargo/bin:$PATH
# One dependency's build script (parking_lot_core via siphasher's cc-rs use)
# looks for `clang` specifically as its linker driver.
RUN apt-get update && apt-get install -y --no-install-recommends clang && rm -rf /var/lib/apt/lists/*
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
      sh -s -- -y --default-toolchain none --no-modify-path -q
RUN mkdir -p /src/spcasm && cd /src/spcasm && git init -q \
    && git remote add origin https://github.com/kleinesfilmroellchen/spcasm.git \
    && git fetch -q --depth 1 origin ${SPCASM_SHA} \
    && git checkout -q FETCH_HEAD
RUN cd /src/spcasm && rustup toolchain install ${RUST_TOOLCHAIN} --profile minimal \
    && rustup default ${RUST_TOOLCHAIN} \
    && cargo build --release -p spcasm
RUN mkdir -p /opt/rom-toolchain/versions \
    && (/src/spcasm/target/release/spcasm --version \
        || /src/spcasm/target/release/spcasm -V \
        || true) >/opt/rom-toolchain/versions/spcasm.txt 2>&1

# ---------------------------------------------------------------------------
# Runtime: every tool copied into one image, plus python3/Pillow/numpy/make/git
# for build.py, the Holy Mapperel font step and the gilyon/FuzzARM generators.
# ---------------------------------------------------------------------------
FROM debian@${DEBIAN_DIGEST} AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
      python3 python3-pip python3-pil python3-numpy make git ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=cc65        /src/cc65/bin/                 /opt/rom-toolchain/cc65/
# cc65's own standard macro/include package (longbranch.mac and friends) that
# blargg-framework sources `.include`; ca65 has no built-in search path for
# it, so callers pass `-I /opt/rom-toolchain/cc65-asminc`.
COPY --from=cc65        /src/cc65/asminc/               /opt/rom-toolchain/cc65-asminc/
# cc65's own canned linker configs (nes.cfg etc.); some upstream ROM Makefiles
# reference "nes.cfg" bare (relying on it being alongside cc65 in $PATH-like
# fashion in the original author's environment) without shipping their own
# copy — callers pass an absolute -C path into this directory when a suite's
# own source tree has none.
COPY --from=cc65        /src/cc65/cfg/                   /opt/rom-toolchain/cc65-cfg/
COPY --from=asm6f       /src/asm6f/asm6f                /opt/rom-toolchain/bin/asm6f
COPY --from=nesasm      /src/nesasm/nesasm              /opt/rom-toolchain/bin/nesasm
COPY --from=wla-dx      /src/wla-dx/build/binaries/     /opt/rom-toolchain/wla-dx/
COPY --from=rgbds       /opt/rgbds-0.6.1/                /opt/rgbds-0.6.1/
COPY --from=rgbds       /opt/rgbds-0.7.0/                /opt/rgbds-0.7.0/
COPY --from=fasmarm     /opt/fasmarm/                    /opt/fasmarm/
COPY --from=devkitarm-src /opt/devkitpro/                /opt/devkitpro/
COPY --from=bass        /src/bass/bass/out/bass          /opt/rom-toolchain/bin/bass
COPY --from=bass-untech /src/bass-untech/bass/out/bass-untech /opt/rom-toolchain/bin/bass-untech
COPY --from=asar        /opt/rom-toolchain/asar          /opt/rom-toolchain/bin/asar
COPY --from=spcasm      /src/spcasm/target/release/spcasm /opt/rom-toolchain/bin/spcasm

COPY --from=cc65        /opt/rom-toolchain/versions/     /opt/rom-toolchain/versions/
COPY --from=asm6f       /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=nesasm      /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=wla-dx      /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=rgbds       /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=fasmarm     /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=devkitarm-src /opt/rom-toolchain/versions/    /opt/rom-toolchain/versions/
COPY --from=bass        /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=bass-untech /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=asar        /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/
COPY --from=spcasm      /opt/rom-toolchain/versions/      /opt/rom-toolchain/versions/

ENV PATH=/opt/rom-toolchain/bin:/opt/rom-toolchain/cc65:/opt/rom-toolchain/wla-dx:/opt/devkitpro/devkitARM/bin:$PATH
ENV DEVKITARM=/opt/devkitpro/devkitARM
ENV DEVKITPRO=/opt/devkitpro
ENV RGBDS_0_6_1=/opt/rgbds-0.6.1/bin
ENV RGBDS_0_7_0=/opt/rgbds-0.7.0/bin
ENV FASMARM=/opt/fasmarm/fasmarm.x64

WORKDIR /work
LABEL org.opencontainers.image.title="veloce-rom-toolchain" \
      org.opencontainers.image.description="Pinned assemblers/toolchains for rebuilding Veloce test ROMs from source (see docs/testing/VELOCE-RESULT.md and tests/roms-src/build.py)"
# Per-tool version LABELs are appended by build-image.sh after this build,
# via `docker commit --change 'LABEL ...'`, using the real captured
# `--version` output under /opt/rom-toolchain/versions/*.txt (also written to
# tools/rom-toolchain/versions.json for rom_manifest.json). A LABEL can't
# embed the output of a RUN from an earlier stage directly in one `docker
# build`, so the commit step is the second half of "image label with tool
# versions" — see build-image.sh's header comment.
