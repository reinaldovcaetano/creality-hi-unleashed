#!/bin/bash
# Creality Hi Unleashed: baixa e confere (SHA-256) tudo o que o build usa. Roda no seu computador (macOS/Linux).
# A OTA oficial da Creality NÃO é baixada aqui (é da Creality): baixe a 1.1.0.74 no site oficial e coloque em
#   insumos/CR4NU200360C20_ota_img_V1.1.0.74.img
set -euo pipefail
cd "$(dirname "$0")/.."
I=insumos; mkdir -p $I pacote/fluidd
sha() { if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }

# nome  SHA-256  URL   (commits fixos: o build é reproduzível)
while read -r nome soma url; do
    [ -z "$nome" ] && continue
    if [ ! -f "$I/$nome" ] || [ "$(sha "$I/$nome")" != "$soma" ]; then
        echo "baixando $nome"; curl -fL --retry 3 -o "$I/$nome" "$url"
    fi
    [ "$(sha "$I/$nome")" = "$soma" ] && echo "  ok  $nome" || { echo "  SHA-256 DIFERENTE: $nome"; exit 1; }
done <<'EOF'
kernel.tar.gz      44829c9f8032b04c3049fb9e053435bc84800832f4c83bcceb85e23568fbc63e https://github.com/bazzonrobocon/t113-x-tina-linux-5.4/archive/dfb63723b8fa0d22459825577f975749011e916e.tar.gz
backports.tar.xz   05f2c39f1be0aab49014a6e27a9492ce7ed0491f552db8a07f3ba0de33b9317d https://cdn.kernel.org/pub/linux/kernel/projects/backports/stable/v6.1.145/backports-6.1.145-1.tar.xz
walnutpi.tar.gz    f29780c4544783bd9fbaa464eba584a6390f1d106966d2e88c434e78be422393 https://github.com/walnutpi/aic8800_sdio/archive/ed6f69e9dd0f709e026dc9d1d9b78258739f935e.tar.gz
mt7961_patch.bin   a276c06c2b772adb50b86639d33c82824ff4c21d617feb78caea74c040b873f6 https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/mediatek/WIFI_MT7961_patch_mcu_1_2_hdr.bin
mt7961_ram.bin     b94217a951518a9c14095765f367bc5dd7698f2dc033941d6f18fc2ebd6a2ab9 https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/mediatek/WIFI_RAM_CODE_MT7961_1.bin
regulatory.db      7e236caecd939c8ec98be4870bf30422f28ffef2565a38aaaa2d9ddabd0c2641 https://git.kernel.org/pub/scm/linux/kernel/git/wens/wireless-regdb.git/plain/regulatory.db
regulatory.db.p7s  50332f0db09b8bcc719235ec4985c27377f2cc2f42abb7b5dcf951866c5a888a https://git.kernel.org/pub/scm/linux/kernel/git/wens/wireless-regdb.git/plain/regulatory.db.p7s
fluidd.zip         71b29320c59d185a52a06250a17ed494a471ea163d1d9c4a821b9cb4bf57dd97 https://github.com/fluidd-core/fluidd/releases/download/v1.38.0/fluidd.zip
lvgl.tar.gz        83b7325a4b78cbb19a39f3ac75346c8086926d7faeab5ff2d825c20da0bbdb46 https://github.com/lvgl/lvgl/archive/refs/tags/v8.3.11.tar.gz
cJSON.c            75c51de8fa40ac9d7a99319c6330719bd692eb81c0a869265f3d4c682533f9b9 https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.c
cJSON.h            0578cc29132912edbc88f83207a8fc76e5db3db0605497e909a9384ef3cc474b https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.h
EOF
# os arquivos de firmware/regdb mudam no upstream com o tempo: se o SHA não bater, confira a versão antes de aceitar.

echo "LVGL 8.3.11 e cJSON 1.7.18 -> hi-painel/lib/"
rm -rf hi-painel/lib; mkdir -p hi-painel/lib/lvgl hi-painel/lib/cjson
tar xzf $I/lvgl.tar.gz -C hi-painel/lib/lvgl --strip-components=1
cp $I/cJSON.c $I/cJSON.h hi-painel/lib/cjson/

echo "Fluidd 1.38.0 -> pacote/fluidd/fluidd"
rm -rf pacote/fluidd/fluidd; mkdir -p pacote/fluidd/fluidd
(cd pacote/fluidd/fluidd && unzip -q ../../../$I/fluidd.zip)

OTA=$I/CR4NU200360C20_ota_img_V1.1.0.74.img
OTA_SHA=e58fdf4922cb65220a0b4e760e8d31e23eaae25587141b36eaac6f56a33333b6
if [ ! -f "$OTA" ]; then
    echo ""; echo "FALTA a OTA oficial da Creality Hi 1.1.0.74: baixe no site da Creality e salve como $OTA"; exit 1
fi
[ "$(sha "$OTA")" = "$OTA_SHA" ] && echo "  ok  OTA 1.1.0.74 oficial" || { echo "  a OTA não é a 1.1.0.74 esperada (SHA-256 diferente)"; exit 1; }
rm -rf $I/ota-extraido; mkdir -p $I/ota-extraido
(cd $I/ota-extraido && cpio -id --quiet < ../CR4NU200360C20_ota_img_V1.1.0.74.img)
ls $I/ota-extraido
echo "insumos prontos. Próximo: docker compose run --rm kbuild"
