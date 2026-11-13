/* Projeto Hi: repete a leitura que o klipper_mcu faz na EEPROM bl24c16f (write reg, 5 ms, read).
 * uso: eeprom_teste <bus> <addr_hex> <reg> <vezes> */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
int main(int c, char **v) {
    char dev[32]; snprintf(dev, sizeof dev, "/dev/i2c-%s", c > 1 ? v[1] : "1");
    int addr = c > 2 ? strtol(v[2], 0, 16) : 0x50, reg = c > 3 ? atoi(v[3]) : 1, n = c > 4 ? atoi(v[4]) : 200;
    int fd = open(dev, O_RDWR);
    if (fd < 0 || ioctl(fd, I2C_SLAVE, addr) < 0) { perror("open/slave"); return 2; }
    int ew = 0, er = 0; unsigned char r = reg, d = 0, first = 0;
    for (int i = 0; i < n; i++) {
        if (write(fd, &r, 1) != 1) { if (ew++ < 3) printf("write falhou #%d: %s\n", i, strerror(errno)); continue; }
        usleep(5000);
        if (read(fd, &d, 1) != 1) { if (er++ < 3) printf("read falhou #%d: %s\n", i, strerror(errno)); continue; }
        if (i == 0) first = d;
        usleep(2000);
    }
    printf("%s 0x%02x reg %d: %d ciclos, erros write=%d read=%d, valor=0x%02x\n", dev, addr, reg, n, ew, er, first);
    return (ew || er) ? 1 : 0;
}
