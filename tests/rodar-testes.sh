#!/bin/bash
# Creality Hi Unleashed: testa os scripts de boot (hi-limpa-overlay, hi-ajustes) DENTRO da rootfs gerada,
# rodando o busybox da própria imagem num ARM emulado (Docker linux/arm/v7 + chroot).
# Uso: tests/rodar-testes.sh [saida/CR4NU200360C20_ota_img_V1.2.0.0.img]
set -euo pipefail
cd "$(dirname "$0")/.."
IMG=${1:-$(ls saida/CR4NU200360C20_ota_img_V*.img | tail -1)}
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
echo "imagem: $IMG"
(cd "$T" && cpio -id --quiet rootfs < "$OLDPWD/$IMG")
docker compose run --rm -v "$T":/v pack sh -c 'unsquashfs -q -n -d /v/r /v/rootfs >/dev/null 2>&1; echo rootfs extraida'
cp tests/cenarios.sh "$T/"
docker run --rm --privileged --platform linux/arm/v7 -v "$T":/v -v "$PWD/pacote/fabrica":/fab busybox:1.36 sh /v/cenarios.sh
