/* Hi Painel: programa da impressora. LVGL 8.3 desenhando direto no /dev/fb0 (girado) + touch Goodix (evdev).
 *
 * Tela lógica 320x240 (deitada). O painel é 240x320 em pé, montado girado:  fb_y = lx, fb_x = 239 - ly
 * (HI_INV_X / HI_INV_Y invertem). O touch manda coordenadas do painel em pé e recebe a mesma rotação.
 *
 * Modos:
 *   hi-painel            abre direto; "Creality" sai do programa.
 *   hi-painel --espera   fica em segundo plano: segurar 3 s no canto superior esquerdo abre o Hi Painel;
 *                        "Creality" volta para a tela original e o programa continua esperando.
 * Ao abrir: pausa o display-server (SIGSTOP), pega o touch só para si (EVIOCGRAB) e guarda a imagem do fb.
 * Ao sair: devolve a imagem, solta o touch e retoma o display-server (SIGCONT). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <glob.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/input.h>
#include "lvgl.h"
#include "ui/tema.h"
#include "ui/comum.h"
#include "ui/telas.h"
#include "dados/moonraker.h"
#include "dados/arquivos.h"

static int fbfd = -1, tfd = -1, dispfd = -1;
static uint32_t *fbmem, *guardado;
static size_t fbtam;
static struct fb_var_screeninfo vinfo, vinfo_orig;
static int stride, inv_x, inv_y, log_toque, modo_espera;
static volatile int sinal_sair;
static int ativo, pedir_saida, dormindo, brilho = 200;
static pid_t pid_creality;
static int modo_boot;              /* --boot: assume a tela ao ligar (com as proteções) */

/* ---------------- log da tela da Creality: detecta a página aberta ---------------- */
#define LOG_CREALITY "/mnt/UDISK/creality/userdata/log/display-server.log"
static FILE *logc; static long logc_pos; static char ult_pagina[64];
static int ver_log_creality(void)    /* 1 = o usuário abriu a Wiki (vira atalho para o Hi Painel) */
{
    if (!logc) { logc = fopen(LOG_CREALITY, "r"); if (!logc) return 0; fseek(logc, 0, SEEK_END); logc_pos = ftell(logc); return 0; }
    fseek(logc, 0, SEEK_END); long fim = ftell(logc);
    if (fim < logc_pos) logc_pos = 0;          /* log rotacionado */
    fseek(logc, logc_pos, SEEK_SET);
    char l[512]; int wiki = 0;
    while (fgets(l, sizeof l, logc)) {
        char *p = strstr(l, "_ui/ui_");
        if (p) { p += 4; char *q = strstr(p, ".c]"); if (q && q - p < 60) { memcpy(ult_pagina, p, q - p); ult_pagina[q - p] = 0; } }
        if (strstr(l, "ui_Screen_CustomerFaq")) wiki = 1;
    }
    logc_pos = ftell(logc); clearerr(logc);
    return wiki;
}
static int arquivo_queda(void)   /* lê o arquivo que o Klipper da Creality grava durante a impressão */
{
    FILE *f = fopen("/mnt/UDISK/creality/userdata/config/print_file_name.json", "r"); if (!f) return 0;
    char b[1024]; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    char *p = strstr(b, "\"file_path\""); if (!p) return 0;
    p = strchr(p + 11, '"'); if (!p) return 0; p++;
    char *q = strchr(p, '"'); if (!q || q - p >= (int)sizeof hi_ql_arquivo) return 0;
    memcpy(hi_ql_arquivo, p, q - p); hi_ql_arquivo[q - p] = 0;
    return hi_ql_arquivo[0] != 0;
}
static int tem_atualizacao_usb(void)
{
    glob_t g; int n = !glob("/mnt/exUDISK/CR4NU200360C20_ota_img_V*.img", 0, NULL, &g) ? (int)g.gl_pathc : 0;
    if (!n) globfree(&g); else globfree(&g);
    return n;
}

/* ---------------- brilho (driver disp2 da Allwinner) ---------------- */
#define DISP_LCD_SET_BRIGHTNESS 0x102
#define DISP_LCD_GET_BRIGHTNESS 0x103
void hi_brilho(int v)
{
    if (dispfd < 0) dispfd = open("/dev/disp", O_RDWR);
    if (dispfd < 0) return;
    unsigned long a[3] = {0, (unsigned long)v, 0};
    ioctl(dispfd, DISP_LCD_SET_BRIGHTNESS, a);
    if (v > 0) brilho = v;
}
int hi_brilho_atual(void) { return brilho; }

/* ---------------- display ---------------- */
static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *p)
{
    if (!ativo) { lv_disp_flush_ready(drv); return; }
    for (int ly = a->y1; ly <= a->y2; ly++) {
        int fx = inv_y ? ly : (HI_H - 1 - ly);
        for (int lx = a->x1; lx <= a->x2; lx++) {
            int fy = inv_x ? (HI_W - 1 - lx) : lx;
            fbmem[fy * stride + fx] = lv_color_to32(*p++) | 0xFF000000u;
        }
    }
    lv_disp_flush_ready(drv);
}

/* ---------------- touch ---------------- */
static int raw_x, raw_y, pressionado;
static void ler_eventos(void)
{
    struct input_event ev;
    while (read(tfd, &ev, sizeof ev) == sizeof ev) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) raw_x = ev.value;
            if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) raw_y = ev.value;
            if (ev.code == ABS_MT_TRACKING_ID) pressionado = ev.value >= 0;
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) pressionado = ev.value;
        else if (ev.type == EV_SYN && log_toque && pressionado) fprintf(stderr, "toque cru x=%d y=%d\n", raw_x, raw_y);
    }
}
static void logico(int *lx, int *ly)
{
    int x = raw_y, y = (HI_H - 1) - raw_x;
    if (inv_x) x = HI_W - 1 - x;
    if (inv_y) y = HI_H - 1 - y;
    *lx = x < 0 ? 0 : x >= HI_W ? HI_W - 1 : x;
    *ly = y < 0 ? 0 : y >= HI_H ? HI_H - 1 : y;
}
static int ignorar_ate_soltar;
static void ler_toque(lv_indev_drv_t *drv, lv_indev_data_t *d)
{
    (void)drv;
    ler_eventos();
    int lx, ly; logico(&lx, &ly);
    d->point.x = lx; d->point.y = ly;
    if (lv_tick_get() < hi_bloqueio_ate && pressionado) ignorar_ate_soltar = 1;   /* trava pós-navegação */
    if (ignorar_ate_soltar) { if (!pressionado && lv_tick_get() >= hi_bloqueio_ate) ignorar_ate_soltar = 0; d->state = LV_INDEV_STATE_RELEASED; return; }
    if (dormindo) {                       /* primeiro toque só acorda a tela */
        if (pressionado) { dormindo = 0; hi_brilho(brilho); ignorar_ate_soltar = 1; lv_disp_trig_activity(NULL); }
        d->state = LV_INDEV_STATE_RELEASED; return;
    }
    d->state = pressionado ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* ---------------- tela da Creality ---------------- */
static pid_t achar_pid(const char *nome)
{
    DIR *d = opendir("/proc"); struct dirent *e; pid_t r = 0;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char p[64], c[64] = ""; snprintf(p, sizeof p, "/proc/%s/comm", e->d_name);
        FILE *f = fopen(p, "r"); if (!f) continue;
        if (fgets(c, sizeof c, f)) c[strcspn(c, "\n")] = 0;
        fclose(f);
        if (!strcmp(c, nome)) { r = atoi(e->d_name); break; }
    }
    if (d) closedir(d);
    return r;
}

static hi_tela_fn tela_ao_abrir;
static void abrir(void)
{
    pid_creality = achar_pid("display-server");
    if (pid_creality) kill(pid_creality, SIGSTOP);
    usleep(50000);
    memcpy(guardado, fbmem, fbtam);
    ioctl(tfd, EVIOCGRAB, 1);
    vinfo.yoffset = 0; ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
    ativo = 1; dormindo = 0; ignorar_ate_soltar = 1;
    hi_brilho(brilho);
    if (tela_ao_abrir) { hi_definir_raiz(tela_inicio); hi_ir(tela_ao_abrir); tela_ao_abrir = NULL; }
    else hi_ir_raiz(tela_inicio);
    lv_obj_invalidate(lv_scr_act());
    lv_disp_trig_activity(NULL);
    fprintf(stderr, "hi-painel: aberto\n");
}
static void fechar(void)
{
    if (!ativo) return;
    ativo = 0;
    memcpy(fbmem, guardado, fbtam);
    ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo_orig);
    ioctl(tfd, EVIOCGRAB, 0);
    hi_brilho(brilho);
    if (pid_creality) kill(pid_creality, SIGCONT);
    fprintf(stderr, "hi-painel: voltou para a tela Creality\n");
}
static void ao_sair_cb(void) { pedir_saida = 1; }
static void ao_sinal(int s) { (void)s; sinal_sair = 1; }
static void ao_fim(void) { fechar(); }

static uint32_t agora_ms(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
static void abrir_creality_cb(void *u) { (void)u; pedir_saida = 1; }
static void tique_dados(lv_timer_t *t)
{
    (void)t;
    hi_atualizar_agora();
    static int usb_avisado;
    int usb = tem_atualizacao_usb();
    if (ativo && usb && !usb_avisado) {
        usb_avisado = 1; if (dormindo) { dormindo = 0; hi_brilho(brilho); }
        hi_confirmar("Atualização no pendrive", "Achei um arquivo de atualização no pendrive. A instalação é feita pela tela da Creality.", "Abrir Creality", COR_DESTAQUE, abrir_creality_cb, NULL);
    }
    if (!usb) usb_avisado = 0;
    /* avisos de fim de impressão, em qualquer tela */
    static char est_ant[16];
    if (ativo && est_ant[0] && strcmp(est_ant, hi_e.impr_estado)) {
        const char *msg = NULL; lv_color_t cor = COR_OK;
        if (!strcmp(hi_e.impr_estado, "complete")) msg = LV_SYMBOL_OK "  Impressão concluída!";
        else if (!strcmp(hi_e.impr_estado, "cancelled")) { msg = "Impressão cancelada"; cor = COR_AVISO; }
        else if (!strcmp(hi_e.impr_estado, "error")) { msg = LV_SYMBOL_WARNING "  Erro na impressão"; cor = COR_ERRO; }
        else if (!strcmp(hi_e.impr_estado, "paused")) { msg = LV_SYMBOL_PAUSE "  Impressão pausada"; cor = COR_AVISO; }
        if (msg) { if (dormindo) { dormindo = 0; hi_brilho(brilho); } lv_disp_trig_activity(NULL); hi_toast(msg, cor); }
    }
    snprintf(est_ant, sizeof est_ant, "%s", hi_e.impr_estado);
    int min = hi_pref("descanso_min", 5);
    if (ativo && !dormindo && min > 0 && lv_disp_get_inactive_time(NULL) > (uint32_t)min * 60000u && !hi_ocupada(&hi_e)) {
        dormindo = 1; hi_brilho(0);
    }
}

int main(int argc, char **argv)
{
    modo_boot = argc > 1 && !strcmp(argv[1], "--boot");
    modo_espera = modo_boot || (argc > 1 && !strcmp(argv[1], "--espera"));
    const char *v;
    inv_x = (v = getenv("HI_INV_X")) && *v == '1'; inv_y = (v = getenv("HI_INV_Y")) && *v == '1';
    log_toque = (v = getenv("HI_LOG_TOQUE")) && *v == '1';

    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) { perror("fb0"); return 1; }
    struct fb_fix_screeninfo finfo;
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo); ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    vinfo_orig = vinfo; stride = finfo.line_length / 4; fbtam = finfo.line_length * vinfo.yres_virtual;
    fbmem = mmap(0, fbtam, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmem == MAP_FAILED) { perror("mmap"); return 1; }
    guardado = malloc(fbtam);
    tfd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (tfd < 0) { perror("touch"); return 1; }
    brilho = hi_pref("brilho", 200);

    signal(SIGTERM, ao_sinal); signal(SIGINT, ao_sinal); signal(SIGHUP, ao_sinal); signal(SIGPIPE, SIG_IGN);
    atexit(ao_fim);

    lv_init();
    static lv_color_t buf1[HI_W * 60];
    static lv_disp_draw_buf_t db; lv_disp_draw_buf_init(&db, buf1, NULL, HI_W * 60);
    static lv_disp_drv_t dd; lv_disp_drv_init(&dd);
    dd.hor_res = HI_W; dd.ver_res = HI_H; dd.flush_cb = flush; dd.draw_buf = &db;
    lv_disp_drv_register(&dd);
    static lv_indev_drv_t id; lv_indev_drv_init(&id);
    id.type = LV_INDEV_TYPE_POINTER; id.read_cb = ler_toque;
    lv_indev_drv_register(&id);

    hi_dados_iniciar();
    hi_sair_cb = ao_sair_cb;
    lv_timer_create(tique_dados, 500, NULL);
    if (!modo_espera) abrir();
    fprintf(stderr, "hi-painel beta iniciado (%s)\n", modo_boot ? "boot: assume quando a Creality chegar na home" : modo_espera ? "espera: Wiki ou 3 s no canto superior esquerdo" : "direto");

    uint32_t ant = agora_ms(), inicio_toque = 0;
    while (!sinal_sair) {
        uint32_t t = agora_ms(); lv_tick_inc(t - ant); ant = t;
        hi_poll_lento = !ativo || dormindo;
        if (ativo) {
            lv_timer_handler();
            if (pedir_saida) { pedir_saida = 0; fechar(); if (!modo_espera) break; }
            usleep(5000);
        } else {
            /* espera: só observa o touch (sem pegar para si) procurando o gesto */
            ler_eventos();
            int lx, ly; logico(&lx, &ly);
            if (pressionado && lx < 60 && ly < 60) {
                if (!inicio_toque) inicio_toque = t;
                else if (t - inicio_toque > 3000) { inicio_toque = 0; abrir(); }
            } else inicio_toque = 0;
            static uint32_t ult; if (t - ult > 500) {
                ult = t; hi_atualizar_agora();
                if (ver_log_creality()) { fprintf(stderr, "hi-painel: Wiki tocada na tela Creality\n"); abrir(); continue; }
                /* boot: assume quando a Creality chegou na tela inicial, sem queda de energia nem atualização pendente */
                static uint32_t desde_home;
                if (modo_boot) {
                    /* Ajuste 13/11: com atualização no pendrive o painel ESPERA (não desiste): quando o pendrive sai
                     * e a Creality está na tela inicial, ele assume. Impressão em andamento: deixa a Creality. */
                    static int esperando_usb;
                    int na_home = !strcmp(ult_pagina, "ui_screen_HomeNormal") && hi_e.conectado && !strcmp(hi_e.klippy, "ready");
                    if (na_home && hi_e.power_loss && arquivo_queda()) {
                        modo_boot = 0; tela_ao_abrir = tela_queda_energia;
                        fprintf(stderr, "hi-painel: queda de energia pendente (%s): tela própria\n", hi_ql_arquivo); abrir(); continue;
                    }
                    if (hi_ocupada(&hi_e)) { modo_boot = 0; fprintf(stderr, "hi-painel: boot fica na Creality (impressão em andamento)\n"); }
                    else if (tem_atualizacao_usb()) {
                        if (!esperando_usb) fprintf(stderr, "hi-painel: atualização no pendrive: esperando o pendrive sair\n");
                        esperando_usb = 1; desde_home = 0;
                    }
                    else if (na_home) {
                        if (esperando_usb) { esperando_usb = 0; fprintf(stderr, "hi-painel: pendrive retirado\n"); }
                        if (!desde_home) desde_home = t; else if (t - desde_home > 8000) { modo_boot = 0; abrir(); continue; }
                    }
                    else desde_home = 0;
                    /* desiste só se a Creality nunca chegou na tela inicial (ex.: assistente de primeira configuração) */
                    if (modo_boot && !esperando_usb && t > 240000 && !desde_home) { modo_boot = 0; fprintf(stderr, "hi-painel: Creality não chegou na tela inicial em 4 min: fica a Creality\n"); }
                }
            }
            usleep(30000);
        }
    }
    fechar();
    return 0;
}
