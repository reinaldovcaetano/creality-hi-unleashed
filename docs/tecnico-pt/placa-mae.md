# Placa-mãe da Hi (fotos da internet, 12/11/2026)

As fotos estão em `fotos-placa/`:
- `wiki/`: wiki da Creality. `黑屏01.png` é a placa montada em alta resolução (3500×2298), `主板套件.png` é a placa solta.
- `3dprima/` e `dremc/`: guias de troca de placa. `3dprima/foto-18.jpg` tem os conectores numerados e `foto-09.jpg` mostra a placa instalada.
- `recortes/`: os detalhes ampliados.

## Identificação

- **Serigrafia:** `CR4NU200360C21` na placa da foto do wiki. O firmware diz `CR4NU200360C20`.
- **Kit de reposição:** "Silent mainboard kit **CR4NU200360C22**_32_M6_GD32F303RET6". Há uma etiqueta "M6" na placa.
- Todas usam a mesma imagem de firmware `CR4NU200360C20`.
- **Chips visíveis:**
  - **Allwinner T113-i** e a memória ao lado;
  - módulo Wi-Fi **AIC8800**, com antena IPEX;
  - **GD32F303** (MCU da placa-mãe, perto do "M6");
  - 4 chips quadrados de driver de motor. Pelo Klipper, Z, Z1 e extrusora usam TMC2208; X e Y são os motores em malha fechada, com driver próprio no motor, ligados pelo 485.
- **Acesso:** a placa fica embaixo. Tirar a tampa inferior (**9 parafusos, chave hex de 2 mm**).
  - Muitos conectores são **colados com cola amarela**.
  - Há uma **manta térmica** atrás da placa.

## Pontos de acesso achados

| O quê | Onde (com a placa na orientação de `黑屏01.png`, T113 à direita) | Detalhe |
|---|---|---|
| **Botões RESET e LOAD (FEL)** | canto superior direito, logo acima do T113. São 2 botões táteis lado a lado | Serigrafia (de cabeça para baixo) **"RES"** no da esquerda e **"LOAD"** no da direita. O **LOAD é o FEL/modo de gravação** (na K2 Plus é "FEL") |
| **Pontos da console serial (UART0)** | borda direita, ao lado do T113: **3 furos dourados em coluna**, o de baixo quadrado (pino 1) | Muito provável TX/RX/GND do UART0 (PB0/PB1, 115200). **Medir com multímetro antes de ligar:** o GND tem continuidade com o terra da placa. A tensão é 3,3 V |
| **Header de 5 furos "J?"** | meio da placa, acima do chip GD32 | Serigrafia: **VCC, DIO, CLK**, … É o **SWD** (SWDIO/SWCLK) do **GD32F303** da placa-mãe. Serve para ler ou gravar o firmware do MCU com um ST-Link/J-Link |
| **USB de gravação** | parte de baixo, acima da USB-A, numa área marcada | Contorno de **Micro USB** (OTG do T113, usado pelo PhoenixSuit) |
| **USB-A** | borda inferior | Pendrive e câmera (USB host) |
| Conector "485" | parte de baixo, à esquerda | Barramento 485 dos motores X/Y e do CFS |
| "ADC BED" / "BED-C" | parte de baixo | Sensor de pressão da mesa (strain gauge) e mesa |
| Tela | conector FPC grande no canto inferior direito (cabo dourado) | LCD GC9307 por SPI + touch |

**Modo de gravação (FEL):** com a placa ligada pela fonte, **segurar LOAD, apertar e soltar RES, soltar LOAD**. Ligar o Micro USB no PC. Deve aparecer `1f3a:efe8`.
