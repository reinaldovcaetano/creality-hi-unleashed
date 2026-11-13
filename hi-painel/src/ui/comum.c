#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "tema.h"
#include "comum.h"

hi_estado_t hi_e;
static hi_atualizar_fn atual;

void hi_set_atualizar(hi_atualizar_fn f) { atual = f; if (f) f(&hi_e); }
void hi_atualizar_agora(void)
{
    hi_estado_copiar(&hi_e);
    if (atual) atual(&hi_e);
}

char *hi_br(char *b) { for (char *p = b; *p; p++) if (*p == '.') *p = ','; return b; }

void hi_fmt_tempo(double s, char *dst, int tam)
{
    long t = (long)(s + 0.5);
    if (t >= 3600) snprintf(dst, tam, "%ldh %02ldmin", t / 3600, (t % 3600) / 60);
    else snprintf(dst, tam, "%ldmin %02lds", t / 60, t % 60);
}

bool hi_ocupada(const hi_estado_t *e) { return !strcmp(e->impr_estado, "printing") || !strcmp(e->impr_estado, "paused"); }

const char *hi_estado_txt(const hi_estado_t *e, lv_color_t *cor)
{
    *cor = COR_OK;
    if (!e->conectado) { *cor = COR_ERRO; return "Sem conexão com o Klipper"; }
    if (!strcmp(e->klippy, "shutdown") || !strcmp(e->klippy, "error")) { *cor = COR_ERRO; return "Erro no Klipper"; }
    if (strcmp(e->klippy, "ready")) { *cor = COR_AVISO; return "Iniciando..."; }
    if (!strcmp(e->impr_estado, "printing")) { *cor = COR_DESTAQUE; return "Imprimindo"; }
    if (!strcmp(e->impr_estado, "paused")) { *cor = COR_AVISO; return "Pausada"; }
    if (!strcmp(e->impr_estado, "complete")) return "Impressão concluída";
    if (!strcmp(e->impr_estado, "cancelled")) { *cor = COR_AVISO; return "Impressão cancelada"; }
    if (!strcmp(e->impr_estado, "error")) { *cor = COR_ERRO; return "Erro na impressão"; }
    if (e->bico_alvo > 0 || e->mesa_alvo > 0) { *cor = COR_BICO; return "Aquecendo"; }
    return "Pronta";
}

/* ---------------- aviso rápido (toast) ---------------- */
static void apagar(lv_timer_t *t) { lv_obj_del((lv_obj_t *)t->user_data); lv_timer_del(t); }
void hi_toast(const char *txt, lv_color_t cor)
{
    lv_obj_t *b = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(b);
    lv_obj_set_style_bg_color(b, COR_CARTAO2, 0); lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 10, 0); lv_obj_set_style_border_color(b, cor, 0); lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_pad_all(b, 8, 0);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_t *l = hi_rotulo(b, txt, &hi_font_14, COR_TEXTO);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); lv_obj_set_width(l, LV_MIN(280, LV_SIZE_CONTENT));
    lv_obj_set_style_max_width(l, 280, 0);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_timer_t *t = lv_timer_create(apagar, 2600, b); lv_timer_set_repeat_count(t, 1);
}

/* ---------------- fundo modal ---------------- */
static lv_obj_t *modal(void)
{
    lv_obj_t *f = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(f);
    lv_obj_set_size(f, HI_W, HI_H);
    lv_obj_set_style_bg_color(f, lv_color_black(), 0); lv_obj_set_style_bg_opa(f, LV_OPA_70, 0);
    lv_obj_add_flag(f, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(f, LV_OPA_TRANSP, 0);                 /* aparece suave */
    lv_obj_fade_in(f, 100, 0);
    hi_bloquear_toque(300);
    return f;
}

/* ---------------- confirmação ---------------- */
typedef struct { hi_ok_cb cb; void *ud; lv_obj_t *f; } conf_t;
static void conf_ok(lv_event_t *e) { hi_bloquear_toque(250); conf_t *c = lv_event_get_user_data(e); hi_ok_cb cb = c->cb; void *ud = c->ud; lv_obj_del_async(c->f); free(c); if (cb) cb(ud); }
static void conf_nao(lv_event_t *e) { hi_bloquear_toque(250); conf_t *c = lv_event_get_user_data(e); lv_obj_del_async(c->f); free(c); }

void hi_confirmar(const char *titulo, const char *texto, const char *botao_ok, lv_color_t cor_ok, hi_ok_cb cb, void *ud)
{
    conf_t *c = calloc(1, sizeof *c); c->cb = cb; c->ud = ud;
    c->f = modal();
    lv_obj_t *k = hi_cartao(c->f, 20, 30, HI_W - 40, HI_H - 60);
    lv_obj_t *t = hi_rotulo(k, titulo, &hi_font_16, COR_TEXTO); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t *x = hi_rotulo(k, texto, &hi_font_14, COR_TEXTO2);
    lv_label_set_long_mode(x, LV_LABEL_LONG_WRAP); lv_obj_set_width(x, HI_W - 70);
    lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(x, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_t *n = hi_botao(k, "Voltar", COR_CARTAO2, 110, 44); lv_obj_align(n, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(n, conf_nao, LV_EVENT_CLICKED, c);
    lv_obj_t *o = hi_botao(k, botao_ok, cor_ok, 130, 44); lv_obj_align(o, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(o, conf_ok, LV_EVENT_CLICKED, c);
}

/* ---------------- menu de opções ---------------- */
typedef struct { hi_menu_cb cb; void *ud; lv_obj_t *f; } menu_t;
static void menu_ev(lv_event_t *e)
{
    hi_bloquear_toque(250);
    lv_obj_t *b = lv_event_get_current_target(e); menu_t *m = lv_obj_get_user_data(b);
    int op = (int)(intptr_t)lv_event_get_user_data(e);
    hi_menu_cb cb = m->cb; void *ud = m->ud; lv_obj_del_async(m->f); free(m);
    if (cb) cb(op, ud);
}
void hi_menu(const char *titulo, const char *texto, const char *opcoes[], const lv_color_t cores[], int n, hi_menu_cb cb, void *ud)
{
    menu_t *m = calloc(1, sizeof *m); m->cb = cb; m->ud = ud; m->f = modal();
    int alt = 56 + (texto ? 22 : 0) + n * 40;
    lv_obj_t *k = hi_cartao(m->f, 30, (HI_H - alt) / 2, HI_W - 60, alt);
    lv_obj_t *t = hi_rotulo(k, titulo, &hi_font_16, COR_TEXTO); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT); lv_obj_set_width(t, HI_W - 80); lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    int y = 34;
    if (texto) { lv_obj_t *x = hi_rotulo(k, texto, &hi_font_12, COR_TEXTO2); lv_obj_align(x, LV_ALIGN_TOP_MID, 0, y); y += 22; }
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = hi_botao(k, opcoes[i], cores[i].full ? cores[i] : COR_CARTAO2, HI_W - 84, 36);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y + i * 40);
        lv_obj_set_user_data(b, m);
        lv_obj_add_event_cb(b, menu_ev, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/* ---------------- teclado numérico ---------------- */
typedef struct { hi_num_cb cb; void *ud; lv_obj_t *f, *visor; char txt[24]; double min, max; int dec; } num_t;
static const char *mapa_num[] = {"7", "8", "9", LV_SYMBOL_BACKSPACE, "\n", "4", "5", "6", "-", "\n", "1", "2", "3", ",", "\n",
                                 "0", "Cancelar", LV_SYMBOL_OK, ""};
static void num_tecla(lv_event_t *e)
{
    num_t *n = lv_event_get_user_data(e);
    lv_obj_t *m = lv_event_get_target(e);
    const char *k = lv_btnmatrix_get_btn_text(m, lv_btnmatrix_get_selected_btn(m));
    if (!k) return;
    size_t l = strlen(n->txt);
    if (!strcmp(k, LV_SYMBOL_BACKSPACE)) { if (l) n->txt[l - 1] = 0; }
    else if (!strcmp(k, "Cancelar")) { hi_bloquear_toque(250); lv_obj_del_async(n->f); free(n); return; }
    else if (!strcmp(k, LV_SYMBOL_OK)) {
        char t[24]; snprintf(t, sizeof t, "%s", n->txt); for (char *p = t; *p; p++) if (*p == ',') *p = '.';
        double v = atof(t);
        if (!n->txt[0] || v < n->min || v > n->max) {
            char a[80], b1[16], b2[16];
            snprintf(b1, sizeof b1, "%.*f", n->dec, n->min); snprintf(b2, sizeof b2, "%.*f", n->dec, n->max);
            snprintf(a, sizeof a, "Valor fora da faixa (%s a %s)", hi_br(b1), hi_br(b2));
            hi_toast(a, COR_AVISO); return;
        }
        hi_bloquear_toque(250); hi_num_cb cb = n->cb; void *ud = n->ud; lv_obj_del_async(n->f); free(n); if (cb) cb(v, ud); return;
    }
    else if (!strcmp(k, "-")) { if (l == 0) strcpy(n->txt, "-"); }
    else if (!strcmp(k, ",")) { if (n->dec > 0 && !strchr(n->txt, ',') && l < 20) strcat(n->txt, ","); }
    else if (l < 10) strcat(n->txt, k);
    lv_label_set_text(n->visor, n->txt[0] ? n->txt : " ");
}

void hi_teclado_num(const char *titulo, double inicial, double min, double max, int decimais, hi_num_cb cb, void *ud)
{
    num_t *n = calloc(1, sizeof *n); n->cb = cb; n->ud = ud; n->min = min; n->max = max; n->dec = decimais;
    snprintf(n->txt, sizeof n->txt, "%.*f", decimais, inicial); hi_br(n->txt);
    n->f = modal();
    lv_obj_t *k = hi_cartao(n->f, 6, 4, HI_W - 12, HI_H - 8);
    lv_obj_t *t = hi_rotulo(k, titulo, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(t, 10, 6);
    n->visor = hi_rotulo(k, n->txt, &hi_font_20, COR_TEXTO); lv_obj_align(n->visor, LV_ALIGN_TOP_RIGHT, -14, 4);
    lv_obj_t *m = lv_btnmatrix_create(k);
    lv_btnmatrix_set_map(m, mapa_num);
    lv_btnmatrix_set_btn_width(m, 13, 2);   /* "Cancelar" mais largo */
    lv_obj_set_size(m, HI_W - 20, HI_H - 46); lv_obj_set_pos(m, 2, 32);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(m, 0, 0); lv_obj_set_style_pad_all(m, 2, 0);
    lv_obj_set_style_bg_color(m, COR_CARTAO2, LV_PART_ITEMS); lv_obj_set_style_text_color(m, COR_TEXTO, LV_PART_ITEMS);
    lv_obj_set_style_text_font(m, &hi_font_20, LV_PART_ITEMS); lv_obj_set_style_radius(m, 8, LV_PART_ITEMS);
    lv_obj_set_style_border_width(m, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(m, COR_DESTAQUE, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(m, num_tecla, LV_EVENT_VALUE_CHANGED, n);
}

/* ---------------- teclado de texto ---------------- */
typedef struct { hi_txt_cb cb; void *ud; lv_obj_t *f, *ta; } txt_t;
static void txt_ev(lv_event_t *e)
{
    txt_t *t = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        char v[128]; snprintf(v, sizeof v, "%s", lv_textarea_get_text(t->ta));
        hi_txt_cb cb = t->cb; void *ud = t->ud; lv_obj_del_async(t->f); free(t); if (cb) cb(v, ud);
    } else if (lv_event_get_code(e) == LV_EVENT_CANCEL) { lv_obj_del_async(t->f); free(t); }
}
static void txt_voltar(lv_event_t *e) { hi_bloquear_toque(250); txt_t *t = lv_event_get_user_data(e); lv_obj_del_async(t->f); free(t); }
void hi_teclado_texto(const char *titulo, const char *inicial, hi_txt_cb cb, void *ud)
{
    txt_t *t = calloc(1, sizeof *t); t->cb = cb; t->ud = ud;
    t->f = modal();
    lv_obj_set_style_bg_color(t->f, COR_FUNDO, 0); lv_obj_set_style_bg_opa(t->f, LV_OPA_COVER, 0);
    lv_obj_t *r = hi_rotulo(t->f, titulo, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(r, 8, 4);
    lv_label_set_long_mode(r, LV_LABEL_LONG_DOT); lv_obj_set_width(r, HI_W - 100);
    lv_obj_t *vt = hi_botao(t->f, LV_SYMBOL_LEFT " Voltar", COR_CARTAO2, 84, 24); lv_obj_set_pos(vt, HI_W - 90, 0);
    lv_obj_add_event_cb(vt, txt_voltar, LV_EVENT_CLICKED, t);
    t->ta = lv_textarea_create(t->f);
    lv_textarea_set_one_line(t->ta, true); lv_textarea_set_text(t->ta, inicial ? inicial : "");
    lv_obj_set_size(t->ta, HI_W - 16, 36); lv_obj_set_pos(t->ta, 8, 24);
    lv_obj_set_style_bg_color(t->ta, COR_CARTAO, 0); lv_obj_set_style_text_color(t->ta, COR_TEXTO, 0);
    lv_obj_set_style_border_color(t->ta, COR_DESTAQUE, 0); lv_obj_set_style_text_font(t->ta, &hi_font_16, 0);
    lv_obj_t *kb = lv_keyboard_create(t->f);
    lv_obj_set_size(kb, HI_W, HI_H - 66); lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, t->ta);
    lv_obj_set_style_bg_color(kb, COR_FUNDO, 0);
    lv_obj_set_style_bg_color(kb, COR_CARTAO2, LV_PART_ITEMS); lv_obj_set_style_text_color(kb, COR_TEXTO, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &hi_font_14, LV_PART_ITEMS); lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, txt_ev, LV_EVENT_READY, t);
    lv_obj_add_event_cb(kb, txt_ev, LV_EVENT_CANCEL, t);
}
