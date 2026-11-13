#!/usr/bin/env python3
"""Projeto Hi: o Klipper da Creality grava 2 linhas INFO para CADA requisição (webhooks _process_request),
inclusive as consultas de estado (objects/query) feitas a cada segundo por Moonraker/telas -> ~250 MB/dia de log
no eMMC. Aqui essas duas linhas deixam de ser gravadas para consultas de leitura (objects/query, objects/list, info).
Comandos (gcode/script etc.) continuam no log. Uso: webhooks_menos_log.py <klippy/webhooks.py>"""
import sys
p = sys.argv[1]; s = open(p).read()
if "HI_LEITURA" in s: print("já aplicado", p); sys.exit(0)
a = "            logging.info(f'webhooks: method:{web_request.get_method()},received {web_request.params},id:{web_request.id}')\n"
b = "            logging.info(f'webhooks: method:{web_request.get_method()},finished {web_request.params},id:{web_request.id}')\n"
assert a in s and b in s, "formato inesperado"
cond = "            if web_request.get_method() not in HI_LEITURA:\n    "
s = s.replace(a, cond + a, 1).replace(b, cond + b, 1)
s = s.replace("    def _process_request(self, web_request):\n",
    "    # Ajuste Projeto Hi: consultas de leitura não vão para o log (eram ~250 MB/dia)\n"
    "    def _process_request(self, web_request):\n", 1)
import re
m = re.search(r"(?m)^import logging[^\n]*\n", s); assert m, "sem import logging"
s = s[:m.end()] + "HI_LEITURA = ('objects/query', 'objects/list', 'info')\n" + s[m.end():]
open(p, "w").write(s); print("ok", p)
