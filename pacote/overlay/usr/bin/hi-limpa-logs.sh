#!/bin/sh
# Projeto Hi: limita os logs do Klipper/Moonraker por tamanho e quantidade.
L=/mnt/UDISK/printer_data/logs
[ -d "$L" ] || exit 0
# rotações diárias: manter só as 3 mais novas de cada
for base in klippy.log moonraker.log; do
    ls -1t "$L/$base".* 2>/dev/null | tail -n +4 | xargs -r rm -f
done
# qualquer log rotacionado acima de 50 MB é truncado no fim (mantém os últimos 20 MB)
for f in "$L"/klippy.log.* "$L"/moonraker.log.*; do
    [ -f "$f" ] || continue
    sz=$(wc -c < "$f")
    [ "$sz" -gt 52428800 ] && { tail -c 20971520 "$f" > "$f.tmp" && mv -f "$f.tmp" "$f"; }
done
# logs da Creality
find /mnt/UDISK/creality/userdata/log -type f -mtime +7 -delete 2>/dev/null
sync
