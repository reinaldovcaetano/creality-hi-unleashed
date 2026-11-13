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
