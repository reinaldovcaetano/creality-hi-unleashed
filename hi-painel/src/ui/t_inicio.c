/* Hi Painel: tela inicial com dados ao vivo */
#include <stdio.h>
#include <string.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"

#define GRAU "\xC2\xB0"
static lv_obj_t *l_rede, *l_bico, *l_bico_a, *l_mesa, *l_mesa_a, *l_estado, *ponto, *l_info, *barra, *l_tile_impr;

static void ev_sair(lv_event_t *e) { (void)e; if (hi_sair_cb) hi_sair_cb(); }

static void aplicar_bico(double v, void *ud) { (void)ud; hi_gcode("M104 S%.0f", v); }
static void aplicar_mesa(double v, void *ud) { (void)ud; hi_gcode("M140 S%.0f", v); }
static void ev_bico(lv_event_t *e) { (void)e; hi_teclado_num("Temperatura do bico (0 desliga)", hi_e.bico_alvo, 0, 300, 0, aplicar_bico, NULL); }
static void ev_mesa(lv_event_t *e) { (void)e; hi_teclado_num("Temperatura da mesa (0 desliga)", hi_e.mesa_alvo, 0, 110, 0, aplicar_mesa, NULL); }

static void reiniciar_fw(void *ud) { (void)ud; hi_gcode("FIRMWARE_RESTART"); hi_reiniciando("FIRMWARE_RESTART", NULL); }
static void ev_estado(lv_event_t *e)
{
    (void)e;
    if (hi_ocupada(&hi_e)) { hi_ir(tela_imprimindo); return; }
    if (!strcmp(hi_e.klippy, "shutdown") || !strcmp(hi_e.klippy, "error")) {
        char m[300]; snprintf(m, sizeof m, "%s\n\nReiniciar o firmware?", hi_e.klippy_msg);
        hi_confirmar("Erro no Klipper", m, "Reiniciar", COR_AVISO, reiniciar_fw, NULL);
    }
}
static void ev_imprimir(lv_event_t *e) { (void)e; hi_ir(hi_ocupada(&hi_e) ? tela_imprimindo : tela_imprimir); }

static void cartao_temp(lv_obj_t *t, lv_coord_t x, const char *nome, lv_color_t cor, lv_obj_t **val, lv_obj_t **alvo, lv_event_cb_t cb)
{
    lv_obj_t *c = hi_cartao(t, x, 24, 148, 66);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *n = hi_rotulo(c, nome, &hi_font_12, COR_TEXTO2); lv_obj_align(n, LV_ALIGN_TOP_LEFT, 10, 6);
    lv_obj_t *ed = hi_rotulo(c, LV_SYMBOL_EDIT, &hi_font_12, COR_TEXTO2); lv_obj_align(ed, LV_ALIGN_TOP_RIGHT, -10, 6);
    *val = hi_rotulo(c, "--", &hi_font_28, cor); lv_obj_align(*val, LV_ALIGN_BOTTOM_LEFT, 10, -6);
    *alvo = hi_rotulo(c, "", &hi_font_12, COR_TEXTO2); lv_obj_align(*alvo, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
}

static lv_obj_t *tile(lv_obj_t *t, lv_coord_t x, lv_coord_t y, const char *ic, const char *txt, lv_color_t cor, hi_tela_fn destino)
{
    lv_obj_t *c = hi_cartao(t, x, y, 98, 52);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
    lv_obj_t *i = hi_rotulo(c, ic, &hi_font_16, cor); lv_obj_align(i, LV_ALIGN_TOP_LEFT, 9, 7);
    lv_obj_t *l = hi_rotulo(c, txt, &hi_font_14, COR_TEXTO); lv_obj_align(l, LV_ALIGN_BOTTOM_LEFT, 9, -6);
    if (destino) hi_ao_clicar(c, destino);
    return c;
}

static void atualizar(const hi_estado_t *e)
{
    char b[96];
    lv_label_set_text(l_rede, e->rede[0] ? e->rede : "");
    snprintf(b, sizeof b, "%.0f" GRAU, e->bico); lv_label_set_text(l_bico, b);
    if (e->bico_alvo > 0) snprintf(b, sizeof b, LV_SYMBOL_CHARGE " %.0f" GRAU, e->bico_alvo); else snprintf(b, sizeof b, "desligado");
    lv_label_set_text(l_bico_a, b);
    snprintf(b, sizeof b, "%.0f" GRAU, e->mesa); lv_label_set_text(l_mesa, b);
    if (e->mesa_alvo > 0) snprintf(b, sizeof b, LV_SYMBOL_CHARGE " %.0f" GRAU, e->mesa_alvo); else snprintf(b, sizeof b, "desligada");
    lv_label_set_text(l_mesa_a, b);
    lv_color_t cor; const char *s = hi_estado_txt(e, &cor);
    lv_label_set_text(l_estado, s);
    lv_obj_set_style_bg_color(ponto, cor, 0);
    if (hi_ocupada(e)) {
        snprintf(b, sizeof b, "%.0f%%", e->progresso * 100); lv_label_set_text(l_info, b);
        lv_obj_clear_flag(barra, LV_OBJ_FLAG_HIDDEN); lv_bar_set_value(barra, (int)(e->progresso * 1000), LV_ANIM_OFF);
        lv_label_set_text(l_tile_impr, "Imprimindo");
    } else {
        lv_obj_add_flag(barra, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(l_info, strcmp(e->klippy, "ready") && e->conectado ? "toque para detalhes" : "");
        lv_label_set_text(l_tile_impr, "Imprimir");
    }
}

lv_obj_t *tela_inicio(void)
{
    lv_obj_t *t = hi_tela_nova();
    lv_obj_t *nm = hi_rotulo(t, "Creality Hi", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(nm, 10, 4);
    l_rede = hi_rotulo(t, "", &hi_font_12, COR_OK); lv_obj_align(l_rede, LV_ALIGN_TOP_RIGHT, -10, 4);
    cartao_temp(t, 8, "Bico", COR_BICO, &l_bico, &l_bico_a, ev_bico);
    cartao_temp(t, 164, "Mesa", COR_MESA, &l_mesa, &l_mesa_a, ev_mesa);

    lv_obj_t *e = hi_cartao(t, 8, 96, 304, 30);
    lv_obj_add_flag(e, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(e, ev_estado, LV_EVENT_CLICKED, NULL);
    ponto = lv_obj_create(e); lv_obj_remove_style_all(ponto);
    lv_obj_set_size(ponto, 8, 8); lv_obj_set_style_radius(ponto, 4, 0); lv_obj_set_style_bg_opa(ponto, LV_OPA_COVER, 0);
    lv_obj_align(ponto, LV_ALIGN_LEFT_MID, 10, 0);
    l_estado = hi_rotulo(e, "", &hi_font_14, COR_TEXTO); lv_obj_align(l_estado, LV_ALIGN_LEFT_MID, 24, 0);
    barra = lv_bar_create(e); lv_obj_set_size(barra, 90, 8); lv_obj_align(barra, LV_ALIGN_RIGHT_MID, -50, 0);
    lv_bar_set_range(barra, 0, 1000);
    lv_obj_set_style_bg_color(barra, COR_CARTAO2, 0); lv_obj_set_style_bg_color(barra, COR_DESTAQUE, LV_PART_INDICATOR);
    l_info = hi_rotulo(e, "", &hi_font_12, COR_TEXTO2); lv_obj_align(l_info, LV_ALIGN_RIGHT_MID, -10, 0);

    lv_obj_t *ti = tile(t, 8, 132, LV_SYMBOL_PLAY, "Imprimir", COR_OK, NULL);
    lv_obj_add_event_cb(ti, ev_imprimir, LV_EVENT_CLICKED, NULL);
    l_tile_impr = lv_obj_get_child(ti, 1);
    tile(t, 111, 132, LV_SYMBOL_SETTINGS, "Calibrar", COR_DESTAQUE, tela_calibracoes);
    tile(t, 214, 132, LV_SYMBOL_SHUFFLE, "Filamento", COR_BICO, tela_filamento);
    tile(t, 8, 188, LV_SYMBOL_GPS, "Controle", COR_MESA, tela_controle);
    lv_obj_t *ta = tile(t, 111, 188, LV_SYMBOL_LIST, "Ajustes", COR_TEXTO2, tela_ajustes);
    if (hi_manutencao_pendentes()) {           /* selo de manutenção pendente */
        lv_obj_t *sl = hi_rotulo(ta, LV_SYMBOL_WARNING, &hi_font_14, COR_AVISO); lv_obj_align(sl, LV_ALIGN_TOP_RIGHT, -8, 7);
    }
    lv_obj_t *cr = tile(t, 214, 188, LV_SYMBOL_HOME, "Creality", COR_TEXTO2, NULL);
    lv_obj_add_event_cb(cr, ev_sair, LV_EVENT_CLICKED, NULL);
    hi_set_atualizar(atualizar);
    return t;
}
