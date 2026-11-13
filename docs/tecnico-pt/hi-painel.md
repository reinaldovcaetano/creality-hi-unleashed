# Plano: "Hi Painel", a tela própria de calibração e filamentos

Planejado em 13/11/2026. **Status: PLANO (nada implementado ainda).**

## O que o usuário pediu

- **Uma tela de calibração com todas as calibrações possíveis.** Em cada uma, mostrar o valor padrão, poder **editar o valor** ou deixar o padrão, e **confirmar**. Exemplos:
  - PID do bico e da mesa, com a temperatura escolhida;
  - **vibração (input shaper)**, mostrando o resultado num gráfico, com a **velocidade/aceleração ideal de cada eixo**;
  - e as demais: malha, Z, PA…
- **Filamentos:** poder **acrescentar um filamento novo direto na impressora** (ex.: "MasterPrint PETG"), com temperaturas, ventoinha, PA e fluxo.

## Resposta curta: dá para fazer? **SIM**, com uma tela nossa

- **A tela da Creality não dá para editar.**
  - O `display-server` é um binário **fechado** de 11 MB, feito em **LVGL 8.x**.
  - Só as imagens (245 PNGs em `/etc/sysConfig/UIResource/`) ficam fora do programa.
  - Telas, botões, textos e lógica estão dentro do binário. Não dá para acrescentar uma tela nem um campo editável nele.
- **Uma tela nossa é viável agora.** Temos:
  - o kernel;
  - o driver da tela GC9307, recriado por nós;
  - o touch (Goodix, `/dev/input/event0`);
  - SSH e Moonraker modernos.
- Tudo o que foi pedido existe no Klipper desta Hi, ou dá para fazer em cima dele (ver "Fatos levantados").

## Fatos levantados na Hi (13/11/2026)

| Item | O que tem |
|---|---|
| Tela | `/dev/fb0` com 240×320 visível (240×640 virtual = duas páginas), **32 bits**. Painel GC9307 de 2,8". |
| Touch | `gt9xxnew_ts` (Goodix) em `/dev/input/event0`. |
| Tela da Creality | `/usr/bin/display-server` (LVGL 8.x), iniciado pelo procd em `/etc/init.d/app` (S99). Dá para parar e religar com `/etc/init.d/app stop` e `start`. |
| Acelerômetros | **Dois LIS2DW**, um por eixo (`[lis2dw x]`, `[lis2dw y]`). O `[resonance_tester]` tem max_freq 90 e accel_per_hz 75. O shaper da Creality usa uma biblioteca C própria (ffi) para calcular. |
| Calibrações no Klipper | `PID_CALIBRATE` (bico e mesa), `SHAPER_CALIBRATE` / `TEST_RESONANCES`, `BED_MESH_CALIBRATE`, `Z_TILT_ADJUST`, `prtouch_v3` (sensor de pressão da mesa e Z), motores em **malha fechada** (`motor_control`, que mede o erro de rastreio). |
| PA automático | A Creality tem PA por **strain gauge** (`/etc/sysConfig/defData/Auto_strain_gauge_pa_compute_config.json`, com parâmetros por material: Hyper PLA, PETG, ABS…). O módulo é fechado; é preciso descobrir o comando que dispara. |
| Banco de filamentos | `material_database.json`, formato de perfil do OrcaSlicer (`kvParam`: temperaturas, ventoinha, PA, fluxo…). **O `master-server` rejeita entrada nova** (visto em 10/11): só aceita editar uma existente. |
| Comunicação | Moonraker em `127.0.0.1:7125`, com API JSON-RPC e websocket. Dá para mandar G-code, assinar o estado e receber as respostas (`notify_gcode_response`) com o resultado das calibrações. |

## Arquitetura proposta

- **Programa "hi-painel"** em C com **LVGL 8.3**, a mesma família da tela da Creality.
  - Desenha direto no `/dev/fb0` (driver fbdev) e lê o touch pelo evdev.
  - É leve: cerca de 1–3 MB de binário e menos de 10 MB de RAM.
  - É compilado no container `kbuild` (GCC ARM), e ganha um serviço novo no `compose.yaml`.
- **Conversa com o Klipper só pelo Moonraker**, via websocket local:
  - manda os comandos (`printer.gcode.script`);
  - lê as temperaturas e o estado (`printer.objects.subscribe`);
  - captura os resultados das calibrações nas respostas de G-code e nos arquivos CSV que o Klipper gera em `/tmp`.
- **Convive com a tela da Creality** (fase 1). A tela original continua sendo a padrão, porque faz coisas que não vamos refazer agora: atualização por pendrive, Wi-Fi pela tela, self-check e códigos de erro.
  - **Para abrir o Hi Painel:** um gesto no touch, por exemplo segurar 3 s no canto superior. O hi-painel fica em segundo plano lendo o touch sem bloqueá-lo. Também dá para abrir por um botão/macro `HI_PAINEL` no Fluidd.
  - **Ao abrir:** pausa a tela da Creality (`/etc/init.d/app stop`, ou SIGSTOP) e assume a tela.
  - **"Voltar":** devolve a tela e religa o `display-server`.
- **Segurança:**
  - não deixa calibrar durante uma impressão;
  - todo aquecimento tem tempo limite;
  - antes de gravar qualquer valor, mostra "atual → novo";
  - gravar usa `SAVE_CONFIG` (avisa que o Klipper reinicia);
  - backup do `printer.cfg` antes de cada gravação.
- **Histórico:** cada calibração fica salva em `/mnt/UDISK/printer_data/hi/calibracoes.json`, com data, valores antigos e novos. A tela mostra "última calibração em…".

## Telas planejadas

### 1. Calibrações (menu)

| Calibração | O que a tela faz | Comando no Klipper |
|---|---|---|
| **PID do bico** | Campo da temperatura (padrão: a do filamento ativo, ou 230 °C) com ± ou teclado numérico. Iniciar → gráfico da temperatura ao vivo → resultado Kp/Ki/Kd ao lado dos atuais → **Salvar / Editar valores / Descartar**. | `PID_CALIBRATE HEATER=extruder TARGET=<t>` + `SAVE_CONFIG` |
| **PID da mesa** | Igual, com padrão de 60 °C (ou o do filamento). | `PID_CALIBRATE HEATER=heater_bed TARGET=<t>` |
| **Vibração / input shaper** | Escolher eixo X, Y ou ambos → teste → **gráfico de frequência** (curva do CSV) → tabela de shapers (tipo, Hz, vibração restante, suavização, **aceleração máxima recomendada**) com o recomendado marcado → **Aplicar e salvar** (ou escolher outro). Mostra também a **aceleração ideal por eixo** e sugere `max_accel` = o menor dos dois eixos. | `SHAPER_CALIBRATE AXIS=X/Y` (ou `TEST_RESONANCES` + o cálculo) |
| **Velocidade ideal por eixo** | **Pesquisa (fase 6).** Teste de velocidade crescente em X e Y, usando os motores em **malha fechada** (`motor_control` mede o erro de rastreio) para achar onde começa a perder passo. A tela mostra a "velocidade máxima segura" de cada eixo, com uma margem. | macro nossa + leitura do erro do `motor_control` |
| **Malha da mesa** | Aquece (temperatura editável) → malha → **mapa de cores** 2D com a variação máxima → salvar o perfil. | `BED_MESH_CALIBRATE` + `SAVE_CONFIG` |
| **Nivelamento Z (2 motores)** | Roda e mostra a diferença antes e depois. | `Z_TILT_ADJUST` |
| **Z-offset** | Ajuste fino com ± 0,01 mm e confirmação. | sensor de pressão (`prtouch_v3`) / `SET_GCODE_OFFSET` + gravação |
| **Pressure Advance** | Valor por filamento (editável). Botão **Automático**, se der para usar o PA por strain gauge da Creality (investigar o comando). Alternativa: gerar e imprimir uma torre de PA. | `SET_PRESSURE_ADVANCE` / módulo da Creality |
| **Calibrar tudo** | Sequência guiada: PID do bico → PID da mesa → shaper → malha, salvando no final. | combinação |
| **Histórico** | Lista das calibrações, com data e valores. | arquivo nosso |

### 2. Filamentos

- **Lista** dos filamentos: os de fábrica da Creality mais os nossos.
- **Novo filamento:** nome (ex.: "MasterPrint PETG"), marca, tipo, temperaturas do bico (1ª camada e demais), mesa, ventoinha (mín./máx.), PA, fluxo, velocidade volumétrica máxima e retração. O padrão vem do tipo (PLA, PETG…), editável campo a campo, com confirmação.
- **Usar:** aplica as temperaturas e o PA na hora (`M104`/`M140`/`SET_PRESSURE_ADVANCE`) e marca como filamento ativo, que vira o padrão das calibrações.
- **Onde salvar:**
  1. **No nosso banco** `/mnt/UDISK/printer_data/hi/filamentos.json`. Garantido, e a gente controla.
  2. **No banco da Creality** (`material_database.json`), para aparecer também na tela original e no Creality Print. **Investigar:** o `master-server` recusa entrada nova (10/11). Caminhos possíveis:
     - (a) descobrir a validação: faixa de ID, `engineVersion`, assinatura;
     - (b) usar posições "de usuário" existentes;
     - (c) gravar com o master-server parado e reiniciar.
  - Se nada disso der, o filamento fica só no Hi Painel.

### 3. Outras telas (simples)

- Início: temperaturas, progresso da impressão, botões grandes.
- Mover eixos e Home.
- Rede (5 GHz, sinal, IP).
- Versão e logs.
- Voltar para a tela Creality.

## Fases

| Fase | Entrega | Como testar |
|---|---|---|
| **F0: investigação** | Como parar e religar a tela da Creality sem efeitos colaterais; regras do banco de materiais (master-server); comando do PA por strain gauge; formato das respostas do SHAPER_CALIBRATE desta versão da Creality (ffi). | SSH na Hi, sem instalar nada |
| **F1: esqueleto** | hi-painel compilado: desenha na tela, touch funcionando, gesto de abrir, "Voltar" religa a Creality. | **Ao vivo por SSH** (roda de `/tmp`, sem pacote); se travar, reinicia |
| **F2: PID** | Bico e mesa: temperatura editável, gráfico ao vivo, resultado, editar, salvar e histórico. | Na Hi, com você |
| **F3: input shaper** | Gráfico de frequência, tabela de shapers, aceleração ideal por eixo, aplicar/salvar. | Na Hi |
| **F4: malha, Z tilt, Z-offset** | Mapa de cores, ajustes. | Na Hi |
| **F5: filamentos** | Banco próprio, adicionar/editar/usar, integração com o banco da Creality (se a F0 liberar). | Na Hi |
| **F6: PA e velocidade ideal** | PA automático (strain gauge) ou torre; teste de velocidade com malha fechada. | Na Hi |
| **F7: pacote** | hi-painel na OTA (tentativa única), docs nos 4 idiomas, publicado no repositório. | Pendrive |

**Desenho antes de código:** as telas são desenhadas primeiro no **simulador do LVGL no Mac**, em 240×320, e as capturas vão para você aprovar. Só depois disso elas vão para a impressora.

## Riscos e limites

- **Tela pequena (2,8", 240×320):**
  - gráficos simples, com uma informação por tela;
  - teclado numérico grande para editar valores;
  - nada de texto longo.
- **A tela da Creality parada** enquanto o Hi Painel está aberto: o que ela faz (avisos, erros) não aparece até voltar. Por isso o Hi Painel não pode ficar aberto durante uma impressão; pode, no máximo, só mostrar o status.
- **`SAVE_CONFIG` reinicia o Klipper:** só fora de impressão, com confirmação.
- **Shaper da Creality:** usa uma biblioteca C própria. O formato do resultado pode diferir do Klipper padrão; a F0 confere.
- **Banco de filamentos da Creality:** pode não aceitar entrada nova (ver F5). Nesse caso o filamento fica só no Hi Painel e é aplicado pelas macros.
- **Velocidade ideal:** o Klipper calcula a aceleração máxima, não a velocidade. A velocidade por eixo depende do teste com malha fechada (pesquisa da F6).

## Decisões em aberto (para o usuário)

1. Como abrir o Hi Painel: gesto no canto, botão no Fluidd, ou os dois?
2. Por qual calibração começar? (Sugestão: PID, depois shaper.)
3. Visual: cores e logo (dá para usar o nível 1, trocar os PNGs, junto).
4. Idioma: só português, ou português e inglês?

---

## Diretrizes do usuário (13/11/2026)

- Ordem das calibrações: tanto faz. **Tela moderna.**
- **Não pode afetar a queda de energia, a recuperação etc.**
- **Precisa ter tudo o que a tela da Creality tem, só que melhorado.**

## F0: investigação (13/11/2026, feita)

### Queda de energia

O fluxo mora no **Klipper da Creality** (`virtual_sdcard.py` + `pause_resume.py`), não na tela.

- **Durante a impressão:** `record_power_loss_info` grava a posição na **EEPROM bl24c16f** (por isso o I2C/PG9 era crítico) e o nome do arquivo em disco.
- **No boot:**
  - a tela consulta o endpoint `pause_resume/check_continue_print_state` (retorna `file_state`/`eeprom_state` e marca `print_stats.power_loss = 1`);
  - mostra "Detected unfinished task, do you wish to continue printing";
  - se a resposta for não, chama `pause_resume/cancel_continue_print`.
- **A chave liga/desliga** fica em `user_print_refer` (`power_loss.switch`).
- **Conclusão:** uma tela nossa repete esse fluxo exatamente pelos mesmos endpoints do Klipper, pelo socket `/tmp/klippy_uds`. Enquanto o Hi Painel **convive** com a tela da Creality (fase atual), o fluxo continua 100% o original.

### Tela da Creality

- `display-server`: LVGL 8.x, `/dev/fb0` (ARGB8888, stride 960), touch em `/dev/input/event0`.
- Iniciada pelo procd em `/etc/init.d/app`.
- Mais de 2226 textos, em vários idiomas e para vários modelos. Parte deles não se aplica à Hi: CFS, laser, câmera com IA.
- **Inventário para cobrir na tela nova:**
  - status;
  - imprimir (arquivos, histórico, prévia);
  - controle (mover, temperaturas, ventoinhas, luz);
  - filamento (carregar, descarregar, cortar);
  - calibrações (auto nivelamento, malha, tensão da correia, vibração, fluxo);
  - manutenção diária;
  - câmera;
  - rede/Wi-Fi;
  - idioma;
  - som/brilho;
  - exportar log;
  - atualização por USB;
  - sobre/versão;
  - mensagens de erro (`error_code_map.json`);
  - queda de energia.
- **Captura do fb0 com a tela em descanso:** só preto, alfa 255.

## F1 (parte 1): visual aprovado para revisão (13/11/2026)

- Projeto em `hi-painel/`:
  - `lib/lvgl` (v8.3.11, commit 74d0a816);
  - `fontes/hi_font_{12,14,16,20,28,40}.c` (Montserrat Medium com Latin-1, acentos em PT, mais os ícones do LVGL, gerados com o lv_font_conv 1.5.2);
  - `src/lv_conf.h` (32 bits, 1 MB);
  - `src/ui/tema.c|h` (tema escuro, componentes);
  - `src/ui/telas.c|h` (9 telas);
  - `mock/mock.c` (renderiza sem hardware e grava PNG).
- **Gerar as imagens:**
  ```bash
  docker run --rm -v "$PWD":/h -w /h ghcr.io/reinaldovcaetano/creality-hi-unleashed-kbuild:alpha sh -c 'make -f Makefile.mock && ./build/mock/hi-mock mock/png'
  ```
  (dentro de `hi-painel/`)
- **Imagens:** `hi-painel/mock/png/00-todas.png` (montagem) e uma por tela:
  - início;
  - calibrações;
  - PID (config e resultado);
  - vibração;
  - malha;
  - filamentos;
  - novo filamento;
  - queda de energia.
- **Próximo:**
  1. aprovação do visual pelo usuário;
  2. cross-compilar para ARM (fbdev + evdev), rodar ao vivo na Hi por SSH com o `display-server` parado, e testar touch e desempenho;
  3. depois ligar ao Moonraker/Klipper (PID primeiro).

## Proporção real da tela (13/11/2026)

- O device tree da Creality traz `lcd_x = 240`, `lcd_y = 320`, `lcd_width = 56` mm e `lcd_height = 93` mm. Ou seja, a tela é um **retângulo 5:3** com pixels **~25% mais altos que largos**.
- O `/dev/fb0` vem preto mesmo com a tela acesa: a Creality desenha numa camada do controlador de vídeo (disp2, dmabuf). Por isso a captura da tela original não funciona por `fb0`.
- **Decisão:**
  - as telas são desenhadas numa **tela virtual de 240×400**, a proporção física real (`HI_W`/`HI_H` em `tema.h`);
  - no envio ao display, **cada 5 linhas viram 4** (média), e assim círculos e letras ficam proporcionais;
  - os mocks em `mock/png/` mostram a aparência física real.
- **Telas refeitas para 240×400**, com mais espaço:
  - início com 6 atalhos (Imprimir, Calibrar, Filamentos, Controle, Câmera, Ajustes);
  - calibrações com 7 itens (+ Nivelamento Z, Velocidade ideal);
  - malha com células quadradas;
  - novo filamento com retração e z-hop.

## F1 (parte 2): primeira execução na Hi (13/11/2026, ~14h30), **funcionou**

### A tela real é DEITADA

- Teste com padrão de cores e foto do usuário: a tela é **320×240 deitada, com pixels quadrados**.
- O painel GC9307 (240×320 em pé) está montado girado 90°. O `lcd_width/height` (56×93 mm) do device tree estava errado.
- A tela virtual de 240×400 e a compressão 5→4 foram **abandonadas**.
- **Rotação** (em `main_fb.c`): `fb_y = lx`, `fb_x = 239 - ly`, que acertou de primeira. Para inverter, há as variáveis `HI_INV_X` e `HI_INV_Y`.

### Touch

- O Goodix manda as coordenadas do painel em pé: x de 1 a 238, y de 16 a 274 nos toques do teste. O `EVIOCGABS` devolve 0.
- O mapeamento usa a mesma rotação da tela e funcionou: o usuário navegou e disse "está funcionando e bem rápido".

### Como roda

- `hi-painel/src/main_fb.c` usa LVGL 8.3 direto no `/dev/fb0` (buffer de 60 linhas) e lê o evdev de `/dev/input/event0`.
- Ao iniciar, ele guarda o fb (a imagem da tela Creality) e o devolve ao sair.
- **Lançador** `ferramentas/hi-painel.sh`:
  - pausa o `display-server` com SIGSTOP;
  - garante o SIGCONT com `trap` ao sair, mesmo em erro;
  - **não para os outros serviços** (master, wifi e web, que também saem do `/etc/init.d/app`).
- **Compilação:**
  ```bash
  docker run --rm -v "$PWD":/h -w /h hi-kbuild make -f Makefile.arm
  ```
  Gera `build/arm/hi-painel`, estático, com 1,1 MB.
  - Atenção: o container público `creality-hi-unleashed-kbuild` ainda **não** tem a libc ARM (`libc6-dev-armel-cross`). Acrescentar.
- **Teste:** `scp` para `/tmp` e `(HI_LOG_TOQUE=1 /tmp/hi-painel.sh &)`. O botão "Creality" sai.

### Telas (landscape, navegáveis)

- início (6 atalhos);
- calibrações (lista rolável);
- PID (configurar → resultado);
- vibração (X/Y, recomendado, aceleração);
- malha;
- filamentos;
- novo filamento;
- queda de energia (demonstração).

Os dados ainda são **de exemplo**.

## Telas que faltam (para cobrir tudo da Creality, melhorado)

1. **Dados ao vivo:** ligação ao Moonraker/Klipper (temperaturas, estado, filamento, rede).
2. **Imprimir:** arquivos (pendrive e interno) com miniatura, detalhes (tempo, filamento), histórico e reimprimir.
3. **Imprimindo:** progresso, tempo, camada, miniatura, pausar/cancelar com confirmação, e ajustes ao vivo (velocidade, fluxo, Z-offset fino, temperaturas, ventoinhas).
4. **Controle:** mover X/Y/Z (passos 0,1/1/10/50), home, temperaturas (bico, mesa) com teclado numérico, ventoinhas (peça, auxiliar, gabinete) e luz.
5. **Filamento:** carregar, descarregar e trocar, com aquecimento guiado.
6. **Calibrações reais:** PID, shaper, malha, Z, PA, histórico e "Calibrar tudo" executando de fato.
7. **Ajustes:**
   - Wi-Fi (redes, senha com teclado);
   - idioma;
   - som;
   - brilho;
   - tempo de descanso;
   - unidades;
   - sobre/versão (nossa, Klipper, kernel);
   - exportar logs para o pendrive;
   - atualização por USB;
   - reset;
   - modo da tela padrão (Hi Painel ou Creality).
8. **Câmera:** imagem ao vivo, se o desempenho permitir.
9. **Manutenção:** lembretes (lubrificação, limpeza) e contadores de horas.
10. **Erros e avisos:** textos do `error_code_map.json` traduzidos, com o que fazer.
11. **Queda de energia real:** endpoints do Klipper.
12. **Componentes:** teclado numérico, teclado de texto, diálogos de confirmação, notificações (toasts).
13. **Descanso de tela** e brilho (backlight).

## Beta 1 (13/11/2026, ~15h): tudo ao vivo

### Dados

- `src/dados/moonraker.c` roda 3 threads:
  - polling do estado a cada 0,7 s;
  - fila de comandos HTTP, à parte, porque um G-code longo bloqueia;
  - listas de arquivos e histórico.
- cJSON 1.7.18.
- `src/dados/arquivos.c` cuida de:
  - o banco de filamentos próprio (`/mnt/UDISK/printer_data/hi/filamentos.json`) mais a leitura do banco da Creality;
  - o histórico de calibrações;
  - as preferências;
  - a gravação no SAVE_CONFIG com backup (`printer.cfg.hi-painel-<data>`), comentando a mesma opção no corpo.

### Telas

| Arquivo | Telas |
|---|---|
| `t_inicio` | início |
| `t_imprimir` | arquivos, detalhe, histórico, imprimindo, ajustes ao vivo |
| `t_controle` | controle |
| `t_filamento` | carregar/descarregar, banco, novo/editar/usar |
| `t_calibra` | PID, shaper (CSV), malha, Z tilt, PA, velocidade (placeholder), histórico, calibrar tudo |
| `t_ajustes` | rede, tela, sobre, logs, reiniciar |

### Fim das calibrações

O `idle_timeout` fica "Printing" enquanto o comando roda e depois volta. O resultado é lido do `gcode_store`.

### Modo espera

`hi-painel --espera`: segurar 3 s no canto superior esquerdo abre o painel. Ele usa SIGSTOP/SIGCONT no display-server e EVIOCGRAB no touch. **Ainda não testado.**

### Retorno do usuário

- **Problema:** "o clique vaza para a tela seguinte".
- **Correção:**
  - transição curta de 140 ms (desliza ao avançar e voltar);
  - fade de 100 ms nos diálogos;
  - trava de toque de 260 ms com `lv_indev_wait_release`.
- **Bug corrigido:** duas `lv_scr_load_anim` seguidas com `auto_del` davam segfault. Agora existe `hi_definir_raiz` e só uma troca acontece por vez.

## Hi Painel como tela padrão + atalho "Wiki" (13/11/2026, ~15h10)

- **Decisão do usuário:** o Hi Painel é a tela padrão ao ligar; a tela da Creality só aparece quando ele escolhe. Na Creality, **robozinho → Ajuda → Wiki** volta para o Hi Painel. Ele escolheu "trocar o item Wiki", porque não dá para criar um item novo no binário fechado.
- **Como detectamos a Wiki:** pelo log `/mnt/UDISK/creality/userdata/log/display-server.log`.
  - O robozinho abre `ui_Screen_CustomerService` (pageid 18).
  - A Wiki abre `ui_Screen_CustomerFaq`; em seguida aparece "ui_CustomerFaq_screen_init fail!".
  - O Hi Painel lê o log incrementalmente (`ver_log_creality`) e assume a tela. Assim não depende de coordenada nem de idioma.
- **Boot (`hi-painel --boot`)** assume a tela depois de 8 s com a Creality em `ui_screen_HomeNormal` e o Klipper ready. **Não assume** quando:
  - `print_stats.power_loss = 1`: o "continuar impressão?" da Creality aparece como sempre;
  - existe uma impressão em andamento;
  - há uma OTA no pendrive (`/mnt/exUDISK/CR4NU200360C20_ota_img_V*.img`);
  - passaram 4 min sem chegar na home.
- **Retomar após queda de energia** (para a tela própria no futuro): `SDCARD_PRINT_FILE FILENAME=<arquivo> ISCONTINUEPRINT=1` (`virtual_sdcard.restore_print`). Para descartar, o endpoint do Klipper `pause_resume/cancel_continue_print` pelo socket `/tmp/klippy_uds`.
- **Com o painel aberto e pendrive de atualização inserido:** pergunta se quer abrir a Creality para instalar.
- **Instalação ao vivo** (beta, ainda fora da OTA):
  - binário em `/mnt/UDISK/hi-painel/hi-painel`;
  - serviço `/etc/init.d/hi-painel` (procd, `sleep 15` + `--boot`, respawn), habilitado (S99/K05);
  - **o init fica na camada gravável**; quando entrar no pacote, o `hi-limpa-overlay` troca pela versão do pacote;
  - desligar: `touch /mnt/UDISK/hi-painel/desligado`.
- **Medido:** o Hi Painel usa 2 MB de RAM e menos de 1% de CPU; a tela da Creality usa 22 MB; 367 MB livres. As duas juntas não pesam.

## Beta 1.1 (13/11/2026, ~15h40)

- **Velocidade ideal** funcional. Edita velocidade máxima, aceleração máxima e velocidade de canto. O botão "Usar" puxa o limite seguro pela vibração: `acel_rec_x/y`, gravados ao medir o shaper.
- **Aplicar como padrão:**
  - grava no SAVE_CONFIG `[printer]` (corpo comentado com `#hi-painel:`);
  - troca o teto fixo das macros (`variable_default_acc`, `SET_VELOCITY_LIMIT ACCEL=… ACCEL_TO_DECEL=…`, os três `|float, N]` do M204) via `hi_cfg_macros_acel`, com backup `gcode_macro.cfg.hi-painel-<data>`;
  - aplica na hora com `SET_VELOCITY_LIMIT` e roda `FIRMWARE_RESTART`.
  - Testado em cópias das configs reais: só as linhas pretendidas mudaram.
- **Teste automático de velocidade (malha fechada):** o objeto `motor_control` só expõe `motor_ready`, `is_homing` e `cut`, sem erro de rastreio pela API. Fica para quando houver acesso ao erro (módulo fechado).
- **Revisão:**
  - extrudar/recolher 10 mm (bico ≥ 170 °C);
  - aviso de concluída/cancelada/erro/pausada em qualquer tela (acorda a tela);
  - motivo da pausa na tela da impressão;
  - "Reiniciar a impressora" em Ajustes.
- **Ainda faltam (próximas versões):**
  - miniatura do G-code;
  - lembretes de manutenção (o Klipper da Creality tem `get_maintenance_item`);
  - câmera na tela;
  - PA automático (AUTO_APAX);
  - tela de queda de energia própria;
  - empacotar o Hi Painel na OTA;
  - teste de boot como tela padrão (o usuário ainda não reiniciou).

## Ajustes de 13/11/2026 (~18h às 19h40)

- **Carregar filamento** usa a macro original `LOAD_MATERIAL` (vai à posição de descarte, purga e limpa). Antes o filamento caía na mesa. Os botões +10/-10 também vão antes para a posição de descarte.
- **Nivelamento e malha** roda a `G29` da Creality (limpa o bico, G28 Z, `Z_TILT_ADJUST`, malha, `CXSAVE_CONFIG`).
  - Antes de começar, confere o sensor de filamento do cabeçote. Se houver filamento, abre "Retire o filamento", com atalho para descarregar.
  - Se houver calibração não salva, pergunta se grava junto, descarta ou volta. Isso porque o `CXSAVE_CONFIG` grava **todas** as pendências.
  - A temperatura da mesa pode ser editada (40–110 °C). O padrão é a mesa do filamento ativo, e a tela mostra em que temperatura a malha salva foi medida.
- **Calibrar tudo**, na ordem: home → vibração → PID do bico → PID da mesa → malha (com a mesa já quente). O nível Z é opcional. No fim, avisa que a malha já está ativa e gravada.
- **Ventoinha da peça:**
  - A Creality limita a 50% reais (o M106 divide por 2). O M106 agora escala por `PRINTER_PARAM.fan0_max_pct`, com padrão 50, idêntico à Creality (`scripts/macro_ventoinha.py`).
  - Tela Ajustes > Ventoinha da peça: deslizador de 20 a 100%, Testar/Parar e Salvar. Ao sair sem salvar, desliga o teste (M107, fora de impressão) e devolve o valor salvo.
  - No Controle e no Ajustar ao vivo, a janela da ventoinha mostra a velocidade **real** de 20 a 100% (`SET_PIN PIN=fan0`).
- **Reiniciando o Klipper:** em todo FIRMWARE_RESTART/SAVE_CONFIG, uma tela de espera só sai quando o Klipper volta a ficar pronto. Depois de 3 min, oferece Fechar.
- **Imprimir:** abre um menu com "Medir malha e imprimir" (padrão), "Usar malha salva", "Descartar pendências" (só aparece se houver) e Voltar. Mostra a mesa do arquivo e a da malha salva. Ver `calibracoes.md`.
- **Menu (`hi_menu`):** o texto quebra linha e a altura é medida. Os botões encolhem se não couberem.
- **Desempenho:** quando a tela da Creality está na frente ou em descanso, o painel consulta o Moonraker a cada 4 s (na frente, a cada 0,7 s). A matriz da malha vem a cada 10 consultas.
- **Build:** os Makefiles recompilam quando um `.h` muda. Uma mudança na estrutura de estado tinha dado segfault no mock.
