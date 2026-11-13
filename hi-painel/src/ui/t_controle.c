/* Hi Painel: controle manual (mover eixos, home, temperaturas, ventoinha, luz, motores) */
#include <stdio.h>
#include <string.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"

#define GRAU "\xC2\xB0"
static const double passos[] = {0.1, 1, 10, 50};
static int passo_i = 2;
static lv_obj_t *l_pos, *b_passo[4], *v_bico, *v_mesa, *v_vent, *sw_luz;

static bool livre(void)
{
    if (hi_ocupada(&hi_e)) { hi_toast("Indisponível durante a impressão", COR_AVISO); return false; }
    if (strcmp(hi_e.klippy, "ready")) { hi_toast("Klipper não está pronto", COR_ERRO); return false; }
    return true;
}

static void ev_mover(lv_event_t *e)
{
    if (!livre()) return;
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    const char *eixo = k < 2 ? "X" : k < 4 ? "Y" : "Z";
    double d = passos[passo_i] * ((k % 2) ? -1 : 1);
    if (k >= 4 && passos[passo_i] > 10) d = (k % 2 ? -10 : 10);   /* Z no máximo 10 mm por toque */
    if (strchr(hi_e.homed, eixo[0] + 32) == NULL) { hi_toast("Faça o Home antes de mover", COR_AVISO); return; }
    hi_gcode("G91\nG1 %s%.2f F%d\nG90", eixo, d, k >= 4 ? 600 : 6000);
}
static void ev_home(lv_event_t *e) { (void)e; if (livre()) { hi_gcode("G28"); hi_toast("Fazendo Home de todos os eixos...", COR_DESTAQUE); } }
static void ev_motores(lv_event_t *e) { (void)e; if (livre()) { hi_gcode("M84"); hi_toast("Motores desligados", COR_OK); } }
static void ev_passo(lv_event_t *e)
{
    passo_i = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < 4; i++) lv_obj_set_style_bg_color(b_passo[i], i == passo_i ? COR_DESTAQUE : COR_CARTAO2, 0);
}
static void set_bico(double v, void *u) { (void)u; hi_gcode("M104 S%.0f", v); }
static void set_mesa(double v, void *u) { (void)u; hi_gcode("M140 S%.0f", v); }
static void set_vent(double v, void *u) { (void)u; hi_gcode("M106 S%.0f", v * 255 / 100); }
static void ev_num(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k == 0) hi_teclado_num("Bico (" GRAU "C, 0 desliga)", hi_e.bico_alvo, 0, 300, 0, set_bico, NULL);
    if (k == 1) hi_teclado_num("Mesa (" GRAU "C, 0 desliga)", hi_e.mesa_alvo, 0, 110, 0, set_mesa, NULL);
    if (k == 2) hi_teclado_num("Ventoinha da peça (%)", hi_e.ventoinha * 100, 0, 100, 0, set_vent, NULL);
}
static void ev_luz(lv_event_t *e)
{
    lv_obj_t *s = lv_event_get_target(e);
    hi_gcode("SET_PIN PIN=LED VALUE=%d", lv_obj_has_state(s, LV_STATE_CHECKED) ? 1 : 0);
}

static lv_obj_t *seta(lv_obj_t *t, int x, int y, const char *txt, int k)
{
    lv_obj_t *b = hi_botao(t, txt, COR_CARTAO2, 46, 40); lv_obj_set_pos(b, x, y);
    lv_obj_add_event_cb(b, ev_mover, LV_EVENT_CLICKED, (void *)(intptr_t)k);
    return b;
}
static lv_obj_t *campo(lv_obj_t *t, int y, const char *nome, int k)
{
    lv_obj_t *c = hi_cartao(t, 214, y, 98, 40);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, ev_num, LV_EVENT_CLICKED, (void *)(intptr_t)k);
    lv_obj_t *n = hi_rotulo(c, nome, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 8, 2);
    lv_obj_t *v = hi_rotulo(c, "", &hi_font_14, COR_TEXTO); lv_obj_set_pos(v, 8, 18);
    return v;
}
static void atualizar(const hi_estado_t *e)
{
    char b[96];
    snprintf(b, sizeof b, "X %.1f  Y %.1f  Z %.2f%s", e->pos[0], e->pos[1], e->pos[2], e->homed[0] ? "" : "  (sem home)"); hi_br(b);
    lv_label_set_text(l_pos, b);
    snprintf(b, sizeof b, "%.0f / %.0f" GRAU, e->bico, e->bico_alvo); lv_label_set_text(v_bico, b);
    snprintf(b, sizeof b, "%.0f / %.0f" GRAU, e->mesa, e->mesa_alvo); lv_label_set_text(v_mesa, b);
    snprintf(b, sizeof b, "%.0f%%", e->ventoinha * 100); lv_label_set_text(v_vent, b);
    if (!lv_obj_has_state(sw_luz, LV_STATE_PRESSED)) {
        if (e->led > 0.5) lv_obj_add_state(sw_luz, LV_STATE_CHECKED); else lv_obj_clear_state(sw_luz, LV_STATE_CHECKED);
    }
}

lv_obj_t *tela_controle(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Controle", true);
    l_pos = hi_rotulo(t, "", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(l_pos, 10, 36);
    /* cruz XY */
    seta(t, 58, 54, LV_SYMBOL_UP " Y", 2);
    seta(t, 8, 98, LV_SYMBOL_LEFT, 1);
    lv_obj_t *h = hi_botao(t, LV_SYMBOL_HOME, COR_DESTAQUE, 46, 40); lv_obj_set_pos(h, 58, 98);
    lv_obj_add_event_cb(h, ev_home, LV_EVENT_CLICKED, NULL);
    seta(t, 108, 98, LV_SYMBOL_RIGHT, 0);
    seta(t, 58, 142, LV_SYMBOL_DOWN " Y", 3);
    /* Z */
    seta(t, 160, 54, "Z+", 4);
    seta(t, 160, 142, "Z-", 5);
    lv_obj_t *mz = hi_rotulo(t, "Z", &hi_font_14, COR_TEXTO2); lv_obj_set_pos(mz, 178, 108);
    /* passo */
    const char *pt[4] = {"0,1mm", "1mm", "10mm", "50mm"};
    for (int i = 0; i < 4; i++) {
        b_passo[i] = hi_botao(t, pt[i], i == passo_i ? COR_DESTAQUE : COR_CARTAO2, 46, 30);
        lv_obj_set_pos(b_passo[i], 8 + i * 50, 194);
        lv_obj_add_event_cb(b_passo[i], ev_passo, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    /* lado direito */
    v_bico = campo(t, 36, "Bico", 0);
    v_mesa = campo(t, 80, "Mesa", 1);
    v_vent = campo(t, 124, "Ventoinha", 2);
    lv_obj_t *lc = hi_cartao(t, 214, 168, 98, 30);
    lv_obj_t *ll = hi_rotulo(lc, "Luz", &hi_font_12, COR_TEXTO2); lv_obj_align(ll, LV_ALIGN_LEFT_MID, 8, 0);
    sw_luz = lv_switch_create(lc); lv_obj_set_size(sw_luz, 40, 20); lv_obj_align(sw_luz, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(sw_luz, COR_DESTAQUE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_luz, ev_luz, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *mo = hi_botao(t, "Soltar motores", COR_CARTAO2, 98, 30); lv_obj_set_pos(mo, 214, 204);
    lv_obj_set_style_text_font(lv_obj_get_child(mo, 0), &hi_font_12, 0);
    lv_obj_add_event_cb(mo, ev_motores, LV_EVENT_CLICKED, NULL);
    hi_set_atualizar(atualizar);
    return t;
}
