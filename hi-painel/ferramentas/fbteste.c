/* Hi Painel: teste do framebuffer. Mostra as informações do fb0 e desenha um padrão
 * (faixas coloridas + um círculo que deve parecer REDONDO se a compressão 5->4 estiver certa). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
int main(int c, char **v) {
    int fd = open("/dev/fb0", O_RDWR); if (fd < 0) { perror("fb0"); return 1; }
    struct fb_var_screeninfo vi; struct fb_fix_screeninfo fi;
    ioctl(fd, FBIOGET_VSCREENINFO, &vi); ioctl(fd, FBIOGET_FSCREENINFO, &fi);
    printf("xres %u yres %u virt %ux%u bpp %u stride %u yoff %u  R%u/%u G%u/%u B%u/%u A%u/%u\n", vi.xres, vi.yres, vi.xres_virtual, vi.yres_virtual,
           vi.bits_per_pixel, fi.line_length, vi.yoffset, vi.red.offset, vi.red.length, vi.green.offset, vi.green.length, vi.blue.offset, vi.blue.length, vi.transp.offset, vi.transp.length);
    if (c > 1 && v[1][0] == 'i') return 0;
    size_t tam = fi.line_length * vi.yres_virtual;
    uint32_t *fb = mmap(0, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); return 1; }
    int W = vi.xres, H = vi.yres, st = fi.line_length / 4;
    uint32_t cores[6] = {0xFFEF4444, 0xFFF59E0B, 0xFF22C55E, 0xFF3B82F6, 0xFF8B5CF6, 0xFFFFFFFF};
    for (int pg = 0; pg < (int)(vi.yres_virtual / H); pg++)
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            /* círculo de raio 80 "físico": no fb, y é comprimido 5->4 => elipse com eixo y = 80*0.8 */
            float dx = x - W / 2.0f, dy = (y - H / 2.0f) / 0.8f;
            uint32_t cor = cores[(y * 6) / H];
            if (dx * dx + dy * dy < 80 * 80) cor = 0xFF0E1116;
            if (x < 3 || y < 3 || x >= W - 3 || y >= H - 3) cor = 0xFFFFFFFF;
            fb[(pg * H + y) * st + x] = cor;
        }
    vi.yoffset = 0; ioctl(fd, FBIOPAN_DISPLAY, &vi);
    printf("padrão desenhado\n");
    return 0;
}
