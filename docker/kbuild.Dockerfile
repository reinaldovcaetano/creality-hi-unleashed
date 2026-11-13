# Creality Hi Unleashed: ambiente de build do kernel (Debian bookworm + GCC 12 para ARMv7)
FROM debian:bookworm
LABEL org.opencontainers.image.source="https://github.com/reinaldovcaetano/creality-hi-unleashed" \
      org.opencontainers.image.description="Creality Hi Unleashed (alpha) build container" \
      org.opencontainers.image.licenses="GPL-3.0"
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
      gcc libc6-dev gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi make bc bison flex libssl-dev libelf-dev \
      python3 kmod cpio xz-utils patch squashfs-tools ca-certificates file >/dev/null \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /repo
