#!/usr/bin/env python3
"""Projeto Hi: a macro G29 da Creality aceita BED=<temp> (temperatura da mesa na medição da malha).
Sem BED continua igual à original (60 °C, sem esperar). Com BED, espera a mesa chegar (M190) antes de medir.
Uso: macro_g29_temp.py <gcode_macro.cfg>"""
import sys
p = sys.argv[1]; s = open(p).read()
ini = s.index("[gcode_macro G29]\n"); fim = s.index("\n[", ini + 5); corpo = s[ini:fim]
if "params.BED" in corpo: print("já aplicado", p); sys.exit(0)
corpo = corpo.replace("gcode:\n  M140 S60\n",
    "gcode:\n  # Ajuste Hi Painel: BED=<temp> mede a malha na temperatura da mesa que vai ser usada (sem BED = 60, original)\n"
    "  {% set bed = params.BED|default(60)|int %}\n  M140 S{bed}\n", 1)
corpo = corpo.replace("  BED_MESH_PROFILE LOAD=\"default\" \n  BED_MESH_CALIBRATE",
    "  BED_MESH_PROFILE LOAD=\"default\" \n  {% if params.BED %}\n  M190 S{bed}\n  {% endif %}\n  BED_MESH_CALIBRATE", 1)
assert "params.BED|default" in corpo and "M190 S{bed}" in corpo, "formato inesperado"
open(p, "w").write(s[:ini] + corpo + s[fim:]); print("ok", p)
