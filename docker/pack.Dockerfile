# Creality Hi Unleashed: monta a OTA (squashfs + cpio "070702" + sw-description)
FROM debian:bookworm
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
      squashfs-tools cpio python3 file >/dev/null && rm -rf /var/lib/apt/lists/*
WORKDIR /pacote
