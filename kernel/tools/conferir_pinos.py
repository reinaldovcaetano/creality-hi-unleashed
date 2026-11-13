#!/usr/bin/env python3
"""Projeto Hi: confere cada grupo de pinos do device tree da Creality contra a tabela do pinctrl do kernel.
Uso: conferir_pinos.py <creality.dts> <pinctrl-sun8iw20.c>
Para cada nó com pins="..." e function="X", verifica se TODOS os pinos têm a função X na tabela.
(Achado de 13/11/2026: PG9 marcado "twi0" no código público; o certo é "twi1".)"""
import re, sys
dts, tab = open(sys.argv[1]).read(), open(sys.argv[2]).read()
# a tabela tem um bloco #if FPGA ... #else: usar a última definição de cada pino
funcs = {}
for m in re.finditer(r'SUNXI_PIN\(SUNXI_PINCTRL_PIN\((\w), (\d+)\),(.*?)\)\),', tab, re.S):
    pin = "P%s%s" % (m.group(1), m.group(2))
    funcs[pin] = set(re.findall(r'SUNXI_FUNCTION\(0x\w+, "([^"]+)"\)', m.group(3)))
    funcs[pin].add("io_disabled")   # 0xF existe em todo pino (o regex para antes dele)
erros = ok = 0
for m in re.finditer(r'(\S+) \{\s*pins = "([^;]*)";\s*function = "([^"]+)";', dts):
    no, pins, f = m.group(1), m.group(2).split('\\0'), m.group(3)
    for p in pins:
        if p not in funcs:
            print("PINO DESCONHECIDO %s em %s" % (p, no)); erros += 1
        elif f not in funcs[p]:
            print("ERRO: %s pede %s=%s; a tabela tem %s" % (no, p, f, sorted(funcs[p]))); erros += 1
        else:
            ok += 1
print("pinos conferidos: %d ok, %d com erro" % (ok, erros))
sys.exit(1 if erros else 0)
