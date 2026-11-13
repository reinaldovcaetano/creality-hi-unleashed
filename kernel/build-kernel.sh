#!/bin/bash
# Creality Hi Unleashed: compila o kernel próprio, a pilha Wi-Fi (backports 6.1 + mt7921u) e o AIC8800.
# Roda DENTRO do container "kbuild" (docker compose run --rm kbuild).
#   /repo   = este repositório
#   /work   = volume Docker (o código do kernel precisa de disco que diferencia maiúsculas/minúsculas)
# Saída:  pacote/p2/kernel-B.img, pacote/p2/modulos/*.ko, pacote/p2/firmware/
set -euo pipefail
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
J=$(nproc)
R=/repo; I=$R/insumos; W=/work; OUT=$R/pacote/p2
BASE=$I/ota-extraido; BASEROOT=$W/base-rootfs

for f in kernel.tar.gz backports.tar.xz walnutpi.tar.gz ota-extraido/kernel ota-extraido/rootfs; do
    [ -e "$I/$f" ] || { echo "faltando insumos/$f: rode scripts/obter-insumos.sh antes"; exit 1; }
done

echo "[1/6] código do kernel (Tina 5.4.61) + patches do projeto"
rm -rf $W/linux $W/out-b; mkdir -p $W/linux $W/out-b
tar xzf $I/kernel.tar.gz -C $W/linux --strip-components=1
(cd $W/linux && patch -p1 -s < $R/kernel/patches/linux-hi.patch)
cp $R/kernel/config/kernel-b.config $W/out-b/.config
make -s -C $W/linux O=$W/out-b olddefconfig
make -s -C $W/linux O=$W/out-b -j$J zImage modules 2>&1 | grep -E "error|Error" || true
test -f $W/out-b/arch/arm/boot/zImage
arm-linux-gnueabi-strings $W/out-b/vmlinux | grep -q "2.6.5-hi1" && echo "  driver I2C 2.6.5-hi1 presente"

echo "[2/6] conferência dos pinos: device tree da Creality x tabela do kernel"
python3 $R/kernel/tools/bootimg.py extrair-dtb $BASE/kernel $W/creality.dtb
$W/out-b/scripts/dtc/dtc -q -I dtb -O dts -o $W/creality.dts $W/creality.dtb
python3 $R/kernel/tools/conferir_pinos.py $W/creality.dts $W/linux/drivers/pinctrl/sunxi/pinctrl-sun8iw20.c

echo "[3/6] linux-backports 6.1.145 (cfg80211, mac80211, mt76, mt7921u)"
rm -rf $W/backports; mkdir -p $W/backports
tar xJf $I/backports.tar.xz -C $W/backports --strip-components=1
cp $R/kernel/config/backports.config $W/backports/.config
make -s -C $W/backports KLIB_BUILD=$W/out-b KLIB=$W/out-b olddefconfig >/dev/null
make -s -C $W/backports KLIB_BUILD=$W/out-b KLIB=$W/out-b -j$J 2>&1 | grep -E "error|Error" || true

echo "[4/6] AIC8800DC (Wi-Fi interno): walnutpi + porte para o cfg80211 6.1, plataforma Allwinner"
rm -rf $W/walnutpi $W/aicw; mkdir -p $W/walnutpi $W/aicw
tar xzf $I/walnutpi.tar.gz -C $W/walnutpi --strip-components=1
cp -a $W/walnutpi/src/aic8800_bsp $W/walnutpi/src/aic8800_fdrv $W/aicw/
(cd $W/aicw && patch -p1 -s < $R/kernel/patches/aic8800-walnutpi-hi.patch)
make -s -C $W/out-b M=$W/aicw BACKPORTS=$W/backports KBUILD_EXTRA_SYMBOLS=$W/backports/Module.symvers \
    CONFIG_AIC_WLAN_SUPPORT=y CONFIG_AIC8800_WLAN_SUPPORT=m CONFIG_AIC_INTF_SDIO=y CONFIG_ARCH_SUNXI=y \
    CONFIG_PLATFORM_ALLWINNER=y CONFIG_PLATFORM_UBUNTU=n CONFIG_AIC_FW_PATH="\"/lib/firmware/aic8800dc\"" \
    -j$J modules 2>&1 | grep -E "error|Error" || true

echo "[5/6] juntando módulos (strip --strip-debug) e firmwares"
rm -rf $OUT; mkdir -p $OUT/modulos $OUT/firmware/mediatek $OUT/firmware/aic8800dc
pega() { local f; f=$(find "$1" -name "$2.ko" | head -1); [ -n "$f" ] || { echo "módulo $2 não gerado"; exit 1; }
         arm-linux-gnueabi-strip --strip-debug -o $OUT/modulos/$2.ko "$f"; }
for m in zram zsmalloc r8152 aes_generic libaes ccm gcm ctr ghash-generic gf128mul cmac seqiv hmac drbg jitterentropy_rng sha1_generic; do pega $W/out-b $m; done
for m in compat cfg80211 mac80211 mt76 mt76-usb mt76-connac-lib mt7921-common mt7921u; do pega $W/backports $m; done
for m in aic8800_bsp aic8800_fdrv; do pega $W/aicw $m; done
# touch: módulos originais da Creality (não há fonte), copiados da própria OTA
rm -rf $BASEROOT; mkdir -p $BASEROOT
( cd $BASEROOT && unsquashfs -q -n -d r $BASE/rootfs lib/modules/5.4.61/gt9xxnew_ts.ko lib/modules/5.4.61/tlsc6x.ko lib/firmware/aic8800dc/aic_userconfig_8800dc.txt >/dev/null 2>&1 ) || true
for m in gt9xxnew_ts tlsc6x; do arm-linux-gnueabi-strip --strip-debug -o $OUT/modulos/$m.ko $BASEROOT/r/lib/modules/5.4.61/$m.ko; done
cp $I/mt7961_patch.bin $OUT/firmware/mediatek/WIFI_MT7961_patch_mcu_1_2_hdr.bin
cp $I/mt7961_ram.bin   $OUT/firmware/mediatek/WIFI_RAM_CODE_MT7961_1.bin
cp $I/regulatory.db $I/regulatory.db.p7s $OUT/firmware/
cp -a $W/walnutpi/src/fw/aic8800DC/. $OUT/firmware/aic8800dc/
cp $BASEROOT/r/lib/firmware/aic8800dc/aic_userconfig_8800dc.txt $OUT/firmware/aic8800dc/   # potência de RF da Creality
for k in $OUT/modulos/*.ko; do
    v=$(modinfo -F vermagic "$k"); [ "$v" = "5.4.61 SMP preempt mod_unload ARMv7 p2v8 " ] || [ "$v" = "5.4.61 SMP preempt mod_unload ARMv7 p2v8" ] || { echo "vermagic errado em $k: '$v'"; exit 1; }
done
echo "  $(ls $OUT/modulos | wc -l) módulos, vermagic conferido"

echo "[6/6] imagem de boot: zImage novo + device tree ORIGINAL da Creality"
python3 $R/kernel/tools/bootimg.py trocar $BASE/kernel $W/out-b/arch/arm/boot/zImage $OUT/kernel-B.img
python3 $R/kernel/tools/bootimg.py conferir $OUT/kernel-B.img | tail -1
sha256sum $OUT/kernel-B.img
echo "kernel pronto em pacote/p2/"
