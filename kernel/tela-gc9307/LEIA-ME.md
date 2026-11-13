# Driver da tela GC9307 da Creality Hi, reconstruído (12/11/2026)

- **De onde veio:** o kernel da Creality 1.1.0.74 (`kernel/Image-creality-1.1.0.74`) foi convertido em ELF com símbolos (vmlinux-to-elf) e desmontado.
  - O painel em uso, conforme o device tree, é `gc9307_9bit_spi_tjc_240_320`, com struct em `c0c1bc20`.
  - Desmontagem das três variantes em `variante1/2/3-disasm.txt`.
- **Como funciona:**
  - SPI de 9 bits por software (bit-bang) nos GPIOs `lcd_gpio_N`: 0 = reset, 1 = clock, 2 = dados, 3 = CS. O bit 8 indica comando (0) ou dado (1), com 2 µs por meio ciclo.
  - Ligar: reset alto, 50 ms, baixo, 50 ms, alto, 180 ms; depois `pin_cfg`.
  - Inicialização: 87 escritas (`panel_init_decomp.c`), terminando em 0x11 (sleep out) + 120 ms, 0x29 e 0x2C.
  - Abrir: power_on(0) → panel_init(200) → tcon_enable(150) → bl_open(0).
  - Fechar: bl_close(0) → 0x28 + 50 ms → 0x10 + 120 ms → tcon_disable(0) → panel_exit(100) → power_off(0).
  - Gamma identidade e cmap padrão Allwinner.
- **Fonte:** `gc9307_9bit_spi.c/.h`, integrado em `drivers/video/fbdev/sunxi/disp2/disp/lcd/` (Kconfig `LCD_SUPPORT_GC9307_9BIT_SPI_TJC_240_320`, Makefile, panels.c/h) no volume `hi-kernel`.
- **Validação:**
  - A sequência compilada do nosso `LCD_panel_init` é **idêntica** à da Creality (100 de 100 operações: GPIO, tempos, escritas e printk), conferida automaticamente.
  - A struct do painel caiu em `c0c1bc24`, 4 bytes depois da da Creality.
- **Ainda não feito:** as variantes `_2_8` e `ue_240_320_3_5` (outras telas de Hi). Só a `tjc_240_320` está no device tree desta máquina.
