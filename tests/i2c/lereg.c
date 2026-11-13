/* Projeto Hi: lê registradores (devmem simples). uso: lereg 0xENDERECO... */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
int main(int c, char **v) {
    int fd = open("/dev/mem", O_RDONLY | O_SYNC); if (fd < 0) { perror("/dev/mem"); return 1; }
    for (int i = 1; i < c; i++) {
        unsigned long a = strtoul(v[i], 0, 16), pg = a & ~0xfffUL;
        volatile unsigned int *m = mmap(0, 4096, PROT_READ, MAP_SHARED, fd, pg);
        if (m == MAP_FAILED) { perror("mmap"); return 1; }
        printf("0x%08lx = 0x%08x\n", a, m[(a - pg) / 4]); munmap((void *)m, 4096);
    }
}
