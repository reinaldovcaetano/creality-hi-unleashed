# Cenários (rodam no busybox ARM). Sai com erro se algum resultado não for o esperado.
set -e
R=/v/r; ROM=$R/usr/share/klipper/config/F018_CR4NU200360C20; FAB=/fab; FALHAS=0
ok()   { echo "  OK   $1"; }
erro() { echo "  ERRO $1"; FALHAS=$((FALHAS+1)); }
mount -t proc proc $R/proc 2>/dev/null || true
echo "=== 1: hi-limpa-overlay (sobras escondendo arquivos do pacote)"
U=$R/overlay/upper; mkdir -p $U; mount -t tmpfs tmpfs $U; mkdir -p $R/tmp; mount -t tmpfs tmpfs $R/tmp
mkdir -p $U/etc/init.d $U/usr/share/moonraker-hi $U/etc/config $U/etc/modules.d
echo velho > $U/etc/init.d/hi-confirma; echo velho > $U/usr/share/moonraker-hi/moonraker-hi.conf
echo minha-rede > $U/etc/config/network; mknod $U/etc/init.d/hi-wifi c 0 0; mknod $U/etc/modules.d/touchscreen-tlsc6x c 0 0
chroot $R /bin/sh -c '. /etc/init.d/hi-limpa-overlay; start'; sleep 1
for p in etc/init.d/hi-confirma usr/share/moonraker-hi/moonraker-hi.conf etc/init.d/hi-wifi; do [ -e $U/$p ] && erro "ficou $p" || ok "removido $p"; done
for p in etc/config/network etc/modules.d/touchscreen-tlsc6x; do [ -e $U/$p ] && ok "preservado $p" || erro "sumiu $p"; done
echo velho > $U/etc/init.d/hi-confirma; chroot $R /bin/sh -c '. /etc/init.d/hi-limpa-overlay; start'
[ -e $U/etc/init.d/hi-confirma ] && ok "2a execução na mesma versão não mexe" || erro "2a execução mexeu"
echo "=== 2: hi-ajustes (só troca config de fábrica)"
mkdir -p $R/mnt/UDISK; mount -t tmpfs tmpfs $R/mnt/UDISK
C=$R/mnt/UDISK/printer_data/config; mkdir -p $C
cp $FAB/v1.1.0.74/gcode_macro.cfg $C/; cp $FAB/v1.1.0.50/box.cfg $C/; echo "# Ajuste meu" > $C/sensorless.cfg
cp $ROM/motor_control.cfg $C/; cp $ROM/printer.cfg $C/printer.cfg
printf '\n#*# <---------------------- SAVE_CONFIG ---------------------->\n' >> $C/printer.cfg
chroot $R /bin/sh -c '. /etc/init.d/hi-ajustes; aplicar'
cmp -s $C/gcode_macro.cfg $ROM/gcode_macro.cfg && ok "gcode_macro de fábrica .74 trocado" || erro "gcode_macro"
cmp -s $C/box.cfg $ROM/box.cfg && ok "box de fábrica .50 trocado" || erro "box"
grep -q "Ajuste meu" $C/sensorless.cfg && ok "sensorless do usuário mantido" || erro "sensorless"
cmp -s $C/printer_params.cfg $ROM/printer_params.cfg && ok "printer_params que faltava instalado" || erro "printer_params"
grep -q SAVE_CONFIG $C/printer.cfg && ok "printer.cfg com calibração mantido" || erro "printer.cfg"
echo "=== 3: scripts do sistema passam no sh -n do busybox da imagem"
for s in hi-ajustes hi-diag hi-limpa-overlay hi-zram hi-confirma hi-wifi moonraker; do
  chroot $R /bin/sh -n /etc/init.d/$s && ok "sh -n $s" || erro "sh -n $s"; done
umount $R/mnt/UDISK $U $R/tmp 2>/dev/null || true; umount $R/proc 2>/dev/null || true
echo "falhas: $FALHAS"; [ $FALHAS = 0 ]
