# Creality Hi Unleashed: monta a OTA (squashfs + cpio "070702" + sw-description)
FROM debian:bookworm
LABEL org.opencontainers.image.source="https://github.com/reinaldovcaetano/creality-hi-unleashed" \
      org.opencontainers.image.description="Creality Hi Unleashed (alpha) build container" \
      org.opencontainers.image.licenses="GPL-3.0"
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
      squashfs-tools cpio python3 file >/dev/null && rm -rf /var/lib/apt/lists/*
WORKDIR /pacote
