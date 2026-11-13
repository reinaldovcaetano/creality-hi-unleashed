#!/usr/bin/env python3
"""Projeto Hi: lê/gera a imagem Android boot v2 da Hi (kernel + DTB, sem ramdisk)."""
import struct, hashlib, sys
PAGE = 2048
def pad(n): return (n + PAGE - 1) // PAGE * PAGE

def ler(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'ANDROID!'
    ks, ka, rs, ra, ss, sa, ta, ps, hv = struct.unpack_from('<9I', d, 8)
    rds, = struct.unpack_from('<I', d, 1632); hs, = struct.unpack_from('<I', d, 1644); dtbs, = struct.unpack_from('<I', d, 1648)
    assert ps == PAGE and rs == 0 and ss == 0 and rds == 0 and hv == 2
    kern = d[PAGE:PAGE + ks]; off = PAGE + pad(ks); dtb = d[off:off + dtbs]
    return d[:PAGE], kern, dtb, d

def id_v2(kern, dtb):
    h = hashlib.sha1()
    for blob in (kern, b'', b'', b'', dtb):   # kernel, ramdisk, second, recovery_dtbo, dtb
        h.update(blob); h.update(struct.pack('<I', len(blob)))
    return h.digest()

def gerar(header, kern, dtb, total):
    hdr = bytearray(header)
    struct.pack_into('<I', hdr, 8, len(kern))
    struct.pack_into('<I', hdr, 1648, len(dtb))
    hdr[576:576 + 32] = id_v2(kern, dtb).ljust(32, b'\0')
    out = bytes(hdr) + kern + b'\0' * (pad(len(kern)) - len(kern)) + dtb + b'\0' * (pad(len(dtb)) - len(dtb))
    if total and len(out) < total: out += b'\0' * (total - len(out))
    return out

if __name__ == '__main__':
    if sys.argv[1] == 'conferir':
        header, kern, dtb, d = ler(sys.argv[2])
        print('kernel', len(kern), 'dtb', len(dtb), 'arquivo', len(d))
        print('id original  ', header[576:596].hex()); print('id calculado ', id_v2(kern, dtb).hex())
        print('regerado idêntico ao original:', gerar(header, kern, dtb, len(d)) == d)
    elif sys.argv[1] == 'trocar':   # trocar <boot original> <zImage novo> <saida>
        header, kern, dtb, d = ler(sys.argv[2]); novo = open(sys.argv[3], 'rb').read()
        out = gerar(header, novo, dtb, len(d))
        assert len(out) <= 16 * 1024 * 1024, 'maior que a partição boot (16 MB)'
        open(sys.argv[4], 'wb').write(out); print('gerado', sys.argv[4], len(out), 'bytes; kernel', len(novo), 'dtb', len(dtb))
    elif sys.argv[1] == 'extrair-dtb':   # extrair-dtb <boot> <saida.dtb>
        header, kern, dtb, d = ler(sys.argv[2]); open(sys.argv[3], 'wb').write(dtb); print('dtb', len(dtb), 'bytes ->', sys.argv[3])
