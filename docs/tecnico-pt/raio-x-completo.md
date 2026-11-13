# Raio-x completo da Creality Hi

Feito em 12/11/2026. Fontes:
- a imagem de fábrica `t113_i_linux_CR4NU200360C20_uart0_V1.1.0.50.img`, desmontada em `creality-oficial/firmware-V1.1.0.50/uart0-extraido/`;
- o rootfs aberto em `rootfs-1.1.0.50/fs/`;
- o device tree descompilado em `rootfs-1.1.0.50/sunxi.dts`;
- a API do Moonraker da impressora, **só leitura**.

O extrator da imagem é `scripts/awimg.py`: `python3 scripts/awimg.py <img> <pasta>`.

## Hardware

| Parte | O que é |
|---|---|
| Processador principal | Allwinner **T113-i**, 2× Cortex-A7 (armv7l), mais um DSP HiFi4 (`dsp0`) |
| RAM | 512 MB (499.976 kB visíveis) |
| Armazenamento | eMMC SanDisk/iNAND "DG400" (`mmcblk0`), com tabela GPT |
| Rede | Só **Wi-Fi** (`wlan0`). O DTS tem um GMAC (`eth@4500000`) ativo, mas não há porta de rede em uso |
| Tela | LCD por `lcd_fb0` e `disp`; touch Goodix (`ctp@14`) ou TLSC6x (`0x2E`), no I2C |
| Câmera | Entrada de vídeo e ISP (`vinc`, `isp`, `scaler`) ativos, e USB host para câmera UVC. Na .74 também há o módulo `[steer]` para câmera no barramento 485 |
| USB | `usbc0` OTG (porta de gravação FEL, Micro USB) e `usbc1` host (pendrive e câmera) |
| LED | `sys-led` piscando com o trigger "timer" |
| **Console serial (debug)** | **UART0**, **PB0 = TX, PB1 = RX, 115200 8N1** (`console=ttyS0,115200`) |
| **JTAG** | Ativo em PH9 (MS), PH10 (CK), PH11 (DO) e PH12 (DI) |

### Microcontroladores (lidos na impressora)

| MCU | Chip | Firmware |
|---|---|---|
| `mcu` (placa-mãe) | GD32F303XE, 120 MHz | `cf1eac2d-dirty-20250210` |
| `nozzle_mcu` (cabeçote) | GD32F303XB, 120 MHz | `23f337b2` |
| `bed_mcu` (mesa e sensor de pressão) | GD32E230X8 | `54872883-dirty-20240605` |
| `rpi` (MCU virtual no Linux) | linux | `43221f50-dirty-20240224` |
| Motores X/Y em malha fechada | no barramento 485 (`0x81`/`0x82`) | arquivos `mot*_002_071.bin` |

## Memória interna (eMMC)

Os tamanhos vêm de `sys_partition.fex`, em setores de 512 bytes.

| Partição | Tamanho | Conteúdo |
|---|---|---|
| boot-resource | 8 MB | logos e recursos do boot |
| env / env-redund | 1 MB cada | variáveis do U-Boot (`boot_partition`, `swu_*`, `version`) |
| bootA / bootB | 16 MB cada | kernel **Linux 5.4.61** (imagem Android boot) |
| rootfsA / rootfsB | 300 MB cada | **SquashFS** somente leitura (OpenWrt/Tina) |
| dsp0 | 1 MB | firmware do DSP |
| private | 16 MB | dados de fábrica (chaves e números de série) |
| rootfs_data | 256 MB | overlay gravável do sistema (o que mudamos em `/usr/bin`, `/etc` etc.) |
| UDISK | o restante | `/mnt/UDISK`: configs, logs, G-codes e banco de materiais |

- **A/B:** o U-Boot inicia pela partição em `boot_partition`/`root_partition`. A atualização grava a outra metade e troca a variável no fim.
- **O que cada recuperação apaga:**
  - a gravação pela placa com "YES" regrava tudo;
  - o reset `wipe_all` só limpa o overlay e os dados.

## Sistema

- **OpenWrt 21.02 / Tina 5.0**, build `tina.113934.20250806` (glibc e busybox). Alvo `t113_i-CR4NU200360C20`.
- **Serviços na ordem de boot:**
  1. `sysfixtime`, `boot` e `system`;
  2. `log` e `fstab`;
  3. **`mcu_update`** (S13): grava o firmware das placas a partir de `/usr/share/klipper/fw/` quando a versão é diferente;
  4. `device_manager` e `dropbear` (S19);
  5. `board_init`: liga o 5 V da USB host e solta o reset dos MCUs;
  6. `network`, `cron`, `tee-supplicant`, `klipper_mcu`, **`klipper`** (S55), **`moonraker`** (S56), `dbus`, `adbd`, `nginx`, `gpio_switch`, `led`;
  7. **`wipe_data`** (S96): o reset por pendrive;
  8. `webrtc` (câmera), `sysntpd`, **`app`** (S99), `mdns`, **`swupdate_autorun`** (S99).
- **SSH:** dropbear com login de root por senha ligado. O hash do root está no `/etc/shadow` da imagem.
- **Python:** `klippy-env` e `moonraker-env` em `/usr/share`. Fluidd em `/usr/share/fluidd`.

### Programas fechados da Creality (`/usr/bin`)

| Programa | Tamanho | Papel |
|---|---|---|
| `display-server` | 11 MB | interface da tela |
| `log_main` | 6 MB | coleta e envio de logs |
| `yolo_attention` + `ai_engine` | 2,8 MB + 0,6 MB | visão por IA da câmera (detecção de falhas) |
| `error_code` | 1,5 MB | tabela e tratamento dos códigos de erro (key/CX…) |
| `master-server` | 1,1 MB | banco de materiais |
| `web-server` | 415 kB | serviço da porta 9999 (LAN e Creality Print) |
| `app-server` | 293 kB | nuvem (desligado na nossa) |
| `wifi-server` | 132 kB | Wi-Fi |
| `upgrade-server` | n/d | atualizações (desligado na nossa) |
| `mcu_util` / `mcu_util_485` | n/d | gravador do firmware das placas e dos motores |

Também vêm ferramentas de debug: `gdbserver`, `strace`, `htop`, `jq`, `rsync`, `7z` e `ffmpeg`.

## Klipper: o código aberto é o mesmo do firmware

Comparando `rootfs-1.1.0.50/fs/usr/share/klipper` com o repositório `Hi_Klipper`:
- `klippy/` é **igual**, tirando:
  - três objetos C compilados (`filament_change.o`, `msgblock_485.o` e `serial_485_queue.o`), recompilados;
  - 29 módulos genéricos do Klipper que a Creality tirou da imagem (bltouch, servo, display etc.).
- As configs do F018 e o `fw/F018` são **idênticos**. A imagem também traz os firmwares do F009 e do F010.
- **Conclusão:** dá para alterar o Klipper com segurança a partir do repositório e trocar arquivos `.py` na impressora, mantendo os `.so` fechados originais.
  - A versão `09faed31-dirty` é a mesma na .50, na .60 e na .74.
  - Na .60, que está instalada, já existe a macro `SET_PIN`.

## Atualizações (SWUpdate)

- **Fluxo:** o `upgrade-server` ou o pendrive (`/etc/ota_bin/local_ota_update.sh`) gravam as variáveis `swu_param`, `swu_software` e `swu_mode` no env do U-Boot. Depois o `/sbin/swupdate_cmd.sh` roda `swupdate -v ... -e <software>,<modo>`, com até 3 tentativas, e reinicia.
- **Assinatura:** o `swupdate` foi compilado com opção de chave pública, mas **os pacotes oficiais não são assinados**: a .74 só tem MD5 por item. Um `.img` OTA nosso seria aceito.
- O `mcu_update` atualiza sozinho as placas e os motores quando o `/usr/share/klipper/fw/` muda. É por isso que um pacote modificado pode regravar os MCUs.

## Caminhos para mexer, do mais seguro ao mais arriscado

1. **Pacote de correções** (scripts por SSH): não mexe no sistema.
2. **Trocar arquivos `.py` do Klipper** no overlay, a partir do repositório (por exemplo `filament_switch_sensor.py` para tirar o spam). Some com o reset ou com um update.
3. **OTA própria:** rootfs da .60 ou da .74 com os nossos ajustes, gerada com `mksquashfs` e empacotada no cpio com o `sw-description`. Sem assinatura. Para não arriscar o bootloader nem os MCUs, deixar de fora `uboot`, `boot0` e as mudanças em `fw/`.
4. **Imagem completa própria pelo PhoenixSuit:** só depois que a recuperação por cabo estiver testada.

**Rede de segurança, em camadas:**
1. A/B (a outra partição continua com o sistema anterior).
2. O reset `wipe_all`.
3. A gravação pela placa com PhoenixSuit.
4. A console serial em PB0/PB1, para ver o boot.

## Kernel e drivers (para uma imagem própria)

A config completa do kernel saiu de dentro do próprio kernel (IKCONFIG) e está em `docs/kernel-5.4.61-config-1.1.0.50.txt`.

- **Versão:** Linux 5.4.61 do pacote de suporte (BSP) da Allwinner, compilado com Linaro GCC 5.3. A Creality não publicou o código do kernel.
- **Módulos:** `CONFIG_MODULES=y`, **sem MODVERSIONS e sem assinatura de módulo**. Um `.ko` compilado fora da árvore carrega se o vermagic bater: `5.4.61 SMP preempt mod_unload ARMv7 p2v8`.
- **Wi-Fi atual:** AIC8800 (módulos `aic8800_bsp`/`aic8800_fdrv`, firmware aic8800D80/aic8800dc).
  - `CFG80211=y` embutido; **`MAC80211` não está ativo**, e `WEXT` também não.
  - Os dongles Realtek com driver do fabricante (8188EU/8188FU/8811CU/8821CU/88x2BU) usam só o cfg80211, então **são viáveis como módulo**.
  - Os MediaTek e Atheros (mt7601u, mt76, ath9k_htc) precisam do mac80211, que teria de ser compilado como módulo. Isso é mais arriscado.
- **Já embutido no kernel:**
  - rede USB **ASIX AX88772/AX88179** e CDC-Ethernet (adaptadores USB para cabo de rede);
  - `USB_STORAGE`, `USB_VIDEO_CLASS` (webcam UVC) e serial CH341.
- **Não tem:** Bluetooth e RTL8152 (adaptador de rede USB-C Realtek comum, que daria para fazer como módulo).
- **Python 3.9**, Fluidd v1.27.0 e Moonraker antigo, do tempo do Python 3.9.

### Dongle Wi-Fi testado em 12/11/2026: MediaTek MT7921AU (USB `0e8d:7961`)

- **Driver:** `mt7921u` (família mt76). Só existe no Linux **5.18 ou mais novo**, e precisa do **mac80211**. Firmware: `WIFI_MT7961_patch_mcu_1_2_hdr.bin` e `WIFI_RAM_CODE_MT7961_1.bin`.
- **Por que é difícil na Hi:** o kernel é o 5.4.61, com o cfg80211 **embutido** e **sem o mac80211**.
  - O caminho comum (linux-backports) traz o próprio cfg80211 como módulo, que entra em conflito com o embutido. O driver AIC8800 interno depende do embutido.
  - A alternativa seria compilar o mac80211 5.4 como módulo e portar o mt7921u para a API da 5.4: muito trabalho, sem garantia, porque o cfg80211 da Allwinner pode ter patches.
  - A outra é recompilar o kernel inteiro a partir de um SDK Tina T113 público: arriscado, porque tela, touch e Wi-Fi interno têm de continuar funcionando.
- **Alternativas viáveis:**
  - um dongle **Realtek** com driver do fabricante (RTL8821CU, RTL88x2BU, RTL8811CU), que só usa o cfg80211;
  - um adaptador de rede **ASIX AX88179/AX88772**, que já funciona hoje.

### Adaptador de rede USB testado em 12/11/2026: Realtek RTL8153 (USB `0bda:8153`, "USB 10/100/1000 LAN")

- **Driver ideal:** `r8152`. Existe no próprio Linux 5.4.61, mas **não foi compilado** (`# CONFIG_USB_RTL8152 is not set`).
  - É simples de compilar como módulo fora da árvore: só depende de `USB_USBNET`/`MII`/`CRC32`, que estão embutidos.
  - Não precisa de mac80211. Viável.
- **É provável que já funcione sem driver novo:** o RTL8153 tem uma segunda configuração USB em modo **CDC-ECM**, e o `cdc_ether` está embutido (`CONFIG_USB_NET_CDCETHER=y`).
  - O kernel 5.4 escolhe a configuração não proprietária quando existe uma, e sem o r8152 não há nada que bloqueie o cdc_ether.
  - O resultado seria uma interface `eth`/`usb` funcionando a até 1 Gb/s, com um pouco mais de CPU do que o r8152.
  - Testar plugando na Hi e vendo o `dmesg`/`ip link`, ou o `network` do `/machine/system_info` do Moonraker.
- **Na imagem própria:**
  - incluir o `r8152.ko`, carregado no boot, e o firmware `rtl_nic/rtl8153*.fw`, se houver;
  - criar a interface no `/etc/config/network` com DHCP.
  - **O `wifi-server` e a tela não sabem de rede por cabo.** A configuração precisa ir no sistema, e o IP passa a ser o que o roteador der para o adaptador.

## Firmware 1.1.0.74 aberto (12/11/2026)

- **Arquivo:** `creality-oficial/firmware-V1.1.0.74/` (SHA-256 confere). O rootfs está em `rootfs-1.1.0.74/fs/` e a lista de diferenças contra a .50 em `diferencas-rootfs-50-vs-74.txt` (380 itens).
- **Build:** `tina.116680.20260724`. O kernel foi recompilado, mas a **config é idêntica à da .50**: o código do kernel em `kernel/` serve para as duas. A config está em `kernel/config-creality-1.1.0.74`.
- **Código que vazou na .74 e não estava no GitHub:**
  - `klippy/chelper/msgblock_485.c` e `serial_485_queue.c`: **o transporte 485 completo em C**;
  - `filament_change.c`;
  - `extras/steer_wrapper.py`: **fonte Python de um wrapper Cython**, mais o `steer_wrapper.c` gerado. Mostra como a Creality escreve os módulos fechados.
- **Script interno de deploy da Creality:** `usr/share/klipper/klipper_auto_deploy.sh`, com usuário `root` e senha root padrão da Creality, que confere com a nossa.
- **Programas novos:** `strain_gauge_collect_data` (coleta do sensor de pressão da mesa), `pa_compute` (cálculo de pressure advance) e `kongda_detect`. Também a lib `libhvnew.so` e certificados em `/usr/share/cert`.
- **Firmwares novos:** CFS `cfs0_000_142` e uma pasta `fw/ccl`.
- **Fechados que mudaram:** `box_wrapper`, `motor_control_wrapper` e `serial_485_wrapper`, além de `display-server`, `master-server`, `web-server`, `app-server`, `upgrade-server`, IA e câmera.
- **.py do Klipper que mudaram:** `klippy.py`, `mcu.py`, `serialhdl.py`, `toolhead.py`, `gcode.py`, `extruder.py`, `pause_resume.py`, `virtual_sdcard.py`, `z_tilt.py` e outros. Diff disponível para estudo.
