# Calibrações da Creality Hi: varredura e otimizações (alfa)

Varredura de todos os processos de calibração/nivelamento da Hi (firmware 1.1.0.74 + Creality Hi Unleashed) e o que foi ajustado.
Nada do que a impressora usa para **se localizar** foi alterado: G28, sensores ópticos do Z (`z_align`), `z_offset`,
sensor de pressão (`prtouch_v3`) e o home preciso (`NEXT_HOMEZ_NACCU`).

## Como a Hi calibra (estado de fábrica)

| Processo | Comando | O que faz |
|---|---|---|
| Nivelamento + malha (tela "plataforma") | `G29` | mesa a 60 °C, limpa o bico (`NOZ_CLEAR`), G28 Z, `Z_TILT_ADJUST`, `BED_MESH_CALIBRATE`, grava com `CXSAVE_CONFIG` (grava **todas** as pendências sem reiniciar) |
| Vibração | `INPUTSHAPER` / `SHAPER_CALIBRATE` | dois acelerômetros (cabeçote e mesa); shapers testados limitados por `AUTOTUNE_SHAPERS` (`mzv,zv,ei`) |
| PID | `PID_CALIBRATE` | bico e mesa |
| Início de impressão pelo Fluidd/Moonraker | `START_PRINT` (prepare=0) | limpa bico, G28 Z, espera a mesa do arquivo, home preciso, **mede a malha toda vez** e grava |
| Início pela tela da Creality | `PRINT_PREPARED` → `START_PRINT` (prepare=1) | usa a malha salva |

## Mudanças (script `scripts/otimiza_calib.py`, idempotente)

| Item | Antes | Depois | Por quê |
|---|---|---|---|
| `[z_tilt] retry_tolerance` | 0.1 | **0.03** | 0,1 mm deixava a mesa parar torta (log: 0,051 mm); valores comuns 0,01–0,04 |
| `[bed_mesh] probe_count` | 9,9 | **7,7** | mesa da Hi é uma curva suave; 49 pontos ≈ 40% mais rápido em toda impressão |
| `[nozzle_clear] clear_cnt` | 10 | **6** | menos tempo de limpeza; mantém 3 toques |
| `START_PRINT` | sempre mede | `variable_skip_mesh` | o Hi Painel pode imprimir com a malha salva |
| `G29` | 60 °C fixo | `G29 BED=<temp>` (`scripts/macro_g29_temp.py`) | malha medida na temperatura de uso; sem `BED` = original |
| Shaper | 2hump_ei | mzv (medido) | a documentação do Klipper desaconselha 2hump_ei na frequência de ressonância; mzv suaviza menos |

## Hi Painel

- **Imprimir**: "Medir malha e imprimir" (padrão), "Usar malha salva", "Descartar pendências" (se houver calibração não salva) e Voltar.
- **Nivelamento e malha**: temperatura da mesa editável (padrão = filamento ativo); mostra em que temperatura a malha salva foi medida.
- **Calibrar tudo**: home → vibração → PID bico → PID mesa → malha (na mesa do filamento ativo).
- **Vibração**: prefere mzv quando vibra quase igual ao recomendado; nunca sugere 2hump/3hump.

## Fontes

- Klipper, Resonance Compensation: https://www.klipper3d.org/Resonance_Compensation.html
- Fórum Klipper, Z_TILT_ADJUST: https://klipper.discourse.group/t/z-tilt-adjust-command-not-level-enough/7451
- 33 Creality Hi Mods: https://printer-hub.ru/en/posts/creality-hi-best-mods
- Fórum Creality, faixa da malha na Hi: https://forum.creality.com/t/creality-hi-bed-mesh-range-of-0-572-mm-is-this-normal-for-a-new-printer/52385
- PID no Klipper: https://3dprintbeginner.com/hotend-and-heatbed-pid-tuning-in-klipper/

## Desempenho (13/11/2026)

- O Klipper da Creality grava no `klippy.log` duas linhas por requisição, inclusive consultas de estado: com uma tela consultando a cada segundo, ~250 MB/dia no eMMC. `scripts/webhooks_menos_log.py` tira do log só as consultas de leitura (`objects/query`, `objects/list`, `info`); o arquivo corrigido vai no overlay do pacote. Resultado medido: ~40 MB/dia.
- O Hi Painel consulta a cada 0,7 s só quando está na frente; com a tela da Creality ou em descanso, a cada 4 s. A matriz da malha vem a cada 10 consultas.
- Medido parado: CPU ~22% ocupada (2 núcleos a 1,2 GHz), ~378 MB de RAM livres de 500, 38 °C.
- Serviços: o pacote não sobe mais o `web-server` (app/nuvem Creality, portas 80/443/9998/9999) nem o `webrtc` da nuvem. A câmera do Fluidd continua (`cam_app` + `webrtc_local` na 8000). SSH só na 2222. Relógio pelo NTP.br (`a.st1.ntp.br`, `0/1.br.pool.ntp.org`). Para quem já tem a Hi instalada, o `hi-ajustes` aplica o mesmo no boot. **Consequência:** o app Creality Print/Cloud deixa de enxergar a impressora (use Fluidd e Hi Painel).

## Backup e como voltar atrás

- Antes de qualquer mudança, os arquivos foram copiados para `printer_data/config/backup-antes-otimizacao-20261113-1615/`:
  - todos os `.cfg`;
  - `webhooks.py.original`;
  - `sistema/` com `system`, `dropbear`, `app`, `hi-diag` e `hi-ajustes`.
- Para voltar as calibrações: copie os `.cfg` do backup de volta e rode `FIRMWARE_RESTART`.
- Para voltar o log: copie o `webhooks.py.original` para `/usr/share/klipper/klippy/webhooks.py`, apague o `.pyc` e rode `/etc/init.d/klipper restart`.
- Para voltar os serviços:
  1. copie `system`, `dropbear` e `app` de volta (`/etc/config/` e `/etc/init.d/`);
  2. rode `/etc/init.d/webrtc enable`, `/etc/init.d/webrtc start` e `/etc/init.d/app restart`.
  - Atenção: o `hi-ajustes` reaplica a função `sistema` no boot. Para desativar de vez, comente a chamada a `sistema` nele.

## Falta testar na prática

- Uma impressão com "Medir malha e imprimir" e outra com "Usar malha salva".
- Um Z tilt, para ver quantas tentativas leva com a tolerância de 0,03.
- Uma medição nova de vibração pelo painel (para ver se ele prefere o mzv).
- `G29 BED=` com temperatura diferente de 60 °C (esperando a mesa aquecer).
- Um boot completo, para conferir que o SSH do sistema sobe na 2222, que a 22 fica fechada e que o web-server e o webrtc não voltam.
- Uma queda de energia real durante a impressão.
