/* marca o canto (x<60, y<60) do fb0 em BRANCO com um quadrado preto dentro, resto cinza */
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
int main(void) {
    int fd = open("/dev/fb0", O_RDWR); struct fb_var_screeninfo vi; ioctl(fd, FBIOGET_VSCREENINFO, &vi);
    uint32_t *fb = mmap(0, 960 * 640, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    for (int p = 0; p < 2; p++) for (int y = 0; y < 320; y++) for (int x = 0; x < 240; x++) {
        uint32_t c = 0xFF404850;
        if (x < 60 && y < 60) c = (x > 20 && x < 40 && y > 20 && y < 40) ? 0xFF000000 : 0xFFFFFFFF;
        if (x >= 180 && y < 60) c = 0xFFEF4444;   /* vermelho: x alto, y baixo */
        fb[(p * 320 + y) * 240 + x] = c;
    }
    vi.yoffset = 0; ioctl(fd, FBIOPAN_DISPLAY, &vi); return 0;
}
