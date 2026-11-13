# Pacote 1a: "Hi Unleashed p1a" (versão OTA 1.1.0.80)

- **Gerado em 12/11/2026, 2ª montagem.** Arquivo: `pacote/saida/CR4NU200360C20_ota_img_V1.1.0.80.img`, 125.829.632 bytes, SHA-256 `b392e6372467e9a4009ae9a592b3c0a222106a3bd3736c7cc9b884c19486cb81`.

> **Por que houve uma 2ª montagem:** a 1ª tentativa no pendrive travou em 0%. A tela entrega a instalação ao `upgrade-server`, que estava vazio na .60, e por isso nada foi gravado. **O `upgrade-server` não acessa a internet**: não tem lib de nuvem nem HTTP. Quem fala com a nuvem da Creality é só o `app-server` (libcrealitycloud). Por isso, agora:
> - o `upgrade-server` fica **original**, e assim as próximas instalações por pendrive funcionam;
> - o `app-server` fica vazio;
> - os domínios `*.crealitycloud.*` vão para o `/etc/hosts` apontando para 0.0.0.0.
- **Para reconstruir:** `pacote/build.sh 1.1.0.80 "Hi Unleashed p1a"`. Usa o Docker `hi-pack` e o volume `hi-pack`. O script de dentro do container é `pacote/build/dentro.sh` e os nossos arquivos ficam em `pacote/overlay/`.

## O que tem dentro

- **Base:** o firmware oficial **1.1.0.74**. O kernel e o uboot são os originais, sem mudança; o uboot é o mesmo que a atualização oficial grava.
- **Kernel:** o original, sem troca. Só módulos novos, todos com vermagic idêntico e todos os símbolos atendidos:
  - `r8152.ko`: adaptador de rede USB RTL8153; pega IP por DHCP sozinho (`/etc/hotplug.d/net/30-hi-cabo`);
  - `zsmalloc.ko` + `zram.ko`: **swap comprimido de 256 MB na RAM**, com lzo e prioridade 100 (`/etc/init.d/hi-zram`).
- **Klipper:**
  - a correção do **spam "set enabled 1"**: `filament_switch_sensor.py` só registra no log quando o estado muda;
  - **configs "de fábrica" trocadas pelas nossas** em `/usr/share/klipper/config/F018_CR4NU200360C20/`:
    - `printer.cfg` com a **Version v1.0.80**. O init do Klipper vê que a versão mudou, copia o nosso e **mantém o bloco SAVE_CONFIG** (PID, malha, shaper). A Creality faz backup em `printer.cfg.1…5`;
    - `motor_control.cfg` da .74 (**prt_track_max_err 0.3**) com a Version v1.0.1, para ser copiado;
    - `gcode_macro.cfg`, `sensorless.cfg`, `box.cfg` e `printer_params.cfg`: os atuais da Hi. A Creality só copia esses se não existirem no UDISK, e os nossos já estão lá.
- **Nuvem e atualização automática:** `app-server` vazio (original em `app-server.orig`) e os domínios da nuvem Creality bloqueados no `/etc/hosts`. O `upgrade-server` fica **original**, porque é ele que instala pelo pendrive. ADB desligado.
- **`/etc/init.d/hi-ajustes` (S54, antes do Klipper):**
  - na 1ª inicialização de cada versão (marca `config/.hi-aplicado-<versão>`), instala as nossas `gcode_macro`, `sensorless`, `box`, `motor_control` e `printer_params`, com backup em `config/backup-antes-hi-*`;
  - se o `printer.cfg` não tiver calibração, monta o de fábrica Hi + a **calibração salva de 12/11** (`/usr/share/hi/save_config-2026-11-12.txt`: PID, shaper, malha);
  - se já tiver calibração, mantém;
  - log em `logs/hi-ajustes.log`.
  - Testado em 3 cenários (UDISK vazio, 2ª inicialização, configs de fábrica com calibração nova) num ARM emulado.
- **Diagnóstico:** `/etc/init.d/hi-diag` + `/usr/bin/hi-diag.sh`.
  - 90 segundos e 10 minutos depois do boot, grava **`hi-diag.txt`** na pasta de logs. Contém: env do U-Boot, memória e swap, disco, saúde do eMMC, módulos, processos, rede e Wi-Fi, USB, câmera, **a lista da camada gravável (overlay)**, dropbear, logread, dmesg, cron e configs.
  - Fica visível no Fluidd, em Máquina > Logs, ou em `http://<IP-DA-HI>:7125/server/files/logs/hi-diag.txt`.
- **SSH de reserva na porta 2222:** login com a **chave SSH configurada no build** (`pacote/local/authorized_keys`) ou com a senha. Tem log detalhado em `hi-ssh2222.log`, para descobrir por que a porta 22 trava.
- **Logs:** cron às 4h30 que roda `/usr/bin/hi-limpa-logs.sh` (mantém 3 rotações e corta acima de 50 MB). Versão registrada em `/etc/hi-versao`.

## Validação feita antes de entregar

- Estrutura idêntica à oficial: cpio `070702` (crc), com os mesmos 5 itens na mesma ordem, squashfs gzip com bloco de 256 KB e MD5 recalculado.
- Rodando o Python 3.9 e o busybox da própria imagem num ARM emulado (Docker arm/v7 + chroot):
  - `filament_switch_sensor.py` compila;
  - todos os scripts passam no `sh -n`.

## O que ainda **não** está neste pacote (fica para o 1b)

- Limpeza seletiva da camada gravável. **Ela sobrevive à atualização**, e é lá que estão a senha do Wi-Fi e os ajustes da .60. Antes de limpar, ver o que há nela pelo `hi-diag.txt`.
- Moonraker e Fluidd novos, câmera no Fluidd, KAMP e timelapse.
- O mecanismo de tentativa única no bootloader, para o kernel próprio.

## Como instalar

1. Pendrive em **FAT32**, com o arquivo `CR4NU200360C20_ota_img_V1.1.0.80.img` **na raiz**, sem renomear.
2. Impressora ligada e parada. Colocar o pendrive. A tela deve oferecer a atualização: confirmar.
3. Não desligar durante a gravação. Ela reinicia sozinha.
4. Depois de uns 3 minutos, conferir:
   - a versão na tela: 1.1.0.80;
   - o Fluidd abrindo;
   - o `hi-diag.txt` na pasta de logs.

**Na Hi atual (.60), a instalação trava em 0%** por causa do `upgrade-server` vazio, que está na camada gravável. É preciso **antes** fazer o reset `wipe_all`. Ele apaga a camada gravável (Wi-Fi incluído) e o UDISK, exceto `/mnt/UDISK/creality`, onde fica o banco de materiais. As configs e a calibração voltam pelo `hi-ajustes`, e o backup delas está em `backup-udisk-2026-11-12/`. Os 5 G-codes da impressora eram exemplos da Creality.

**Antes, o texto dizia: se a tela não oferecer a atualização:** o `upgrade-server` vazio da .60 pode estar bloqueando. A saída é o reset por pendrive (`wipe_all`, ver `docs/recuperacao-usb.md`). Ele limpa a camada gravável, mas **apaga também o Wi-Fi, as configs e o banco de materiais do UDISK**. As configs voltam pelo pacote; o Wi-Fi é refeito pela tela.

**Se algo der errado depois de instalar:** a metade anterior (a .60 atual) continua na outra partição. A volta é trocar `boot_partition`, ou, em último caso, regravar de fábrica pelo PhoenixSuit com o cabo Micro USB.
