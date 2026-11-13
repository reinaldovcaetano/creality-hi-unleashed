#!/usr/bin/env python3
"""Projeto Hi: driver AIC8800 (walnutpi/aic8800_sdio, RELEASE_DATE 2024_1024) no kernel 5.4 + cfg80211 do backports 6.1.
Só as verificações de versão que tratam de APIs do Wi-Fi (cfg80211/mac80211/ieee80211, vindas do backports 6.1)
passam a usar o caminho novo. As de APIs do kernel (escalonador, ioctl, debugfs, MAC) continuam no 5.4."""
import re, sys
A = sys.argv[1]  # pasta aic8800_fdrv

def ed(fn, pares):
    p = f"{A}/{fn}"; s = open(p).read()
    for a, b, n in pares:
        c = s.count(a)
        assert c == n, f"{fn}: esperava {n}x '{a}', achei {c}"
        s = s.replace(a, b)
    open(p, "w").write(s); print("ok", fn)

T = "1 /* Projeto Hi: API do Wi-Fi vem do backports 6.1 */"
# 1) HIGH_KERNEL_VERSION/2 = APIs do 6.0/6.1 -> ligadas; 3/4 = 6.3 -> desligadas
p = f"{A}/rwnx_compat.h"; s = open(p).read()
m = list(re.finditer(r"#define HIGH_KERNEL_VERSION4 KERNEL_VERSION\(6, 3, 0\)\n", s))[-1]
e = s.index("#endif", m.end()) + len("#endif")
s = s[:e] + """

/* Projeto Hi: kernel 5.4 + cfg80211/mac80211 do backports 6.1 */
#undef HIGH_KERNEL_VERSION
#undef HIGH_KERNEL_VERSION2
#undef HIGH_KERNEL_VERSION3
#undef HIGH_KERNEL_VERSION4
#define HIGH_KERNEL_VERSION KERNEL_VERSION(0, 0, 1)
#define HIGH_KERNEL_VERSION2 KERNEL_VERSION(0, 0, 1)
#define HIGH_KERNEL_VERSION3 KERNEL_VERSION(255, 0, 0)
#define HIGH_KERNEL_VERSION4 KERNEL_VERSION(255, 0, 0)
""" + s[e:]
s = s.replace("#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 60)", "#if " + T, 1)
open(p, "w").write(s); print("ok rwnx_compat.h")

ed("rwnx_main.c", [("#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))", "#if (" + T + ")", 2),
                   ("#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)", "#if " + T, 2)])
ed("rwnx_mod_params.c", [("#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)", "#if " + T, 4),
                         ("#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)", "#if " + T, 1)])
ed("rwnx_gki.c", [("#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)", "#if " + T, 1)])
ed("rwnx_gki.h", [("#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)", "#if " + T, 1)])
