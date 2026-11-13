#!/usr/bin/env python3
"""Projeto Hi: troca a escala da ventoinha da peça no M106 por uma velocidade máxima ajustável
(PRINTER_PARAM.fan0_max_pct; 50 = igual à Creality, que dividia o valor por 2). Uso: macro_ventoinha.py <gcode_macro.cfg> [pct]"""
import re, sys
p = sys.argv[1]; pct = int(sys.argv[2]) if len(sys.argv) > 2 else 50
s = open(p).read()
# 1) variável nova em PRINTER_PARAM (logo após fan0_min)
if "variable_fan0_max_pct:" not in s:
    s = re.sub(r"(\nvariable_fan0_min:[^\n]*\n)", r"\1variable_fan0_max_pct: %d    # Ajuste Hi Painel: velocidade máxima real da ventoinha da peça (%%); 50 = Creality\n" % pct, s, count=1)
else:
    s = re.sub(r"variable_fan0_max_pct: *\d+", "variable_fan0_max_pct: %d" % pct, s, count=1)
# 2) bloco de escala do fan 0 no M106 (o linear dos ajustes de 10/11 ou o original da Creality)
ini = s.index("[gcode_macro M106]")
fim = s.index("###########################  fan 1 2", ini)
corpo = s[ini:fim]
a = corpo.index("  {% if tmp > 0 %}")
novo = """  {% if tmp > 0 %}
  {% if fan == 0 %}
  # Ajuste Hi Painel: escala pela velocidade máxima (PRINTER_PARAM.fan0_max_pct); 50 = igual à Creality (dividia por 2)
  {% set value = tmp * printer["gcode_macro PRINTER_PARAM"].fan0_max_pct|float / 100 %}
  {% if value < printer["gcode_macro PRINTER_PARAM"].fan0_min %}
    {% set value = printer["gcode_macro PRINTER_PARAM"].fan0_min %}
  {% endif %}
  {% endif %}
  {% endif %}
"""
s = s[:ini] + corpo[:a] + novo + s[fim:]
open(p, "w").write(s)
print("ok", p, "fan0_max_pct =", pct)
