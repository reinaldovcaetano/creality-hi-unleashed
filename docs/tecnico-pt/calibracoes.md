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
| Shaper | 2hump_ei | ~~mzv~~ → **o recomendado pela medição** | a troca para mzv foi **desfeita** na mesma noite: ver "Vibração na base 1.1.0.74" abaixo |

## Hi Painel

- **Imprimir**: "Medir malha e imprimir" (padrão), "Usar malha salva", "Descartar pendências" (se houver calibração não salva) e Voltar.
- **Nivelamento e malha**: temperatura da mesa editável (padrão = filamento ativo); mostra em que temperatura a malha salva foi medida.
- **Calibrar tudo**: home → vibração → PID bico → PID mesa → malha (na mesa do filamento ativo).
- **Vibração**: usa o shaper recomendado pela medição (a preferência por mzv foi retirada). Entende o formato novo da base 1.1.0.74.

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
- **Monitor da Creality** (`/usr/bin/Monitor`, achado no boot de 13/11): ele confere os processos com `ps | grep` e religa `web-server`, `webrtc` (por `/etc/init.d/webrtc restart`) e `webrtc_local` se não os vê. Desligar no init não basta. Por isso `/usr/bin/web-server` e `/usr/bin/webrtc` viram programas vazios (`while sleep`), como o `app-server`, e os originais ficam em `.orig`. O Monitor encontra os processos, e nenhuma porta abre. Isso está no build (`dentro.sh`) e no `hi-ajustes`. Testado ao vivo: o Monitor religou os vazios (1,5 MB cada), as portas ficaram só 2222/4408/7125/8000 e a câmera respondeu.

## Vibração na base 1.1.0.74 (13/11/2026, à noite)

- A base 1.1.0.74 troca o cálculo do shaper do Klipper por um módulo próprio, em C. No console aparece só o melhor shaper:
  `best shaper.name 2hump_ei, best shaper.freq 50.8Hz, best shaper.vibrs 0.0179, ... best shaper.max_accel 2869`
  - As linhas `Fitted shaper '...'` do Klipper padrão não aparecem mais.
  - O `klippy.log` tem uma linha por shaper (nível ERROR), mas os nomes estão deslocados uma posição e os campos trocados. Não use essas linhas.
  - `variable_autotune_shapers` não é respeitado: ele testa zv, mzv, ei, 2hump_ei e 3hump_ei.
- O CSV `/tmp/shaper_calc_output_<eixo>-<data>.csv` traz freq, `psd_sum`, `psd_x/y/z`, `after_shaper` e a resposta de cada shaper (×10000).
  - Com a conta do Klipper (limiar = pico/20), a vibração que sobra bate exatamente com a da Creality.
- Medição de 13/11 às 19h08, pela tela da Creality:

| Eixo | zv | mzv | ei | 2hump_ei | 3hump_ei | escolhido |
|---|---|---|---|---|---|---|
| X (pico 48,9 Hz) | 26,2% | 7,9% | 7,5% | 1,8% | 0,5% | 2hump_ei 50,8 Hz (aceleração segura 2869) |
| Y (pico 38,2 Hz) | 31,5% | 12,7% | 12,9% | 6,7% | 6,3% | 3hump_ei 52,4 Hz (aceleração segura 1890) |

- **Conclusão:** nesta máquina, o mzv deixa 4 a 6 vezes mais vibração. Ele também ficaria numa frequência mais baixa (~41 Hz no X e ~35 Hz no Y), então aceita **menos** aceleração. A preferência por mzv foi retirada do Hi Painel.
- O Hi Painel lê o formato novo e desenha o gráfico pelo CSV novo.
- **Bug corrigido:** o nome do shaper cabia em 7 letras, então "2hump_ei" virava "2hump_e", um nome inválido no printer.cfg.

### Algoritmo oficial do Klipper × Creality (mesma medição)

`scripts/shaper_oficial.py` roda o `shaper_calibrate.py` oficial (que continua na Hi) sobre o CSV da Creality:

| Filtro | X: vibração | X: acel. segura | Y: vibração | Y: acel. segura |
|---|---|---|---|---|
| mzv | 8,4% (44,8 Hz) | 5900 | 13,9% (38,0 Hz) | 4300 |
| ei | 7,9% (55,2 Hz) | 5700 | 14,0% (48,6 Hz) | 4400 |
| 2hump_ei | 2,0% (52,8 Hz) | 3100 | 7,4% (46,4 Hz) | 2300 |
| 3hump_ei | 0,4% (51,2 Hz) | 1800 | 5,9% (50,6 Hz) | 1700 |

- **O oficial recomenda X 2hump_ei 52,8 e Y mzv 38,0.** Um filtro mais pesado só ganha se a nota for 20% melhor.
- **A Creality escolhe X 2hump_ei 50,8 e Y 3hump_ei 52,4.**
  - O 3hump no Y só é seguro até ~1700–1900 mm/s². A 4300, que é o limite da Hi, arredonda os cantos em ~0,3 mm.
- **Salvo em 13/11, escolha do usuário ("peça linda e rápida"):** X 2hump_ei 52,8 Hz, Y mzv 38,0 Hz. Backup do printer.cfg: `printer.cfg.antes-shaper-*`.
- **Atenção:** a vibração pela tela da Creality grava sozinha o resultado dela. Pelo Hi Painel isso não acontece mais (ver abaixo).

### Hi Painel: três perfis com o cálculo oficial

- Depois de medir, o painel roda `/usr/share/hi/shaper_oficial.py` na própria Hi (com `nice`, ~50 s por eixo, numpy 1.20 da Hi) sobre o CSV da medição.
  - A tela mostra "Calculando (Klipper oficial)".
  - Se o cálculo falhar, fica o resultado da Creality.
- Perfis, escolhidos por eixo sobre a tabela do Klipper oficial:
  - **Alta:** o filtro de menor vibração. Paredes mais lisas; acelere menos.
  - **Média** (padrão): o recomendado pelo Klipper oficial.
  - **Rápida:** entre mzv e ei, o de menor nota do Klipper. Cantos vivos e mais aceleração.
- A tela mostra a aceleração segura do perfil (o menor valor entre os dois eixos).
  - Tocar no cartão do eixo troca o filtro daquele eixo à mão.
  - "Aplicar e salvar" grava no printer.cfg e reinicia o Klipper.
- O perfil escolhido fica guardado (`vib_perfil`).
  - O **Calibrar tudo** usa esse perfil: calcula enquanto os PIDs rodam e, depois do "Salvar tudo", troca a escolha da Creality pelo perfil e reinicia mais uma vez.
  - Os CSVs somem quando o Klipper reinicia, por isso o cálculo é feito antes.
- Números de 13/11:

| Perfil | X | Y | Aceleração segura |
|---|---|---|---|
| Alta | 3hump_ei 51,2 (0,4%) | 3hump_ei 50,6 (5,9%) | 1700 |
| Média | 2hump_ei 52,8 (2,0%) | mzv 38,0 (13,9%) | 3100 |
| Rápida | ei 55,2 (7,9%) | ei 48,6 (14,0%) | 4400 |

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
- Uma medição nova de vibração pelo painel (formato novo, gráfico e Aplicar).
- `G29 BED=` com temperatura diferente de 60 °C (esperando a mesa aquecer).
- ~~Um boot completo~~: feito em 13/11 às 19h. O SSH subiu na 2222, a 22 ficou fechada e o NTP.br funcionou. **O web-server e o webrtc voltaram**, porque o Monitor da Creality os religa. Corrigido com programas vazios: ver abaixo. Falta mais um boot para confirmar.
- Uma queda de energia real durante a impressão.
