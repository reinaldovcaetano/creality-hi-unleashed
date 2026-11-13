<h1 align="center">🖨️ Creality Hi Unleashed</h1>
<p align="center"><b>Firmware da comunidade para a impressora 3D Creality Hi (F018): Wi-Fi 6 em 5 GHz, pilha de kernel moderna, Moonraker e Fluidd novos e um instalador seguro, com teste de boot único.</b></p>
<p align="center">
<img alt="status" src="https://img.shields.io/badge/status-ALFA-red">
<img alt="impressora" src="https://img.shields.io/badge/impressora-Creality%20Hi%20(F018)-blue">
<img alt="soc" src="https://img.shields.io/badge/SoC-Allwinner%20T113--i-lightgrey">
<img alt="kernel" src="https://img.shields.io/badge/kernel-5.4.61%20%2B%20backports%206.1-green">
</p>
<p align="center">
<a href="README.md">English</a> · <b>Português</b> · <a href="README.es.md">Español</a> · <a href="README.zh-CN.md">中文</a>
</p>

> [!WARNING]
> **ALFA. Use por sua conta e risco.**
> - Este firmware foi testado em **uma** Creality Hi, com a placa-mãe `CR4NU200360C20`.
> - Ele troca o kernel e **não** tem relação com a Creality nem aval dela.
> - Se o teste de boot falhar, o instalador volta sozinho para o sistema anterior, e a placa sempre pode ser regravada pelo USB (ver [Recuperação](#recuperação)).
> - Mesmo assim, só instale se aceitar o risco.

---

## Download (alfa)

Uma imagem pronta para instalar está em **[Releases → v1.2.0.0-alpha](https://github.com/reinaldovcaetano/creality-hi-unleashed/releases/tag/v1.2.0.0-alpha)**. Ela não tem chave SSH nem calibração pessoal. Leia o aviso acima antes de instalar; para usar a sua chave SSH, compile a imagem você mesmo.

## O que você ganha

| | Creality de fábrica 1.1.0.74 | Creality Hi Unleashed (alfa, OTA 1.2.1.0) |
|---|---|---|
| Wi-Fi | AIC8800DC interno, **só 2,4 GHz** | **Wi-Fi 6 em 5 GHz** com o dongle USB MediaTek **MT7921AU** (testado: 80 MHz, 2×2, conexão a 1200 Mbit/s). O chip interno fica de reserva. |
| Pilha sem fio | cfg80211/mac80211 embutidos no kernel 5.4 | **linux-backports 6.1.145** (cfg80211, mac80211, mt76, mt7921u) + driver AIC8800 portado para ela |
| Kernel | binário da Creality (código não publicado) | recompilado a partir do BSP Allwinner Tina 5.4.61, com a config e o **device tree original** da Creality |
| Rede por cabo | não tem | adaptador USB **RTL8153** (r8152), com DHCP ao plugar |
| Memória | 488 MB de RAM, sem swap | swap de **256 MB em zram** (lzo) |
| Moonraker / Fluidd | Moonraker antigo, Fluidd 1.27 | **Moonraker (API 1.5) no Python 3.9 da impressora**, **Fluidd 1.38**, **câmera dentro do Fluidd** |
| Spam no log | `set enabled 1` gravado a cada segundo | só registra quando o estado muda |
| Nuvem Creality, ADB | ligados | **desligados** (domínios da nuvem bloqueados, ADB desativado) |
| SSH | porta 22 | **dropbear extra na porta 2222**, com login por chave e log detalhado |
| Segurança na instalação | troca A/B | **teste de boot único**: a metade nova inicia uma vez e só vira a oficial depois de passar no teste |
| Tela de toque | interface fechada da Creality | **Hi Painel**: interface própria e moderna (LVGL 8.3) com dados ao vivo, calibrações editáveis (PID, input shaper com gráfico, malha, Z tilt, PA, velocidade ideal), impressão com miniaturas, ajustes ao vivo, banco de filamentos, Wi-Fi com teclado, lembretes de manutenção e retomada após queda de energia. A tela da Creality fica a um toque. |

## Como foi feito (resumo)

1. **Anatomia do firmware.**
   - A OTA é um cpio SWUpdate ("070702") **sem assinatura**, com `sw-description`, `uboot`, `kernel`, um `rootfs` squashfs gzip e uma lista MD5.
   - O eMMC tem metades A/B: `bootA/B` em p4/p5 e `rootfsA/B` em p6/p7.
   - Quem instala pelo pendrive é o `upgrade-server`, que funciona sem internet.
2. **Código do kernel.**
   - A Creality nunca publicou. Um código Tina 5.4.61 público do T113 bateu com o dela: das 3619 opções de config da Creality, faltam só 6, e **98,97 %** dos símbolos reais dela existem no nosso build.
   - As peças que faltavam foram reconstruídas:
     - **driver da tela GC9307:** recriado a partir da desmontagem do kernel da Creality, com a sequência de inicialização idêntica (100 de 100 operações);
     - **pino ID da USB-A invertido:** `USB_SUNXI_USB_ID_GPIO_ACTIVE_HIGH`, que a Creality usa para a porta USB-A funcionar como host.
3. **5 GHz.**
   - O driver MT7921 não existe para a 5.4. Por isso o kernel foi recompilado com `cfg80211` em módulos e com o **linux-backports 6.1**.
   - O driver do **AIC8800DC** interno (walnutpi 2024_1024, próximo da versão da Creality) foi portado para a API 6.1 e compilado para a plataforma Allwinner. Compilado para a plataforma genérica "Ubuntu", ele nunca ligaria o chip.
4. **Instalador por tentativa única.**
   - O pacote **não** troca a metade oficial. Ele grava `bootcmd=run hi_tenta`, e o U-Boot inicia a metade nova **uma vez**.
   - O `hi-confirma` só torna essa metade oficial depois de passar por:
     - 90 s de boot;
     - **60 s de Klipper "ready" sem interrupção**;
     - **leitura real da EEPROM pelo Klipper**;
     - tela, touch e USB-A em modo host.
   - Se algo falhar, ele reinicia e a Hi volta para a metade antiga.
   - Lição aprendida: o U-Boot calcula o `root=` **antes** do `bootcmd`, então o `hi_tenta` também precisa definir `mmc_root`/`nand_root`.
5. **O defeito do I2C, achado ao vivo na impressora.**
   - No kernel novo, o Klipper caía com `MCU 'rpi' shutdown: Unable write i2c device`.
   - Causa: **na tabela de pinos do BSP público, o PG9 função 3 está como `twi0`; o certo é `twi1`**. A Creality corrigiu isso sem publicar.
   - Efeito: só o SCL era ligado ao I2C e o SDA (PG9) ficava sem dono.
   - Corrigido no kernel. Agora o `kernel/tools/conferir_pinos.py` confere **todos os grupos de pinos do device tree contra a tabela do kernel** (328/328).
   - O driver I2C também ganhou novas tentativas: uma EEPROM ocupada recusa a leitura (NACK) nos **dois** drivers, o nosso e o da Creality.
6. **A armadilha da camada gravável.**
   - O `rootfs_data` sobrevive às atualizações, então um arquivo antigo nessa camada pode esconder o arquivo novo do pacote.
   - O `hi-limpa-overlay` remove essas cópias antigas uma vez por versão, preserva as configurações do usuário e faz backup de tudo o que remove.

As anotações completas do laboratório estão em [`docs/tecnico-pt/`](docs/tecnico-pt/).

## Como replicar (compilar você mesmo)

**Requisitos:**
- Docker com Docker Compose v2;
- cerca de 10 GB livres;
- `curl`, `unzip`, `cpio`;
- a **OTA oficial 1.1.0.74 da Creality Hi**, baixada do site da Creality.

```bash
git clone https://github.com/reinaldovcaetano/creality-hi-unleashed && cd creality-hi-unleashed
# 1. coloque a OTA oficial em: insumos/CR4NU200360C20_ota_img_V1.1.0.74.img
./scripts/obter-insumos.sh            # baixa e confere o SHA-256 de todos os insumos (commits fixos)
# 2. opcional, nunca vai para o git:
#    pacote/local/authorized_keys     -> sua chave pública SSH (login na porta 2222)
#    pacote/local/save_config.txt     -> o bloco SAVE_CONFIG da SUA impressora (restaurado depois de um reset)
docker compose run --rm kbuild        # kernel + backports + AIC8800 -> pacote/p2/   (~10-15 min)
docker compose run --rm painel        # Hi Painel (tela própria) -> pacote/painel/   (~1 min)
docker compose run --rm pack          # OTA -> saida/CR4NU200360C20_ota_img_V1.2.1.0.img
./tests/rodar-testes.sh               # testa os scripts de boot num ARM emulado (busybox da própria imagem)
```

O build do kernel mostra a conferência de pinos (`328 ok, 0 com erro`) e confere o `vermagic` de cada módulo. Ele também monta a imagem de boot com o **device tree original da Creality**.

### Containers de build prontos

Os containers de build estão publicados no GitHub Packages, então você não precisa montá-los localmente:

```bash
docker compose pull      # ghcr.io/reinaldovcaetano/creality-hi-unleashed-{kbuild,pack}:alpha
```

Eles só têm as ferramentas: Debian bookworm, o compilador cruzado ARM e o squashfs-tools. Nenhum arquivo da Creality vai junto.

## Instalação

1. Use um pendrive em **FAT32**, só com `CR4NU200360C20_ota_img_V1.2.1.0.img` na raiz. Não renomeie o arquivo.
2. Com a impressora parada, coloque o pendrive e confirme a atualização na tela.
3. **Espere uns 7 minutos.** Ela reinicia uma vez no sistema novo, faz o teste de boot e então:
   - **se passar:** a metade nova vira a oficial (`logs/hi-confirma.log` mostra `CONFIRMADO`);
   - **se falhar:** ela reinicia sozinha no sistema anterior (`logs/hi-confirma-falhou.log`).
4. Para usar 5 GHz, plugue o dongle MT7921AU e reinicie.
   - O dongle vira `wlan0` com o **mesmo MAC do Wi-Fi interno**, então o IP não muda.
   - Sem o dongle, a impressora usa o Wi-Fi interno como antes.

Depois de instalar:
- **Fluidd:** `http://<ip-da-impressora>:4408`, com a câmera.
- **Moonraker:** porta `7125`.
- **SSH:** porta `2222`.
- **Diagnóstico:** `logs/hi-diag.txt` e `logs/hi-limpa-overlay.log`.

> [!IMPORTANT]
> **Não** instale atualizações oficiais da Creality por cima deste firmware. Elas trocariam o kernel e perderiam os drivers de 5 GHz. Use builds deste projeto.

## Hi Painel (a tela nova)

- **Tela padrão ao ligar.** A tela da Creality sobe primeiro; quando ela chega na tela inicial, o Hi Painel assume. Ele **não** assume durante uma impressão nem com um pendrive de atualização, para o instalador da Creality funcionar como sempre.
- **Queda de energia:** se uma impressão foi interrompida, o Hi Painel mostra *Continuar / Descartar*. Ele usa os mesmos comandos do Klipper que a Creality: `SDCARD_PRINT_FILE ... ISCONTINUEPRINT=True` e `pause_resume/cancel_continue_print`. Também dá para escolher decidir na tela da Creality.
- **Ir para a tela da Creality:** botão *Creality*. **Voltar:** na tela da Creality, toque no robozinho, depois em *Ajuda* e em *Wiki*.
- **Calibrações:** cada uma mostra o valor padrão, deixa editar, roda ao vivo e compara *atual × novo* antes de salvar. Ao salvar, sempre faz backup do `printer.cfg` antes. A *Velocidade ideal* também atualiza o teto de aceleração das macros.
- **Desligar:** rode `touch /mnt/UDISK/hi-painel/desligado` e reinicie. Uma versão de teste em `/mnt/UDISK/hi-painel/hi-painel` tem prioridade sobre a do pacote.
- **Ver as telas sem a impressora:** `hi-painel/mock` (`make -f Makefile.mock`; `HI_MOONRAKER=<ip-da-impressora>` usa dados ao vivo).

## Recuperação

- **Automática:** se o teste de boot falhar, a impressora volta para a metade anterior.
- **Troca manual da metade de boot:**

  ```sh
  fw_setenv boot_partition bootB; fw_setenv root_partition rootfsB; reboot
  ```
- **Reset de fábrica:** prepare um pendrive com um arquivo vazio chamado `wipe_all`, conforme a wiki da Creality.
- **Regravação completa pelo USB:**
  1. Segure o botão **LOAD/FEL** na placa.
  2. Ligue o cabo Micro USB num PC com Windows.
  3. Grave a imagem de fábrica da Creality pelo **PhoenixSuit**.

## Limitações conhecidas (alfa)

- Testado em uma só impressora.
- Só a variante de tela `gc9307_9bit_spi_tjc_240_320`, a que está no device tree testado, foi reconstruída.
- O módulo de touch `tlsc6x` vem desligado, porque a unidade testada usa touch Goodix. Para religar:

  ```sh
  echo tlsc6x > /etc/modules.d/touchscreen-tlsc6x
  ```
- Só o dongle MT7921AU (`0e8d:7961`) foi testado.
- A nuvem da Creality fica desligada de propósito.
- As configs da impressora que vêm no pacote são as configs ajustadas do autor: aceleração de 4300 mm/s² e correções de macros. As suas configs são **mantidas** se você as tiver alterado; as de fábrica são trocadas.

## Estrutura do repositório

```
compose.yaml, docker/        containers de build (kbuild = kernel, pack = OTA)
scripts/obter-insumos.sh     baixa e confere todos os insumos (versões fixas)
kernel/patches/              linux-hi.patch (tela, USB-ID, pino PG9, nova tentativa no I2C), aic8800-walnutpi-hi.patch
kernel/config/               kernel-b.config, backports.config, config original da Creality
kernel/tools/                bootimg.py (Android boot v2), conferir_pinos.py (device tree x pinctrl)
kernel/build-kernel.sh       build completo do kernel e do Wi-Fi
pacote/build/                dentro.sh (monta a OTA), tentativa.py (boot de tentativa única)
pacote/overlay*/             arquivos adicionados ao rootfs (scripts de boot, Moonraker, configs)
tests/                       testes em ARM emulado, ferramentas de bancada do I2C
docs/tecnico-pt/             anotações completas do laboratório (português)
```

## Créditos e licenças

- **Feito com a ajuda do [Claude](https://claude.ai) (Anthropic), pelo Claude Code.** O Claude trabalhou junto com o autor na desmontagem do firmware, no porte do kernel e do Wi-Fi, na depuração ao vivo na impressora (incluindo o defeito do PG9/I2C), no processo de build, nos testes e nesta documentação em quatro idiomas. 🤖🤝

- **Creality:** Hi_Klipper (GPL-3.0). A OTA é da Creality e **não** é redistribuída aqui.
- **BSP Allwinner Tina Linux 5.4:** espelho de `bazzonrobocon` (GPL-2.0). Os nossos patches de kernel são GPL-2.0.
- **linux-backports**, **linux-firmware** e **wireless-regdb:** kernel.org.
- **walnutpi/aic8800_sdio:** GPL-2.0.
- **Moonraker** (GPL-3.0) e **Fluidd** (GPL-3.0).
- Scripts e documentação do projeto: GPL-3.0 (ver `LICENSE`).

*Creality e Creality Hi são marcas dos seus donos. Este é um projeto independente da comunidade.*
