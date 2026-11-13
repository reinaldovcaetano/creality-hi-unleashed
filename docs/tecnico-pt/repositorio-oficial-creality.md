# Repositório oficial da Creality para a Hi

Baixado em 12/11/2026 para `creality-oficial/`.

- **Repositório:** https://github.com/CrealityOfficial/Hi_Klipper (GPL-3.0, criado em 28/09/2025).
  - Só tem 2 commits da Creality sobre o Klipper de dezembro de 2022: "update for open source" (28/09/2025) e um ajuste no README (07/11/2025).
  - Única tag e release: **V1.1.0.50**.
- **Outros repositórios da Hi na conta CrealityOfficial:** nenhum. Também não existe um "Hi_Annex", como os que a K1 e a KE têm.
- **Clone completo com git** em `creality-oficial/Hi_Klipper/` (4837 commits, tag V1.1.0.50). A licença do Xcode foi aceita em 12/11/2026 e o git funciona.
- **O commit da Creality é o `1f002fe5`:** 423 arquivos, ~1,44 milhão de linhas. Para ver tudo o que a Creality mudou sobre o Klipper, rodar `git show --stat 1f002fe5`.
- **Também traz objetos de MCU pré-compilados:** `src/prtouch_v2.o`, `prtouch_v3.o`, `remap_io*.o`, `z_align*.o` e `z_upsafe*.o`, além de `src/gd32/`.

## O que foi baixado

| Arquivo | O que é |
|---|---|
| `Hi_Klipper/` (clone git) | Código do Klipper da Hi. O main e a tag só diferem no README |
| `firmware-V1.1.0.50/CR4NU200360C20_ota_img_V1.1.0.50.img` | Atualização OTA (SWUpdate), igual às da página de downloads. SHA-256 `1bbc46da…a89d` |
| `firmware-V1.1.0.50/t113_i_linux_CR4NU200360C20_uart0_V1.1.0.50.img` | **Imagem completa de fábrica** (253 MB, cabeçalho `IMAGEWTY`, formato Allwinner PhoenixSuit/LiveSuit). Grava a memória inteira pela USB, em modo FEL ou burn, e serve de **recuperação** se a impressora ficar sem iniciar. SHA-256 `192e415a…efe5` |

- **Ferramenta de recuperação:** o README aponta para https://wiki.creality.com/en/k2-flagship-series/k2-plus/firmware-flashing. É o procedimento da K2, que usa o mesmo tipo de imagem.

## O que tem no código

- **Código aberto (.py) que interessa:**
  - `filament_switch_sensor.py`, de onde vem o spam "set enabled 1";
  - `auto_addr_wrapper.py`, que procura o CFS;
  - `external_material_wrapper.py`, `custom_macro.py` e `nozzle_clear.py`;
  - os `.py` "de casca" (`box.py`, `motor_control.py`, `prtouch_v3.py` etc.), que só importam os wrappers.
- **Continua fechado (`.cpython-39.so`):** `box_wrapper`, `motor_control_wrapper`, `serial_485_wrapper`, `prtouch_v2/v3_wrapper` e `filament_rack_wrapper`.
- **Não tem o `steer.py`:** o módulo de câmera só existe dentro do firmware.
- **Configs do F018** (`Hi_Klipper/config/F018_CR4NU200360C20/`), da época da .50 (printer.cfg v1.0.70):
  - box, factory_printer, printer_params e sensorless são **iguais** aos da .74;
  - na .74, o `printer.cfg` ganhou `[steer]` e o pino `PB6` deixou de ser invertido (passou a ter `value: 0`);
  - na .74, o `motor_control.cfg` passou `prt_track_max_err` de 0.1 para **0.3**;
  - na .74, o `gcode_macro.cfg` ganhou a macro `SET_PIN`, que faz o `fan0` usar o pino `fanp0` liga/desliga.
- **Firmware das placas** (`fw/F018/`): os nomes dos arquivos são os mesmos da .74 (mcu0_020_000, noz0_019_000, mot2_002_071). Tem também `fw/cfs/`, o firmware do CFS.

## Para que serve no nosso caso

1. **Rede de segurança:** a imagem uart0 permite tentar um firmware próprio, porque, se der errado, dá para gravar a imagem de fábrica pela USB. Antes, é preciso confirmar o procedimento e o botão ou modo de gravação da Hi.
2. **Base para o firmware próprio:** dá para compilar e alterar o Klipper aberto, por exemplo para silenciar o sensor. Mas os módulos `.so` fechados precisam ser os da versão instalada, e o `09faed31-dirty` da impressora pode não ser exatamente este código. Comparar com o rootfs antes de trocar algo.
3. **O diff do filament_switch_sensor e dos outros `.py`** contra a impressora mostra o que dá para corrigir com segurança no pacote de correções.

## Malha fechada, sensor de pressão da mesa e barramento 485 (pesquisa de 12/11/2026)

Foram vistos todos os repositórios de Klipper da Creality: Hi, K2, K1, KE e CR-10SE.

| Parte | Aberto (código-fonte) | Fechado |
|---|---|---|
| **Barramento 485** | `klippy/chelper/box_serial.c` (transporte serial, CRC8, início 0x3C), `msgblock_485.h` (formato `0xF7 + endereço + tamanho + estado + função + dados + crc8`) e `serial_485_queue.h` | `serial_485_wrapper.so` (classes `Serialhdl_485` e `Serial_485_Wrapper`) e os `.o` `msgblock_485`/`serial_485_queue` |
| **Motores em malha fechada** | só o `motor_control.cfg` (parâmetros) e os códigos de erro | `motor_control_wrapper.so` (59 métodos: calibração do encoder, offset elétrico, proteção, stall, leitura de pinos etc.) e o firmware de dentro dos motores (`mot*.bin`) |
| **Sensor de pressão da mesa (PRTouch v3)** | `hx711s.py`/`src/hx711s.c` (leitura das células de carga), o `prtouch.py` antigo (818 linhas, igual ao da K2) e `z_align.py`. **Da K1:** `prtouch_v1_wrapper.py` e `prtouch_v2_wrapper.py` (≈2200 linhas cada) e o firmware de MCU `src/prtouch_v2.c` | `prtouch_v3_wrapper.so` e os objetos de MCU `prtouch_v3.o`, `z_align.o`, `z_upsafe.o` e `remap_io.o` |

- As referências da K1 e da K2 estão em `creality-oficial/referencias-outros-modelos/`.
- **O mapa dos módulos fechados** (classes, métodos e comandos) foi gerado importando os `.so` num ARM emulado, com o Docker `--platform linux/arm/v7` e chroot no rootfs. Está em `docs/modulos-fechados-mapa.txt`, e o script é `scripts/introspect.py`.
- **Consequência:** os arquivos `.py` "de casca" (`box.py`, `motor_control.py`, `prtouch_v3.py` e `serial_485.py`) são abertos e só importam o wrapper. Dá para **herdar ou substituir métodos** dos módulos fechados sem o código deles. Por exemplo, `BoxAction.enable_filament_sensor` gera o spam, e pode ser trocado por uma versão silenciosa no `box.py`.
