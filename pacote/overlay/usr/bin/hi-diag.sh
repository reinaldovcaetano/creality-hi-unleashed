#!/bin/sh
# Projeto Hi: grava um diagnóstico completo na pasta de logs do Klipper,
# que o Moonraker/Fluidd já mostra (Máquina > Logs). Só lê, não muda nada.
L=/mnt/UDISK/printer_data/logs
OUT=$L/hi-diag.txt
[ -d "$L" ] || exit 0
[ -f "$OUT" ] && mv -f "$OUT" "$L/hi-diag-anterior.txt"
s() { echo; echo "######## $*"; sh -c "$*" 2>&1; }
{
echo "Projeto Hi - diagnóstico - $(date) - $(cat /etc/hi-versao 2>/dev/null)"
s 'fw_printenv version board boot_partition root_partition bootcmd bootdelay swu_mode parts_clean'
s 'cat /etc/openwrt_release; uname -a; uptime'
s 'free; cat /proc/swaps; cat /sys/block/zram0/comp_algorithm /sys/block/zram0/disksize 2>/dev/null'
s 'df -h; mount'
s 'cat /sys/block/mmcblk0/device/life_time /sys/block/mmcblk0/device/pre_eol_info /sys/block/mmcblk0/device/name 2>/dev/null'
s 'ls -l /dev/by-name/'
s 'lsmod'
s 'ps w'
s 'top -b -n1 | head -25'
s 'ls /etc/rc.d/'
s 'ip addr; ip route; cat /proc/net/wireless'
s 'iw dev; iw reg get; iw list | grep -E "Band|MHz|Wiphy"'
s 'for d in /sys/bus/usb/devices/*; do [ -f $d/idVendor ] && echo "$(basename $d) $(cat $d/idVendor):$(cat $d/idProduct) $(cat $d/manufacturer 2>/dev/null) $(cat $d/product 2>/dev/null)"; done'
s 'ls -l /dev/video* /dev/ttyS* /dev/input/* 2>/dev/null; for v in /sys/class/video4linux/*; do echo "$(basename $v) $(cat $v/name)"; done'
s 'cat /sys/class/thermal/thermal_zone*/temp; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq'
s 'echo "--- camada gravável (overlay/upper), arquivos:"; find /overlay/upper -xdev \( -type f -o -type c -o -type l \) 2>/dev/null | head -400; du -sh /overlay/upper 2>/dev/null'
s 'echo "--- dropbear"; ps w | grep -i [d]ropbear; netstat -tlnp 2>/dev/null | grep -E ":22|:2222"; ls -la /etc/dropbear; head -3 /etc/passwd; grep root /etc/shadow | cut -c1-20; ls -la /root 2>/dev/null'
s 'logread | tail -150'
s 'dmesg'
s 'crontab -l'
s 'ls -la /mnt/UDISK/printer_data/config/ /mnt/UDISK/printer_data/logs/'
s 'cat /tmp/mcu_update.log 2>/dev/null | tail -20'
} > "$OUT" 2>&1
sync
