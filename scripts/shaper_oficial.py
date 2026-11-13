#!/usr/bin/env python3
"""Projeto Hi: recalcula a vibração com o algoritmo OFICIAL do Klipper (shaper_calibrate.py) a partir do CSV que o
módulo da Creality (base 1.1.0.74) deixa em /tmp/shaper_calc_output_<eixo>-<data>.csv. Roda no computador.
Uso:
  mkdir t && cd t && mkdir extras && touch extras/__init__.py && echo 'base_dir="/nao"' > extras/base_info.py
  ssh -p 2222 root@HI 'cat /usr/share/klipper/klippy/extras/shaper_calibrate.py' > extras/shaper_calibrate.py
  ssh -p 2222 root@HI 'cat /usr/share/klipper/klippy/extras/shaper_defs.py'     > extras/shaper_defs.py
  ssh -p 2222 root@HI 'cat /tmp/shaper_calc_output_x-*.csv' > x.csv   (idem y)
  python3 -m venv venv && venv/bin/pip install numpy && venv/bin/python shaper_oficial.py
"""
import sys, csv, numpy as np
sys.path.insert(0, '.')
from extras import shaper_calibrate as sc
c = object.__new__(sc.ShaperCalibrate); c.numpy = np; c.printer = None
c.autotune_shapers = ['zv', 'mzv', 'ei', '2hump_ei', '3hump_ei']
for ax in 'xy':
    a = np.array([[float(v) for v in r if v.strip()] for r in list(csv.reader(open(ax + '.csv')))[1:]])
    cd = sc.CalibrationData(a[:, 0].copy(), a[:, 1].copy(), a[:, 2].copy(), a[:, 3].copy(), a[:, 4].copy()); cd.set_numpy(np)
    best, todos = c.find_best_shaper(cd, None, logger=lambda m: None)
    print("== eixo", ax.upper())
    for s in todos:
        print(f"  {s.name:9s} {s.freq:5.1f} Hz  vibração {s.vibrs*100:5.1f}%  suavização {s.smoothing:.3f}  aceleração segura {round(s.max_accel/100)*100:5.0f}")
    print(f"  -> Klipper oficial recomenda: {best.name} {best.freq:.1f} Hz")
