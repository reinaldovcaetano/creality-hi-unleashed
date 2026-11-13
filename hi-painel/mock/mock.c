/* Hi Painel: renderiza telas sem hardware (320x240) com dados AO VIVO da impressora (HI_MOONRAKER=<ip>) e salva PNG. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl.h"
#include "../src/ui/tema.h"
#include "../src/ui/comum.h"
#include "../src/ui/telas.h"
#include "../src/dados/moonraker.h"

#define W 320
#define H 240
static lv_color_t fb[W * H], buf[W * H];
void hi_brilho(int v) { (void)v; }
int hi_brilho_atual(void) { return 200; }
static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *p)
{
    for (int y = a->y1; y <= a->y2; y++) for (int x = a->x1; x <= a->x2; x++) fb[y * W + x] = *p++;
    lv_disp_flush_ready(drv);
}
static uint32_t crc_t[256];
static void crc_ini(void) { for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1; crc_t[n] = c; } }
static uint32_t crc(uint32_t c, const uint8_t *b, size_t n) { c ^= 0xffffffffu; while (n--) c = crc_t[(c ^ *b++) & 0xff] ^ (c >> 8); return c ^ 0xffffffffu; }
static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *tp, const uint8_t *d, uint32_t n)
{
    uint8_t h[8]; be32(h, n); memcpy(h + 4, tp, 4); fwrite(h, 1, 8, f); if (n) fwrite(d, 1, n, f);
    uint8_t *tmp = malloc(n + 4); memcpy(tmp, tp, 4); if (n) memcpy(tmp + 4, d, n);
    uint8_t t[4]; be32(t, crc(0, tmp, n + 4)); fwrite(t, 1, 4, f); free(tmp);
}
static void salvar_png(const char *nome, const lv_color_t *img, int w, int h)
{
    size_t linha = 1 + w * 3, raw_n = linha * h; uint8_t *raw = malloc(raw_n);
    for (int y = 0; y < h; y++) { raw[y * linha] = 0; for (int x = 0; x < w; x++) { lv_color_t c = img[y * w + x];
        raw[y * linha + 1 + x * 3] = c.ch.red; raw[y * linha + 2 + x * 3] = c.ch.green; raw[y * linha + 3 + x * 3] = c.ch.blue; } }
    size_t blocos = (raw_n + 65534) / 65535; uint8_t *z = malloc(2 + raw_n + blocos * 5 + 4), *p = z;
    *p++ = 0x78; *p++ = 0x01; uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t o = 0; o < raw_n; o += 65535) { uint16_t n = (uint16_t)(raw_n - o > 65535 ? 65535 : raw_n - o);
        *p++ = (o + n >= raw_n); *p++ = n & 0xff; *p++ = n >> 8; *p++ = ~n & 0xff; *p++ = (~n >> 8) & 0xff; memcpy(p, raw + o, n); p += n; }
    be32(p, (b << 16) | a); p += 4;
    FILE *f = fopen(nome, "wb"); const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10}; fwrite(sig, 1, 8, f);
    uint8_t ih[13]; be32(ih, w); be32(ih + 4, h); ih[8] = 8; ih[9] = 2; ih[10] = ih[11] = ih[12] = 0;
    chunk(f, "IHDR", ih, 13); chunk(f, "IDAT", z, (uint32_t)(p - z)); chunk(f, "IEND", NULL, 0); fclose(f); free(z); free(raw);
}
#define MC 3
#define MW (MC * W + (MC + 1) * 10)
static lv_color_t mont[MW * (7 * H + 80)];
static lv_obj_t *tela_reiniciando_demo(void) { lv_obj_t *t = tela_ajustes(); hi_reiniciando("gravando as calibrações (SAVE_CONFIG)", NULL); return t; }
static lv_obj_t *tela_vent_demo(void) { lv_obj_t *t = tela_controle(); hi_ventoinha_popup(); return t; }
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    crc_ini(); lv_init();
    static lv_disp_draw_buf_t db; lv_disp_draw_buf_init(&db, buf, NULL, W * H);
    static lv_disp_drv_t dd; lv_disp_drv_init(&dd); dd.hor_res = W; dd.ver_res = H; dd.flush_cb = flush; dd.draw_buf = &db; dd.full_refresh = 1;
    lv_disp_t *disp = lv_disp_drv_register(&dd);
    hi_dados_iniciar();
    hi_pedir_arquivos(); sleep(3); hi_atualizar_agora();
    snprintf(hi_ql_arquivo, sizeof hi_ql_arquivo, "/mnt/UDISK/printer_data/gcodes/Nozzle cover_PLA_3h28m.gcode");
    struct { const char *n; hi_tela_fn f; } telas[] = {
        {"01-inicio", tela_inicio}, {"02-calibracoes", tela_calibracoes}, {"03-pid", tela_pid}, {"04-vibracao", tela_shaper},
        {"05-malha", tela_malha}, {"06-imprimir", tela_imprimir}, {"07-controle", tela_controle}, {"08-filamento", tela_filamento},
        {"09-filamentos", tela_filamentos}, {"10-ajustes", tela_ajustes}, {"11-rede", tela_rede}, {"12-imprimindo", tela_imprimindo}, {"13-manutencao", tela_manutencao}, {"14-queda", tela_queda_energia}, {"15-velocidade", tela_velocidade}, {"16-calibrar-tudo", tela_calibrar_tudo}, {"17-ventoinha", tela_ventoinha}, {"18-ventoinha-popup", tela_vent_demo}, {"19-reiniciando", tela_reiniciando_demo}};
    int nt = sizeof telas / sizeof telas[0], linhas = (nt + MC - 1) / MC, MH = linhas * H + (linhas + 1) * 10;
    for (int i = 0; i < MW * MH; i++) mont[i] = lv_color_hex(0x30343B);
    char nome[256];
    for (int i = 0; i < nt; i++) {
        fprintf(stderr, "tela %s\n", telas[i].n);
        hi_set_atualizar(NULL);
        lv_obj_t *s = telas[i].f(); lv_scr_load(s);
        for (int k = 0; k < 3; k++) { hi_atualizar_agora(); lv_tick_inc(600); lv_timer_handler(); usleep(200000); }
        lv_refr_now(disp);
        snprintf(nome, sizeof nome, "%s/%s.png", dir, telas[i].n); salvar_png(nome, fb, W, H);
        int ox = 10 + (i % MC) * (W + 10), oy = 10 + (i / MC) * (H + 10);
        for (int y = 0; y < H; y++) memcpy(&mont[(oy + y) * MW + ox], &fb[y * W], W * sizeof(lv_color_t));
    }
    snprintf(nome, sizeof nome, "%s/00-todas.png", dir); salvar_png(nome, mont, MW, MH);
    printf("ok: %d telas\n", nt);
    return 0;
}
