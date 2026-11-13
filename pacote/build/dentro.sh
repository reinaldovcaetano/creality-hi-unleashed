#!/bin/bash
# Projeto Hi: monta o pacote OTA (roda dentro do container hi-pack, como root).
# Entradas: /base (ota-extraido da 1.1.0.74), /pacote (overlay + build), variáveis VERSAO e ROTULO.
set -euo pipefail
W=/work
VERSAO=${VERSAO:?}
ROTULO=${ROTULO:?}
rm -rf $W/rootfs $W/out; mkdir -p $W/out

echo "[1/7] extraindo rootfs da 1.1.0.74"
unsquashfs -q -n -d $W/rootfs /base/rootfs >/dev/null
F=$W/rootfs

echo "[2/7] copiando os nossos arquivos (overlay)"
cp -a /pacote/overlay/. $F/
chown -R 0:0 $F
chmod 755 $F/etc/init.d/hi-* $F/usr/bin/hi-*.sh $F/etc/hotplug.d/net/30-hi-cabo
# Dados pessoais NÃO ficam no repositório: ficam em pacote/local/ (ignorado pelo git), ambos opcionais.
#  - pacote/local/authorized_keys : sua chave pública SSH (login sem senha na porta 2222)
#  - pacote/local/save_config.txt : bloco SAVE_CONFIG da SUA impressora (PID/shaper/malha) para restaurar após reset
if [ -f /pacote/local/authorized_keys ]; then
  mkdir -p $F/etc/dropbear; cp /pacote/local/authorized_keys $F/etc/dropbear/authorized_keys
  chown -R 0:0 $F/etc/dropbear; chmod 700 $F/etc/dropbear; chmod 600 $F/etc/dropbear/authorized_keys
  echo "  chave SSH incluída (pacote/local/authorized_keys)"
else
  echo "  sem pacote/local/authorized_keys: SSH 2222 só com a senha root da Creality"
fi
if [ -f /pacote/local/save_config.txt ]; then
  mkdir -p $F/usr/share/hi; cp /pacote/local/save_config.txt $F/usr/share/hi/save_config.txt; chown 0:0 $F/usr/share/hi/save_config.txt
  echo "  calibração salva incluída (pacote/local/save_config.txt)"
fi
chmod 644 $F/lib/modules/5.4.61/*.ko

echo "[3/7] correção do spam do sensor de filamento (log só quando muda)"
python3 - "$F/usr/share/klipper/klippy/extras/filament_switch_sensor.py" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
old="""        logging.info(f'Filament Sensor {self.name}: set enabled {gcmd.get_int("ENABLE", 1)}')
        self.sensor_enabled = gcmd.get_int("ENABLE", 1)"""
new="""        # Ajuste Projeto Hi: o box_wrapper repete este comando 1x/s; so registra quando muda
        novo = gcmd.get_int("ENABLE", 1)
        if novo != self.sensor_enabled:
            logging.info(f'Filament Sensor {self.name}: set enabled {novo}')
        self.sensor_enabled = novo"""
assert s.count(old)==1, "trecho do filament_switch_sensor nao encontrado"
open(p,'w').write(s.replace(old,new))
PY
rm -f $F/usr/share/klipper/klippy/extras/filament_switch_sensor.pyc $F/usr/share/klipper/klippy/extras/__pycache__/filament_switch_sensor.*

echo "[4/7] nuvem desligada (app-server vazio + domínios bloqueados), ADB desligado"
# upgrade-server fica ORIGINAL: é ele que instala pelo pendrive e não acessa a internet.
# Quem fala com a nuvem/OTA online é o app-server (libcrealitycloud).
mv $F/usr/bin/app-server $F/usr/bin/app-server.orig
printf '#!/bin/sh\n# Projeto Hi: nuvem desligada (original em /usr/bin/app-server.orig)\nwhile true; do sleep 3600; done\n' > $F/usr/bin/app-server
chmod 755 $F/usr/bin/app-server
{
  echo ""
  echo "# Projeto Hi: nuvem Creality bloqueada"
  for d in api.crealitycloud.com api.crealitycloud.cn mqtt.crealitycloud.com mqtt.crealitycloud.cn \
           admin-pre.crealitycloud.com admin-pre.crealitycloud.cn api-dev.crealitycloud.cn \
           pre-tb-iot.crealitycloud.com www.crealitycloud.cn www.crealitycloud.com; do
      echo "0.0.0.0 $d"
  done
} >> $F/etc/hosts
rm -f $F/etc/rc.d/S80adbd $F/etc/rc.d/K99adbd
# Ajuste 13/11: a Hi de referência tem touch Goodix (gt9xxnew_ts); o tlsc6x só falhava no probe a cada boot.
# O .ko continua em /lib/modules (para religar: echo tlsc6x > /etc/modules.d/touchscreen-tlsc6x).
rm -f $F/etc/modules.d/touchscreen-tlsc6x

echo "[4b/7] Fluidd 1.38.0, nginx (câmera em /camera/ e /call/) e Moonraker novo"
rm -rf $F/usr/share/fluidd && cp -a /pacote/fluidd/fluidd $F/usr/share/fluidd && chown -R 0:0 $F/usr/share/fluidd
python3 - "$F/etc/nginx/nginx.conf" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
bloco = """        # Projeto Hi: camera WebRTC da Creality (porta 8000) dentro do Fluidd
        location /camera/ {
            proxy_pass http://127.0.0.1:8000/;
        }
        location /call/ {
            proxy_pass http://127.0.0.1:8000/call/;
        }

        location /webcam/ {"""
assert s.count("        location /webcam/ {")==1, "nginx: location /webcam/ nao encontrado"
open(p,'w').write(s.replace("        location /webcam/ {", bloco))
PY
chmod 755 $F/etc/init.d/moonraker
find $F/usr/share/moonraker-hi -type d -exec chmod 755 {} \; ; find $F/usr/share/moonraker-hi -type f -exec chmod 644 {} \;

if [ "${P2:-0}" = 1 ]; then
echo "[4c/7] PACOTE 2: kernel B (backports 6.1), módulos, firmwares, hi-wifi e hi-confirma"
cp -a /pacote/overlay-p2/. $F/
rm -f $F/etc/modules.d/net-aic8800
mv $F/lib/firmware/aic8800dc $F/lib/firmware/aic8800dc.creality
cp -a /pacote/p2/firmware/aic8800dc $F/lib/firmware/aic8800dc
mkdir -p $F/lib/firmware/mediatek
cp -a /pacote/p2/firmware/mediatek/. $F/lib/firmware/mediatek/
cp -a /pacote/p2/firmware/regulatory.db /pacote/p2/firmware/regulatory.db.p7s $F/lib/firmware/
mkdir -p $F/lib/modules/5.4.61.creality
cp -a $F/lib/modules/5.4.61/aic8800_bsp.ko $F/lib/modules/5.4.61/aic8800_fdrv.ko $F/lib/modules/5.4.61.creality/
cp -a /pacote/p2/modulos/. $F/lib/modules/5.4.61/
chown -R 0:0 $F/lib/firmware $F/lib/modules $F/etc/init.d/hi-wifi $F/etc/init.d/hi-confirma $F/etc/modules.d
chmod 755 $F/etc/init.d/hi-wifi $F/etc/init.d/hi-confirma; chmod 644 $F/lib/modules/5.4.61/*.ko
ln -sf ../init.d/hi-wifi $F/etc/rc.d/S19hi-wifi
ln -sf ../init.d/hi-confirma $F/etc/rc.d/S99hi-confirma
ls $F/lib/modules/5.4.61 | wc -l
fi

if [ "${TENTATIVA:-0}" = 1 ] && [ "${P2:-0}" != 1 ]; then
echo "[4d/7] tentativa única com o kernel original: só o hi-confirma"
cp -a /pacote/overlay-p2/etc/init.d/hi-confirma $F/etc/init.d/hi-confirma; chown 0:0 $F/etc/init.d/hi-confirma; chmod 755 $F/etc/init.d/hi-confirma
ln -sf ../init.d/hi-confirma $F/etc/rc.d/S99hi-confirma
fi

echo "[4e/7] Hi Painel (tela própria)"
if [ -x /pacote/painel/hi-painel ]; then
  install -m 755 /pacote/painel/hi-painel $F/usr/bin/hi-painel
  chmod 755 $F/etc/init.d/hi-painel
  ln -sf ../init.d/hi-painel $F/etc/rc.d/S99hi-painel
  ln -sf ../init.d/hi-painel $F/etc/rc.d/K05hi-painel
  echo "  hi-painel $(wc -c < /pacote/painel/hi-painel) bytes"
else
  rm -f $F/etc/init.d/hi-painel
  echo "  sem pacote/painel/hi-painel: pacote sai sem a tela própria"
fi

echo "[5/7] serviços, cron e versão"
ln -sf ../init.d/hi-diag $F/etc/rc.d/S99hi-diag
ln -sf ../init.d/hi-ajustes $F/etc/rc.d/S54hi-ajustes
ln -sf ../init.d/hi-limpa-overlay $F/etc/rc.d/S11hi-limpa-overlay
ln -sf ../init.d/hi-zram $F/etc/rc.d/S21hi-zram
ln -sf ../init.d/hi-zram $F/etc/rc.d/K90hi-zram
mkdir -p $F/etc/crontabs
echo '30 4 * * * /usr/bin/hi-limpa-logs.sh' > $F/etc/crontabs/root
echo "$ROTULO (base Creality 1.1.0.74, versao OTA $VERSAO) - build $(date -u +%Y-%m-%dT%H:%MZ)" > $F/etc/hi-versao
echo "$VERSAO" > $F/etc/hi-versao-ota

echo "[5b/7] MD5 das configs de fábrica (hi-ajustes) e lista de arquivos do pacote (hi-limpa-overlay)"
mkdir -p $F/usr/share/hi
: > $F/usr/share/hi/fabrica.md5
for d in /pacote/fabrica/*; do
  for f in gcode_macro.cfg sensorless.cfg box.cfg motor_control.cfg printer_params.cfg; do
    [ -f "$d/$f" ] && echo "$(md5sum "$d/$f" | cut -d' ' -f1)  $f" >> $F/usr/share/hi/fabrica.md5
  done
done
sort -u -o $F/usr/share/hi/fabrica.md5 $F/usr/share/hi/fabrica.md5
wc -l < $F/usr/share/hi/fabrica.md5
rm -rf $W/base-ro; unsquashfs -q -n -d $W/base-ro /base/rootfs >/dev/null
python3 - $F $W/base-ro > $F/usr/share/hi/arquivos-do-pacote.txt <<'PY'
import os, sys, filecmp
novo, base = sys.argv[1], sys.argv[2]
for raiz, dirs, arqs in os.walk(novo):
    for a in arqs:
        p = os.path.join(raiz, a); r = os.path.relpath(p, novo); b = os.path.join(base, r)
        if r.startswith("usr/share/hi/arquivos-do-pacote"): continue
        if os.path.islink(p):
            if not (os.path.islink(b) and os.readlink(b) == os.readlink(p)): print(r)
        elif not os.path.isfile(b) or os.path.islink(b) or not filecmp.cmp(p, b, shallow=False):
            print(r)
PY
echo "arquivos do pacote (novos ou alterados): $(wc -l < $F/usr/share/hi/arquivos-do-pacote.txt)"
rm -rf $W/base-ro

echo "[6/7] gerando rootfs (squashfs gzip, bloco 256K, igual ao original)"
mksquashfs $F $W/out/rootfs -comp gzip -b 262144 -all-root -noappend -no-progress -quiet
ls -la $W/out/rootfs

echo "[7/7] sw-description, MD5 e cpio"
cp /base/uboot $W/out/
if [ "${P2:-0}" = 1 ]; then cp /pacote/p2/kernel-B.img $W/out/kernel; else cp /base/kernel $W/out/; fi
sed "s/\"1\.1\.0\.74\"/\"$VERSAO\"/g" /base/sw-description > $W/out/sw-description
if [ "${TENTATIVA:-0}" = 1 ]; then python3 /pacote/build/tentativa.py $W/out/sw-description; fi
grep -c "\"$VERSAO\"" $W/out/sw-description
cd $W/out
for f in sw-description uboot kernel rootfs; do printf '%s  %s\n' "$(md5sum $f | cut -d' ' -f1)" "$f"; done > cpio_item_md5
cat cpio_item_md5
NOME=CR4NU200360C20_ota_img_V$VERSAO.img
printf 'sw-description\nuboot\nkernel\nrootfs\ncpio_item_md5\n' | cpio -o -H crc --quiet > /saida/$NOME
cpio -t -v < /saida/$NOME 2>/dev/null
ls -la /saida/$NOME; sha256sum /saida/$NOME
