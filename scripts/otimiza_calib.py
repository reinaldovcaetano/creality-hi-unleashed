#!/usr/bin/env python3
"""Projeto Hi: otimizações de calibração (13/11/2026). Idempotente. Uso:
  otimiza_calib.py printer.cfg     -> z_tilt retry_tolerance 0.03, malha 7x7, limpeza do bico 6 passadas
  otimiza_calib.py gcode_macro.cfg -> START_PRINT aceita "usar malha salva" (variable_skip_mesh) e END_PRINT zera
Não mexe em home, sensores ópticos do Z, z_offset nem no sensor de pressão."""
import re, sys
p = sys.argv[1]; s = open(p).read(); o = s
def sec(s, nome, chave, valor, nota):
    i = s.index("[%s]" % nome); j = s.find("\n[", i + 1); j = len(s) if j < 0 else j
    corpo = s[i:j]
    novo = re.sub(r"(?m)^%s\s*:.*$" % re.escape(chave), "%s: %s  # Ajuste Projeto Hi: %s" % (chave, valor, nota), corpo, count=1)
    return s[:i] + novo + s[j:]
if p.endswith("printer.cfg"):
    s = sec(s, "z_tilt", "retry_tolerance", "0.03", "era 0.1 (nivela os dois Z bem mais fino)")
    s = sec(s, "bed_mesh", "probe_count", "7,7", "era 9,9 (mesa suave: ~40% mais rápido)")
    s = sec(s, "nozzle_clear", "clear_cnt", "6", "era 10 passadas na limpeza do bico")
else:
    ini = s.index("[gcode_macro START_PRINT]\nvariable_prepare: 0\n")
    if "variable_skip_mesh" not in s:
        s = s.replace("[gcode_macro START_PRINT]\nvariable_prepare: 0\n",
            "[gcode_macro START_PRINT]\nvariable_prepare: 0\n"
            "# Ajuste Projeto Hi: skip_mesh=1 (posto pelo Hi Painel) usa a malha salva em vez de medir de novo\n"
            "variable_skip_mesh: 0\n", 1)
        a = "    NEXT_HOMEZ_NACCU\n    G28 Z\n    BED_MESH_CALIBRATE\n    CXSAVE_CONFIG\n"
        assert a in s, "START_PRINT em formato inesperado"
        s = s.replace(a, "    NEXT_HOMEZ_NACCU\n    G28 Z\n"
            "    {% if printer['gcode_macro START_PRINT'].skip_mesh|int == 1 %}\n"
            "      BED_MESH_PROFILE LOAD=\"default\"\n"
            "      {action_respond_info(\"Hi: usando a malha salva (sem medir)\")}\n"
            "    {% else %}\n"
            "      BED_MESH_CALIBRATE\n      CXSAVE_CONFIG\n"
            "    {% endif %}\n"
            "    SET_GCODE_VARIABLE MACRO=START_PRINT VARIABLE=skip_mesh VALUE=0\n", 1)
        s = s.replace("[gcode_macro END_PRINT]\ngcode:\n",
            "[gcode_macro END_PRINT]\ngcode:\n  SET_GCODE_VARIABLE MACRO=START_PRINT VARIABLE=skip_mesh VALUE=0\n", 1)
print(("alterado " if s != o else "já estava ") + p); open(p, "w").write(s)
