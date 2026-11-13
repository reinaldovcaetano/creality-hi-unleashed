# Mudanças

## Não lançado (depois da 1.2.0.1-alpha)

Já aplicado na impressora de teste e no pacote. Vai na próxima imagem.

### Calibrações
- `[z_tilt] retry_tolerance` 0,1 → 0,03; `[bed_mesh] probe_count` 9×9 → 7×7; limpeza do bico 10 → 6 passadas (`scripts/otimiza_calib.py`).
- `G29 BED=<temp>`: malha medida na temperatura de uso. Sem `BED`, fica igual ao original (`scripts/macro_g29_temp.py`).
- `START_PRINT` com `variable_skip_mesh`: permite imprimir com a malha salva.
- Vibração: prefere mzv a 2hump_ei.
- Nada de localização foi alterado (G28, z_align, z_offset, prtouch, NEXT_HOMEZ_NACCU).

### Ventoinha da peça
- M106 escala por `fan0_max_pct`. O padrão é 50%, igual à Creality (`scripts/macro_ventoinha.py`).

### Hi Painel
- Menu Imprimir: medir a malha, usar a salva ou descartar pendências.
- Nivelamento e malha pela G29, com a temperatura editável, a checagem do filamento e a proteção das pendências.
- Calibrar tudo: home → vibração → PID do bico → PID da mesa → malha.
- Carregar filamento pela `LOAD_MATERIAL`.
- Ventoinha:
  - tela de máximo, que desfaz o teste ao sair sem salvar;
  - deslizador de velocidade real.
- Tela "Reiniciando o Klipper", que espera ele ficar pronto.
- O painel consulta mais devagar quando está escondido.

### Sistema
- O log do Klipper não grava as consultas de leitura. Caiu de ~250 para ~40 MB/dia (`scripts/webhooks_menos_log.py`).
- Sem `web-server` e `webrtc` da nuvem Creality: o app Creality Print/Cloud deixa de ver a impressora. A câmera do Fluidd continua.
- SSH só na porta 2222.
- Relógio pelo NTP.br.
- Na Hi já instalada, o `hi-ajustes` aplica as mudanças de sistema no boot.

Detalhes em `docs/tecnico-pt/calibracoes.md` e `docs/tecnico-pt/hi-painel.md`.
