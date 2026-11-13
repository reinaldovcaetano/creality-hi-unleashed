# Hi Unleashed 1.0 (completa): OTA 1.2.0.0

> **Status: ALFA.** Gerada e testada só em ARM emulado. **Não instalada na Hi** (decisão do usuário em 13/11/2026: não faz sentido testar agora). A Hi segue na 1.1.0.92. O pendrive "IMPRESSORA" ficou gravado com ela; antes de usar, regerar se algo mudou no projeto.

- **Gerada em 13/11/2026 (alfa).** Arquivo: `pacote/saida/CR4NU200360C20_ota_img_V1.2.0.0.img`.
  SHA-256 `d6ad942500e445d6c622032f53c65abc80a206534ca509e13f72617129101674`.
- **Para reconstruir:** `cd pacote && P2=1 TENTATIVA=1 ./build.sh 1.2.0.0 "Hi Unleashed 1.0 (completa)"`.
- **Por que 1.2.0.0 e não 0.0.0.1:** o instalador da Creality (`swupdate -N`) recusa versões menores que a instalada.

## O que ela faz de uma vez só

Ela junta tudo dos pacotes 1a, 1b, 1c e 2 (1.1.0.80 a 1.1.0.92), mais estes ajustes.

### Kernel e drivers

- **Kernel próprio "B"**, sobre a config da Creality:
  - driver da tela GC9307 recriado;
  - pino ID da USB-A invertido;
  - **PG9 = twi1** (correção da tabela de pinos do BSP);
  - nova tentativa no I2C.
- **Drivers:**
  - backports 6.1, com o **dongle MT7921AU (5 GHz)**;
  - AIC8800 interno com o driver novo;
  - r8152 (rede por cabo USB);
  - zram 256 MB.
- **tlsc6x desligado:** o touch desta Hi é Goodix.

### Software

- Moonraker novo (API 1.5).
- Fluidd 1.38.
- Câmera no Fluidd.
- Correção do spam do sensor de filamento.
- Nuvem e ADB desligados.
- SSH 2222 com chave.
- Diagnóstico em `hi-diag.txt` e limpeza de logs.

### Wi-Fi

- `hi-wifi`: o dongle vira wlan0 com o MAC do interno, e o IP não muda.
- Sem dongle, fica só o interno.

### Instalação por tentativa única

O `hi-confirma` só confirma a instalação depois de:

- 90 s de boot;
- 60 s de Klipper "ready" sem interrupção;
- leitura real da EEPROM pelo Klipper;
- tela, touch e USB-A em host.

Se algo falhar, a Hi volta sozinha para a versão anterior.

### Novos nesta versão

- **`hi-limpa-overlay` (S11):** uma vez por versão, tira da camada gravável as cópias antigas e os "whiteouts" de qualquer arquivo do pacote.
  - A lista de 1377 arquivos é gerada no build, comparando com a .74, e fica em `/usr/share/hi/arquivos-do-pacote.txt`.
  - Não mexe em `/etc/config`, no Wi-Fi, nas chaves SSH, no hostname nem em `uci-defaults`.
  - O backup vai para `/mnt/UDISK/hi-overlay-sobras-<versão>.tgz` e o log para `logs/hi-limpa-overlay.log`.
- **`hi-ajustes` com regra nova:** as configs (gcode_macro, sensorless, box, motor_control, printer_params) só são trocadas se faltarem ou se forem **de fábrica**. A conferência usa o MD5 das versões .50, .74 e GitHub, em `/usr/share/hi/fabrica.md5`.
  - Arquivo ajustado à mão fica como está.
  - A calibração (SAVE_CONFIG) é sempre mantida.
  - Sem calibração, entra a de 12/11.

## Testes antes de entregar (ARM emulado, com o busybox da própria imagem)

- **Limpeza do overlay:**
  - removeu as sobras de hi-confirma, moonraker-hi.conf, o whiteout de hi-wifi e o hosts;
  - manteve `etc/config/network` e o whiteout do tlsc6x;
  - a 2ª execução não faz nada.
- **hi-ajustes:**
  - gcode_macro de fábrica .74 e box de fábrica .50 foram trocados;
  - sensorless do usuário foi mantido;
  - motor_control já era o do pacote;
  - printer_params, que faltava, foi instalado;
  - printer.cfg com calibração foi mantido.
- **Pacote:**
  - MD5 internos ok;
  - versão 1.2.0.0;
  - hi_tenta p6/p7;
  - kernel = pinfix+retry;
  - `conferir_pinos.py` 328/0;
  - 28 módulos;
  - todos os links rc.d presentes;
  - tlsc6x ausente.

## Instalação

- Pendrive FAT32 com só o `.img` na raiz. Confirmar na tela e esperar cerca de 7 min.
- Numa Hi que esteja na 1.1.0.92 (metade A), ela grava na B; a 1.1.0.92 fica de volta na A.
- **Numa Hi com o `upgrade-server` da Creality trocado** (como a .60 desta Hi antes do reset): a instalação trava em 0%, e aí é preciso o `wipe_all` antes. Ver `docs/pacote-1a.md`.
