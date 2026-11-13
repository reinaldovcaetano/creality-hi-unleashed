# Pacote 2: kernel próprio + dongle Wi-Fi 6 (5 GHz), e o 1c (teste do mecanismo)

Gerados em 12/11/2026 ~23h. Arquivos em `pacote/saida/`:

| Arquivo | O que é | SHA-256 |
|---|---|---|
| `CR4NU200360C20_ota_img_V1.1.0.81.img` | **1b**: Moonraker API 1.5, Fluidd 1.38, câmera no Fluidd, correções. Kernel original, instalação normal | `2c6dbd19…f28c` |
| `CR4NU200360C20_ota_img_V1.1.0.82.img` | **1c**: o mesmo 1b, mas instalado por **tentativa única** (kernel original). Serve para testar o mecanismo do U-Boot isoladamente | `76d13660…326a` |
| `CR4NU200360C20_ota_img_V1.1.0.90.img` | **2**: tudo do 1b + **kernel B** + pilha Wi-Fi do backports 6.1 + **dongle MT7921AU** + AIC8800 interno portado, por **tentativa única** | `a0ba53f5…671c` |

- **Para reconstruir:**
  - 1b: `./build.sh 1.1.0.81 "..."`;
  - 1c: `P2=0 TENTATIVA=1 ./build.sh 1.1.0.82 "..."`;
  - 2: `P2=1 TENTATIVA=1 ./build.sh 1.1.0.90 "..."`.
  - As peças do pacote 2 ficam em `pacote/p2/` (kernel-B.img, modulos/, firmware/) e `pacote/overlay-p2/`. As fontes estão em `kernel/`.

## Tentativa única (o que protege a troca de kernel)

O U-Boot da Hi não tem contador de boot nem kernel alternativo; ele só sabe ler o env.
- **O que muda no `sw-description`** (`pacote/build/tentativa.py`): a instalação **não troca** `boot_partition`/`root_partition`. Ela grava:
  - `bootcmd = run hi_tenta`
  - `hi_tenta = setenv bootcmd run setargs_nand boot_normal; saveenv; setenv boot_partition bootX; setenv root_partition rootfsX; run setargs_nand boot_normal`
- **No 1º boot depois de instalar:** o U-Boot devolve o `bootcmd` normal e **grava o env** (a oficial continua sendo a metade antiga). Só então inicia a metade nova, **uma vez**.
- **`/etc/init.d/hi-confirma` (S99), na metade nova:**
  - espera até 4 min por `display-server` rodando, `/dev/fb0`, módulo de touch carregado e Klipper "ready", três vezes seguidas;
  - se passar: `fw_setenv boot_partition/root_partition` da metade nova, `version`, e apaga `hi_tenta`;
  - se falhar: grava `logs/hi-confirma-falhou.log` e **reinicia**, voltando para a antiga.
- **Se o kernel novo travar antes do sistema subir:** desligar e ligar volta para a antiga.
- **Risco que sobra:**
  - o U-Boot interpretar o `hi_tenta` de outro jeito (o interpretador dele é o simples, sem `if`); nesse caso ele pararia no próprio U-Boot;
  - recuperação: PhoenixSuit + cabo Micro USB (`docs/recuperacao-usb.md`). Por isso existe o **1c**, que testa só isso, com o kernel original.

## Pacote 2 por dentro

- **Kernel B** (`kernel/pacote2/kernel-B.img`): Tina 5.4.61 com a config da Creality + **driver da tela GC9307 recriado** (sequência idêntica à da Creality) + `CFG80211`/`MAC80211` fora do kernel.
  - Imagem Android boot v2 com o **device tree da Creality**; o gerador reproduz a imagem original byte a byte.
- **Wi-Fi:** linux-backports 6.1.145 (cfg80211, mac80211, mt76, mt7921u).
  - **AIC8800DC interno:** driver walnutpi 2024_1024 portado, com o firmware dele em `/lib/firmware/aic8800dc`. O da Creality foi guardado em `aic8800dc.creality` e `/lib/modules/5.4.61.creality/`.
  - `regulatory.db` (wireless-regdb oficial) e `iw reg set BR`.
- **`/etc/init.d/hi-wifi` (S19):** carrega a pilha, depois o `mt7921u`, espera até 6 s o dongle virar `wlan0` e então carrega o AIC (que vira `wlan1`, ou `wlan0` sem dongle). Log em `/tmp/hi-wifi.log`, copiado para `logs/` pelo `hi-confirma`.
  - O `wifi-server`/tela da Creality continua usando o `wlan0`. Com o dongle, a tela passa a ver as redes de 5 GHz.
- **28 módulos**, todos com vermagic e símbolos conferidos: crypto, backports, aic, r8152, zram/zsmalloc e os touchs originais.
- **Validado no ARM emulado:** scripts (`sh -n`), links de boot, módulos, firmwares, versão e o `sw-description` em modo tentativa.

## Ordem recomendada

1. Instalar o **1b (1.1.0.81)**, pela instalação normal e sem risco de kernel. Usar alguns dias.
2. Com o usuário perto da impressora e o PC/cabo à mão: **1c (1.1.0.82)**, para validar o mecanismo de tentativa única.
3. Se o 1c confirmar sozinho (`logs/hi-confirma.log`: "CONFIRMADO"): **pacote 2 (1.1.0.90)** com o dongle plugado.

## Correção de 12/11/2026 ~23h20: lógica da porta USB-A (pergunta do usuário)

- **O problema:** o `usbc0` é a porta OTG e é **nele que ficam a USB-A (pendrive e dongle) e o Micro USB**. A câmera fica no `usbc1`.
  - O usbc0 decide se é host ou dispositivo pelo pino ID **PG7** (`usb_port_type=2`, `usb_detect_type=1`).
  - A Creality compila com `CONFIG_USB_SUNXI_USB_ID_GPIO_ACTIVE_HIGH`, que **não existe no código público**.
- **Desmontagem do `vbus_id_hw_scan`:**
  - **Creality:** pino 0 → dispositivo, pino 1 → host (`clz`).
  - **Nosso, antes da correção:** o inverso.
  - **A detecção de VBUS é idêntica** nos dois.
- **Correção:** a opção foi recriada em `drivers/usb/sunxi_usb/Kconfig`, com o `#if` em `get_id_state()` (`usb_hw_scan.c`; cópia em `kernel/pacote2/usb_hw_scan.c.patched`). Ela foi ativada nos kernels A e B.
  - **Conferido no binário:** agora pino 0 → `DEVICE(1)` e pino 1 → `HOST(0)`, como na Creality.
- **O `hi-confirma` também exige o `4101000.ehci0` registrado** (porta USB-A em modo host). Se não estiver, ele volta para a versão anterior. Na Hi atual, a checagem passa.
- **Diferenças de config entre o kernel B e o da Creality depois disso:** só Wi-Fi, criptografia, aic, r8152, zram, as 2 variantes de tela que não são desta máquina e o touch tlsc6x embutido (ele vem como `.ko` original).
- **Regerados:** 1.1.0.90 (`55fcd037…b7d9`) e 1.1.0.82 (`663e37af…6698`).

## Auditoria geral (12/11/2026 ~23h35, pedida pelo usuário)

| Frente | Resultado |
|---|---|
| Hardware × driver | Dos **115 `compatible`** do device tree, **nenhum** tem driver na Creality e falta no nosso |
| Kernel da Creality × nosso código | O da Creality veio de um **pacote Allwinner mais novo**: I2C ("drv-mode/engine-mode"), ajuste de PHY USB e DSP (rproc/rpbuf/msgbox) são mais novos. O DSP está **offline** na Hi e **não há placa de som**, então não importa. O DTS não tem parâmetros de PHY USB. O I2C (touch Goodix `gt9xxnew_ts` no i2c-3) passou a ser checado pelo `hi-confirma` |
| **Wi-Fi interno: alimentação do chip** | **CRÍTICO, corrigido.** O driver walnutpi estava com `CONFIG_PLATFORM_UBUNTU=y` e **não chamava `sunxi_wlan_set_power`/`sunxi_mmc_rescan_card`/`sunxi_wlan_get_bus_index`**; o chip nem ligaria. Recompilado com `CONFIG_PLATFORM_ALLWINNER=y CONFIG_PLATFORM_UBUNTU=n`. Agora importa os mesmos símbolos de plataforma que o da Creality |
| Wi-Fi interno: potência | O `aic_userconfig_8800dc.txt` do walnutpi usa **20 dBm**; o da Creality, 16–18 (ajuste de antena e certificação). **Corrigido: vai o arquivo da Creality** (mesmo formato, 52 linhas) |
| Porta USB-A | Lógica do pino ID invertida pela Creality, **recriada** (seção acima). O `hi-confirma` exige o ehci0 em host |
| Carga automática de módulos | Não há `udevd` nem `modules.alias`; o `modules-boot.d` só tem o g2d. A ordem do Wi-Fi fica sob controle do `hi-wifi` |
| Firmware AIC | O driver pede `*_8800dc_u02` e `*_h_u02`; todos estão no pacote |
| Programas da Creality × Moonraker | **Nenhum** (tela, web-server, master, wifi, app, IA) usa o Moonraker: falam direto com o Klipper. Trocar o Moonraker só afeta Fluidd e painel externo (já testado) |
| U-Boot × `hi_tenta` | O env da própria Creality já usa `;` dentro de variável executada por `run` (`boot_dsp0`), então o interpretador aceita. `saveenv` existe no binário |
| **`hi-confirma` na Hi real (simulação)** | Rodado por SSH com as escritas trocadas por mensagens: **todas as checagens passaram em 15 s** (tela, fb0, touch respondendo, USB-A em host, Klipper ready 3×) e identificou a metade certa (B) |

Pacotes regerados depois da auditoria: **1.1.0.90 `a0ba53f5…671c`** e **1.1.0.82 `76d13660…326a`**.

## Correção de 13/11/2026 ~06h45: a tentativa usaria o sistema antigo

Achado na "mais uma conferida" (a conferência anterior tinha sido interrompida).
- **O problema:** o U-Boot da Allwinner converte `root_partition` em `nand_root`/`mmc_root` na **inicialização da placa** ("update part info" → "set root to %s"), antes do `bootcmd`.
  - O `hi_tenta` antigo só trocava `boot_partition`/`root_partition` dentro do `bootcmd`. A tentativa iniciaria o **kernel novo** com o **rootfs antigo** (o `root=` já estava calculado).
  - O resultado seria seguro (o rootfs antigo não tem `hi-confirma`, então nada seria confirmado e o boot seguinte voltaria para a antiga), mas o teste **nunca** passaria. No pacote 2, o Wi-Fi ficaria sem driver nessa tentativa.
- **Correção** (`pacote/build/tentativa.py`): o `hi_tenta` também faz `setenv nand_root /dev/mmcblk0pN; setenv mmc_root /dev/mmcblk0pN`.
  - Pela GPT da imagem de fábrica: **rootfsA = mmcblk0p6, rootfsB = mmcblk0p7** (bootA = p4, bootB = p5).
  - O `hi-confirma` já descobre a metade pelo `root=` do `/proc/cmdline` (6 = A, 7 = B), então bate.
- **Conferir na Hi antes do 1c:** `cat /proc/cmdline` deve mostrar `root=/dev/mmcblk0p7` (hoje ela roda na metade B), e `ls -l /dev/by-name/` deve mostrar `rootfsB -> mmcblk0p7`.
- **Pacotes regerados:**
  - **1.1.0.82** `eca667885a472ae00fd2ad565f7638c4a9985bb0bef65fbf015cbe198dd8aafe` (regerado 13/11 07h37 com a correção do Moonraker);
  - **1.1.0.90** `29172b0ad1dac7c796d5b623c0e61a81d5fbd9344f028b909c8acfafa3bcc81d` (regerado 13/11 07h48 com o hi-confirma blindado).
  - O 1.1.0.81 não muda (instalação normal).

## 13/11/2026: 1ª instalação do pacote 2 (1.1.0.90) e correção do I2C → 1.1.0.91

- **Resultado da 1.1.0.90:** tela, touch, USB-A, Wi-Fi interno (AIC8800 novo, conectou) e os 28 módulos ok. O Klipper caiu aos 61 s: "MCU 'rpi' shutdown: Unable write i2c device", com dmesg "Address + Read bit transmitted, ACK not received" (EEPROM bl24c16f 0x50, i2c.1). O hi-confirma não confirmou e a Hi voltou sozinha para a 1.1.0.82.
- **Diagnóstico em bancada, na própria Hi, sem pendrive:** `kernel/i2c-teste/` (módulo `i2c_hi.ko` com o nosso driver, `eeprom_teste`, `ocupado_teste`).
  - Com a EEPROM ocupada, os dois drivers falham 20 de 20: o nosso v2.6.5 e o original da Creality v2.1.4.
  - Leitura normal: 400 de 400 ok em ambos.
- **Correção:** novas tentativas no `sunxi_twi_xfer` (`patch_retry.py`, versão "2.6.5-hi1"). Com a EEPROM ocupada, 20 de 20 foram recuperadas.
- **hi-confirma reforçado:** 90 s de boot + 60 s de Klipper "ready" contínuo + leitura real da EEPROM pelo Klipper; janela de 7 min.
- **1.1.0.91** SHA `f6a504298daa874d865a017efcdd1eff0e3f112f41fdf78a4d1f77be4ed4b52b`.

## 13/11/2026: 1.1.0.91 falhou → causa real: pino PG9 → 1.1.0.92

- Na 1.1.0.91, as novas tentativas do I2C também falharam, inclusive nas escritas e em qualquer endereço. Com o barramento ativo, o pinmux mostrava **PG8 = twi1 e PG9 = UNCLAIMED**.
- **Causa:** na tabela `pinctrl-sun8iw20.c` do BSP público, o PG9 função 0x3 = "twi0". O certo é "twi1"; a Creality já tinha corrigido isso no kernel dela.
- **Correção:** `kernel/pacote2/pinctrl-PG9-twi1.patch`.
- **Conferência automática:** `kernel/conferir_pinos.py` passou de 327/1 para 328/0.
- **1.1.0.92** SHA `c1d4a755ec5a127bf419153c19408da619bd1b6769f707453d62cda9116958b9`.
