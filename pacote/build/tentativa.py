#!/usr/bin/env python3
"""Projeto Hi: transforma o sw-description da Creality em instalação por "tentativa única".
Em vez de trocar boot_partition/root_partition (metade oficial), grava:
  bootcmd   = "run hi_tenta"
  hi_tenta  = "setenv bootcmd run setargs_nand boot_normal; saveenv; setenv boot_partition bootX; setenv root_partition rootfsX; run setargs_nand boot_normal"
Ou seja: no próximo boot o U-Boot já devolve o bootcmd normal e GRAVA o env (metade oficial = a antiga) e só então
inicia a metade nova uma vez. Se ela travar, desligar/ligar volta para a antiga. O /etc/init.d/hi-confirma
da metade nova torna ela oficial depois de conferir tela, touch e Klipper."""
import re, sys
p = sys.argv[1]; s = open(p).read()

def entrada(nome, valor):
    return ('                {\n                    name = "%s";\n                    value = "%s";\n                },\n' % (nome, valor))

for secao, alvo in (("now_A_next_B", "B"), ("now_B_next_A", "A")):
    i = s.index(secao + " = {")
    j = s.index("bootenv: (", i) + len("bootenv: (")
    k = s.index(");", j)
    bloco = s[j:k]
    itens = re.findall(r'\{\s*name = "([^"]+)";\s*value = "([^"]*)";\s*\}', bloco)
    nomes = [n for n, _ in itens]
    assert "boot_partition" in nomes and "root_partition" in nomes, (secao, nomes)
    novos = [(n, v) for n, v in itens if n not in ("boot_partition", "root_partition", "version")]
    # O U-Boot da Allwinner converte root_partition em nand_root/mmc_root na inicialização da placa
    # ("update part info", ANTES do bootcmd). Trocar root_partition dentro do bootcmd não basta:
    # é preciso definir o dispositivo direto. GPT da Hi: rootfsA = mmcblk0p6, rootfsB = mmcblk0p7.
    dev = "/dev/mmcblk0p%d" % (6 if alvo == "A" else 7)
    novos.append(("hi_tenta", "setenv bootcmd run setargs_nand boot_normal; saveenv; "
                              "setenv boot_partition boot%s; setenv root_partition rootfs%s; "
                              "setenv nand_root %s; setenv mmc_root %s; run setargs_nand boot_normal" % (alvo, alvo, dev, dev)))
    novos.append(("bootcmd", "run hi_tenta"))
    corpo = "\n" + "".join(entrada(n, v) for n, v in novos).rstrip(",\n") + "\n            "
    s = s[:j] + corpo + s[k:]
open(p, "w").write(s)
print("sw-description em modo tentativa única")
