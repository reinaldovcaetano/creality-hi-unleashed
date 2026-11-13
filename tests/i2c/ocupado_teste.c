/* Projeto Hi: força a EEPROM a ficar ocupada (grava o MESMO valor que já está lá) e lê logo em seguida,
 * sem esperar o ciclo de gravação. Com o driver sem nova tentativa isso dá NACK; com a correção, deve passar.
 * uso: ocupado_teste <bus> <addr_hex> <reg> <vezes> */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
int main(int c, char **v) {
    char dev[32]; snprintf(dev, sizeof dev, "/dev/i2c-%s", v[1]);
    int addr = strtol(v[2], 0, 16), reg = atoi(v[3]), n = atoi(v[4]);
    int fd = open(dev, O_RDWR); if (fd < 0 || ioctl(fd, I2C_SLAVE, addr) < 0) { perror("open"); return 2; }
    unsigned char r = reg, val, w[2];
    if (write(fd, &r, 1) != 1 || read(fd, &val, 1) != 1) { perror("leitura inicial"); return 2; }
    int falhas = 0;
    for (int i = 0; i < n; i++) {
        usleep(10000);
        w[0] = reg; w[1] = val;
        if (write(fd, w, 2) != 2) { printf("gravação falhou #%d: %s\n", i, strerror(errno)); falhas++; continue; }
        unsigned char d = 0;   /* sem esperar os ~5 ms do ciclo de gravação */
        if (write(fd, &r, 1) != 1 || read(fd, &d, 1) != 1) { falhas++; if (falhas < 4) printf("leitura logo após gravar falhou #%d: %s\n", i, strerror(errno)); continue; }
        if (d != val) { printf("valor diferente! 0x%02x != 0x%02x\n", d, val); falhas++; }
    }
    printf("%s 0x%02x reg %d valor 0x%02x: %d ciclos, falhas=%d\n", dev, addr, reg, val, n, falhas);
    return falhas ? 1 : 0;
}
