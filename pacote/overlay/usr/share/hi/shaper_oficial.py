#!/usr/bin/env python3
# Projeto Hi: recalcula a vibração com o algoritmo OFICIAL do Klipper (extras/shaper_calibrate.py, que continua na Hi)
# sobre o CSV que o módulo da Creality (base 1.1.0.74) deixa em /tmp/shaper_calc_output_<eixo>-<data>.csv.
# A escolha da Creality prefere 2hump/3hump (seguros só até ~1800 mm/s² no Y); o oficial só troca por um filtro mais
# pesado se a nota for 20% melhor. Saída no formato padrão do Klipper (o Hi Painel já sabe ler).
# Uso: shaper_oficial.py <x|y> [mtime_minimo]
import sys, os, glob, csv
sys.path.insert(0, '/usr/share/klipper/klippy')
import numpy as np
from extras import shaper_calibrate as sc

ax = sys.argv[1].lower()
desde = float(sys.argv[2]) if len(sys.argv) > 2 else 0
arqs = sorted(glob.glob('/tmp/shaper_calc_output_%s-*.csv' % ax), key=os.path.getmtime)
if not arqs or os.path.getmtime(arqs[-1]) < desde:
    print("hi-shaper: sem CSV novo do eixo %s" % ax); sys.exit(2)
linhas = [[float(v) for v in r if v.strip()] for r in list(csv.reader(open(arqs[-1])))[1:]]
a = np.array([l for l in linhas if len(l) >= 5])
cd = sc.CalibrationData(a[:, 0].copy(), a[:, 1].copy(), a[:, 2].copy(), a[:, 3].copy(), a[:, 4].copy())
cd.set_numpy(np)
c = object.__new__(sc.ShaperCalibrate); c.numpy = np; c.printer = None
c.autotune_shapers = ['zv', 'mzv', 'ei', '2hump_ei', '3hump_ei']
print("Calculating the best input shaper parameters for %s axis" % ax)
best, todos = c.find_best_shaper(cd, None, logger=print)
print("Recommended shaper_type_%s = %s, shaper_freq_%s = %.1f" % (ax, best.name, ax, best.freq))
