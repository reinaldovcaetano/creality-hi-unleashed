# Kernel da Hi: código-fonte encontrado e compilado

Feito em 12/11/2026. Os arquivos estão em `~/Documents/Projeto Hi/kernel/`.

## O código

- **Fonte:** https://github.com/bazzonrobocon/t113-x-tina-linux-5.4. É o kernel BSP da Allwinner "Tina" para o T113, **Linux 5.4.61**, com o driver AIC8800 incluído.
  - O tarball está em `kernel/t113-x-tina-linux-5.4.tar.gz`. Não tem histórico git: é um único "Initial commit" de 10/07/2025.
- **Config da Creality:** extraída do próprio kernel da Hi (IKCONFIG), em `kernel/config-creality-1.1.0.50`. O kernel descomprimido da Hi está em `kernel/Image-creality-1.1.0.50`.
- **Compilação:**
  - roda no Docker: imagem `hi-kbuild` (`kernel/build-env.Dockerfile`: Debian bookworm, `gcc-arm-linux-gnueabi` 12.2 e vmlinux-to-elf);
  - o código fica no **volume `hi-kernel`**, porque o disco do Mac não diferencia maiúsculas de minúsculas e o kernel precisa disso;
  - o build completo do vmlinux leva cerca de 2 minutos.

## Prova de que é o mesmo kernel

1. **Config:** das 3619 opções da Creality, só **6** não existem neste código. Todas são acréscimos da Creality:
   - `LCD_SUPPORT_GC9307_*` (3 painéis da tela);
   - `TOUCHSCREEN_TLSC6X_TS` (touch, que a Hi já traz como `.ko`);
   - `AIC_FW_PATH`;
   - `USB_SUNXI_USB_ID_GPIO_ACTIVE_HIGH`.
2. **Símbolos:** a tabela kallsyms foi extraída do kernel da Hi e comparada com o `System.map` do nosso build.
   - **98,97%** das funções e variáveis reais da Creality existem no nosso build (41.068 de 41.497).
   - O resto é sobretudo inlining diferente entre o GCC 12 e o GCC 5.3, e o driver da tela `gc9307_*`.
   - O detalhe está em `kernel/comparacao-kallsyms.txt`.
3. **Módulos:** `r8152.ko` e `mac80211.ko` compilados com o vermagic **idêntico**, `5.4.61 SMP preempt mod_unload ARMv7 p2v8`.
   - **Todos os símbolos que eles importam (292 e 113) são exportados pelo kernel original** (8882 `__ksymtab` na Hi).
   - Ficam em `kernel/modulos-teste/`, junto com os módulos de criptografia que o mac80211 usa em tempo de execução: ccm, gcm, ctr, cmac, ghash, gf128mul, seqiv, drbg, jitterentropy_rng e hmac.

## O que isso libera

- **Adaptador de rede RTL8153:** o `r8152.ko` está pronto para teste no kernel original, com `insmod`, sem trocar o kernel.
- **mac80211 no kernel original:** pronto para teste. É a base de quase todos os dongles Wi-Fi que não são Realtek de fabricante.
- **Dongle MT7921AU (`mt7921u`):** não existe na 5.4. Dois caminhos:
  1. **Manter o kernel da Creality:** portar o `mt76`/`mt7921u` da 5.18 ou mais nova para a API do mac80211 e do cfg80211 da 5.4. É trabalho de porte, porque faltam APIs de HE/6 GHz no cfg80211 embutido.
  2. **Kernel nosso:** recompilar com `CFG80211=m` e usar o **linux-backports 6.x**, que traz cfg80211, mac80211 e mt7921u modernos como módulos. Exige refazer o driver da tela `gc9307` (só a Creality tem). Dá para reconstruir a sequência de inicialização desmontando o `Image-creality`.
     - O touch `tlsc6x.ko` pode ser reaproveitado.
     - O `aic8800` tem fonte neste código, mas teria de ser recompilado contra o cfg80211 do backports.
     - O kernel novo entra no `boot.fex` (imagem Android boot) da OTA própria. Tem a proteção do A/B e da gravação por cabo.

## Como testar um módulo na impressora (temporário, some no reboot)

```sh
# copiar para /tmp da Hi e carregar
insmod /tmp/r8152.ko
dmesg | tail -20      # deve aparecer r8152 ... eth1 / usb0
ip link
```

Para o mac80211: `insmod` primeiro nos módulos de criptografia (gf128mul, ctr, ghash, gcm, ccm, cmac, seqiv, hmac, jitterentropy_rng, drbg) e depois o `mac80211.ko`.

## Kernel próprio (pacote 2), progresso de 12/11/2026 à noite

- **Formato no pacote:** o item `kernel` da OTA é uma imagem Android boot v2 (`ANDROID!`, page 2048, base 0x40008000) = zImage + **device tree da Creality** (59.915 bytes, dtb_addr 0x41114800). Para o kernel novo, trocar só o zImage e manter o device tree.
- **Driver da tela recriado:** `kernel/tela-gc9307/` (ver o LEIA-ME de lá). A sequência compilada é idêntica à da Creality.
- **Kernel A** = config da Creality 1.1.0.74 + o nosso driver da tela.
  - Compila sem erro: `kernel/kernel-a/zImage` com 4.890.672 bytes (o da Creality tem 4.899.848).
  - Builds agora vão em pastas separadas (`O=/work/out-a`); a árvore foi limpa com `make mrproper`.
  - **Todos os módulos originais da Creality carregam nele** (aic8800_bsp/fdrv, gt9xxnew_ts, tlsc6x): vermagic igual e todos os símbolos atendidos (dpd_res, get_fw_path e get_testmode vêm do aic8800_bsp).
- **Chip Wi-Fi interno confirmado como AIC8800DC** (`CONFIG_AIC_FW_PATH="/lib/firmware/aic8800dc"`), que **só faz 2,4 GHz**. Bate com o que o usuário disse.
- **Próximo: kernel B** = kernel A com `CFG80211` fora do kernel + **linux-backports 6.1** (cfg80211, mac80211 e mt7921u modernos como módulos) + aic8800 recompilado contra o backports.
- **Kernel B** = kernel A sem `CFG80211`/`MAC80211` (passam a vir do backports), sem o aic8800 interno do kernel, e com CRYPTO CCM/GCM/CMAC/CTR/GHASH/SEQIV como módulos.
  - Compila: `kernel/kernel-b/zImage` (4.780.112 bytes) e `config`.
- **linux-backports 6.1.145** (SHA-256 conferido com o `sha256sums.asc` oficial) **compila contra o kernel B**: `cfg80211`, `mac80211`, `mt76`, `mt76-usb`, `mt76-connac-lib`, `mt7921-common`, **`mt7921u`** e `compat`, todos com vermagic `5.4.61 SMP preempt mod_unload ARMv7 p2v8`.
  - Ficam em `kernel/kernel-b/backports-modulos/` (com strip). A config está em `kernel/kernel-b/backports.config`.
- **Falta: o aic8800 (Wi-Fi interno) contra o cfg80211 6.1.** O driver deste código só conhece APIs até a 5.13.
  - Compilado contra o backports, dá **60 erros, em ~20 mudanças conhecidas de API** (log em `kernel/kernel-b/aic8800-erros-6.1.log`):
    - parâmetro `link_id` nas ops de chave, beacon e canal (MLO, 6.0);
    - `station_parameters` → `link_sta_params` (ht/vht/he_capa, supported_rates, opmode_notif);
    - `cfg80211_roam_info.links[]`;
    - `mgmt_frame_register` → `update_mgmt_frame_registrations`;
    - `cfg80211_ch_switch_notify(…, link_id)` e `ieee80211_amsdu_to_8023s` (novos argumentos);
    - `IEEE80211_MAX_AMPDU_BUF` → `_HT`/`_HE`;
    - `REGULATORY_IGNORE_STALE_KICKOFF` removido;
    - um bit HE renomeado;
    - aspas no `-D` do nome do kernel (erro nosso, de build).
  - Caminhos: (a) portar à mão, com escopo fechado; (b) usar um aic8800 mais novo do fornecedor (ex.: radxa-pkg/aic8800, que já suporta 6.x) adaptado ao backports.
- **Build do aic8800 externo:** `make -C /work/out-b M=/work/aic NOSTDINC_FLAGS="<includes do backports>" KBUILD_EXTRA_SYMBOLS=/work/backports/Module.symvers CONFIG_AIC_WLAN_SUPPORT=y CONFIG_AIC8800_WLAN_SUPPORT=m CONFIG_AIC_INTF_SDIO=y`.

## Pacote 2: peças prontas (12/11/2026 ~23h)

- **Driver do Wi-Fi interno na versão certa.**
  - O AIC8800 da Creality é o `RELEASE_DATE 2024_0929_2e313139`. O do código Tina era antigo (outros nomes de firmware), e por isso foi descartado.
  - O mais próximo público é o **walnutpi/aic8800_sdio (2024_1024_9a3636f0)**. Ele usa o mesmo conjunto de arquivos de firmware da Hi; alguns binários são um pouco mais novos, e por isso vai **o firmware dele junto**.
  - Candidatos baixados em `kernel/aic8800-porte/candidatos/`.
  - **Porte para o cfg80211 do backports 6.1:** `kernel/aic8800-porte/porte_walnutpi_backports61.py`.
    - Liga as macros `HIGH_KERNEL_VERSION`/`2` (APIs 6.0/6.1) e desliga a `3`/`4` (6.3).
    - Liga só as verificações de versão de APIs do Wi-Fi (AMPDU/HE, wiphy_lock, cfg80211_register_netdevice, regd_sync, quiet). As de APIs do kernel (sched_set_fifo_low, ndo_siocdevprivate, debugfs, MAC) continuam no 5.4.
    - O Makefile do bsp foi corrigido para build externo (`$(src)` no lugar de `$(srctree)/$(src)`).
    - **Resultado: 0 erros, nenhum aviso perigoso, todos os símbolos atendidos.**
  - O porte do driver antigo (`porte_cfg80211_61.py`) também compila, mas não será usado.
- **Imagem de boot:** `kernel/pacote2/bootimg.py` regenera a imagem Android boot v2 da Creality **byte a byte** (o id SHA1 confere).
  - `kernel/pacote2/kernel-B.img` = zImage do kernel B (4.779.640 bytes) + o device tree da Creality.
- **Módulos:** `kernel/pacote2/modulos/`, 28 `.ko`, **todos com vermagic certo e símbolos fechados**:
  - backports: compat, cfg80211, mac80211, mt76, mt76-usb, mt76-connac-lib, mt7921-common, mt7921u;
  - aic8800_bsp/fdrv (walnutpi);
  - criptografia: aes_generic, libaes, ccm, gcm, ctr, ghash-generic, gf128mul, cmac, seqiv, hmac, drbg, jitterentropy_rng, sha1_generic;
  - r8152, zram, zsmalloc;
  - touchs originais da Creality (gt9xxnew_ts, tlsc6x).
- **Firmwares:** `kernel/pacote2/firmware/`.
  - `mediatek/WIFI_MT7961_patch_mcu_1_2_hdr.bin` e `WIFI_RAM_CODE_MT7961_1.bin` (linux-firmware oficial).
  - `aic8800dc/`, do walnutpi.
- **Falta:**
  - script de carga do Wi-Fi: dongle como `wlan0` se estiver plugado, AIC como `wlan1`; sem dongle, o AIC fica `wlan0`;
  - mecanismo de tentativa única no U-Boot e confirmação depois dos testes;
  - montar o OTA 1.1.0.90.
