#include "tema.h"
#include "comum.h"

void (*hi_sair_cb)(void) = NULL;

/* ---------- navegação (pilha simples de telas) ---------- */
static hi_tela_fn pilha[16];
static int topo = -1;

/* trava de toque: depois de trocar de tela ou abrir um diálogo, ignora toques por um instante
 * e exige soltar o dedo (o toque que abriu não "vaza" para a tela nova). Lida em main_fb.c. */
uint32_t hi_bloqueio_ate;
void hi_bloquear_toque(uint32_t ms)
{
    hi_bloqueio_ate = lv_tick_get() + ms;
    lv_indev_t *in = lv_indev_get_act();
    if (in) lv_indev_wait_release(in);
}

static void carregar_anim(hi_tela_fn f, lv_scr_load_anim_t anim)
{
    hi_set_atualizar(NULL);   /* a tela nova registra a sua */
    lv_obj_t *nova = f();
    lv_scr_load_anim(nova, anim, anim == LV_SCR_LOAD_ANIM_NONE ? 0 : 140, 0, true);   /* true: apaga a anterior ao fim */
    hi_bloquear_toque(260);
}
static void carregar(hi_tela_fn f) { carregar_anim(f, LV_SCR_LOAD_ANIM_FADE_IN); }

void hi_ir(hi_tela_fn f)
{
    if (topo < 15) pilha[++topo] = f;
    carregar_anim(f, topo == 0 ? LV_SCR_LOAD_ANIM_NONE : LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

void hi_ir_raiz(hi_tela_fn f) { topo = 0; pilha[0] = f; carregar(f); }
void hi_definir_raiz(hi_tela_fn f) { topo = 0; pilha[0] = f; }   /* só a pilha, sem carregar */
void hi_trocar(hi_tela_fn f) { if (topo >= 0) pilha[topo] = f; carregar(f); }

void hi_voltar(void)
{
    if (topo > 0) { topo--; carregar_anim(pilha[topo], LV_SCR_LOAD_ANIM_MOVE_RIGHT); }
}

static void ev_ir(lv_event_t *e) { hi_ir((hi_tela_fn)lv_event_get_user_data(e)); }
static void ev_voltar(lv_event_t *e) { (void)e; hi_voltar(); }

void hi_ao_clicar(lv_obj_t *o, hi_tela_fn destino)
{
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, ev_ir, LV_EVENT_CLICKED, (void *)destino);
}

/* ---------- componentes ---------- */
static lv_obj_t *caixa(lv_obj_t *pai)
{
    lv_obj_t *o = lv_obj_create(pai);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t *hi_tela_nova(void)
{
    lv_obj_t *t = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(t, COR_FUNDO, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(t, COR_TEXTO, 0);
    lv_obj_set_style_text_font(t, &hi_font_14, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    return t;
}

lv_obj_t *hi_rotulo(lv_obj_t *pai, const char *txt, const lv_font_t *f, lv_color_t cor)
{
    lv_obj_t *l = lv_label_create(pai);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, cor, 0);
    return l;
}

lv_obj_t *hi_cabecalho(lv_obj_t *tela, const char *titulo, bool voltar)
{
    lv_obj_t *b = caixa(tela);
    lv_obj_set_size(b, HI_W, 32);
    lv_obj_set_style_bg_color(b, COR_CARTAO, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(b, COR_BORDA, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    if (voltar) {
        lv_obj_t *z = caixa(b);                      /* área de toque grande para voltar */
        lv_obj_set_size(z, 120, 32);
        lv_obj_add_flag(z, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(z, ev_voltar, LV_EVENT_CLICKED, NULL);
        lv_obj_t *s = hi_rotulo(b, LV_SYMBOL_LEFT, &hi_font_16, COR_DESTAQUE);
        lv_obj_align(s, LV_ALIGN_LEFT_MID, 10, 0);
    }
    lv_obj_t *l = hi_rotulo(b, titulo, &hi_font_16, COR_TEXTO);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, voltar ? 30 : 10, 0);
    return b;
}

lv_obj_t *hi_cartao(lv_obj_t *pai, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *c = caixa(pai);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, COR_CARTAO, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_border_color(c, COR_BORDA, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    return c;
}

lv_obj_t *hi_botao(lv_obj_t *pai, const char *txt, lv_color_t cor, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *b = caixa(pai);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, cor, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, lv_color_lighten(cor, 40), LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = hi_rotulo(b, txt, &hi_font_14, lv_color_white());
    lv_obj_center(l);
    return b;
}

lv_obj_t *hi_lista(lv_obj_t *tela, lv_coord_t y, lv_coord_t h)
{
    lv_obj_t *l = lv_obj_create(tela);
    lv_obj_remove_style_all(l);
    lv_obj_set_pos(l, 0, y);
    lv_obj_set_size(l, HI_W, h);
    lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(l, 6, 0);
    lv_obj_set_style_pad_row(l, 5, 0);
    lv_obj_set_scroll_dir(l, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(l, LV_SCROLLBAR_MODE_ACTIVE);
    return l;
}

lv_obj_t *hi_item(lv_obj_t *lista, const char *icone, lv_color_t cor_icone, const char *titulo, const char *detalhe)
{
    lv_obj_t *c = caixa(lista);
    lv_obj_set_size(c, HI_W - 12, 40);
    lv_obj_set_style_bg_color(c, COR_CARTAO, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_border_color(c, COR_BORDA, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *ic = caixa(c);
    lv_obj_set_size(ic, 28, 28);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_radius(ic, 8, 0);
    lv_obj_set_style_bg_color(ic, cor_icone, 0);
    lv_obj_set_style_bg_opa(ic, LV_OPA_30, 0);
    lv_obj_t *s = hi_rotulo(ic, icone, &hi_font_14, cor_icone);
    lv_obj_center(s);
    lv_obj_t *t = hi_rotulo(c, titulo, &hi_font_14, COR_TEXTO);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT); lv_obj_set_size(t, HI_W - 12 - 42 - 26, lv_font_get_line_height(&hi_font_14));
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 42, detalhe ? 3 : 11);
    if (detalhe) {
        lv_obj_t *d = hi_rotulo(c, detalhe, &hi_font_12, COR_TEXTO2);
        lv_label_set_long_mode(d, LV_LABEL_LONG_DOT); lv_obj_set_size(d, HI_W - 12 - 42 - 26, lv_font_get_line_height(&hi_font_12));
        lv_obj_align(d, LV_ALIGN_TOP_LEFT, 42, 21);
    }
    lv_obj_t *seta = hi_rotulo(c, LV_SYMBOL_RIGHT, &hi_font_12, COR_TEXTO2);
    lv_obj_align(seta, LV_ALIGN_RIGHT_MID, -10, 0);
    return c;
}

lv_obj_t *hi_campo_num(lv_obj_t *pai, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *nome, const char *valor, const char *dica)
{
    lv_obj_t *c = hi_cartao(pai, x, y, w, dica ? 70 : 56);
    lv_obj_t *n = hi_rotulo(c, nome, &hi_font_12, COR_TEXTO2);
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 10, 5);
    lv_obj_t *menos = hi_botao(c, LV_SYMBOL_MINUS, COR_CARTAO2, 40, 32);
    lv_obj_align(menos, LV_ALIGN_TOP_LEFT, 8, 22);
    lv_obj_t *mais = hi_botao(c, LV_SYMBOL_PLUS, COR_CARTAO2, 40, 32);
    lv_obj_align(mais, LV_ALIGN_TOP_RIGHT, -8, 22);
    lv_obj_t *v = hi_rotulo(c, valor, &hi_font_20, COR_TEXTO);
    lv_obj_align(v, LV_ALIGN_TOP_MID, 0, 26);
    if (dica) {
        lv_obj_t *d = hi_rotulo(c, dica, &hi_font_12, COR_TEXTO2);
        lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 0, -3);
    }
    return c;
}

/* item de lista maior ("pílula"), para listas de ações como as calibrações */
lv_obj_t *hi_item_grande(lv_obj_t *lista, const char *icone, lv_color_t cor_icone, const char *titulo, const char *detalhe)
{
    lv_obj_t *c = hi_item(lista, icone, cor_icone, titulo, detalhe);
    lv_obj_set_height(c, 54);
    lv_obj_set_style_radius(c, 27, 0);
    lv_obj_t *ic = lv_obj_get_child(c, 0);
    lv_obj_set_size(ic, 40, 40); lv_obj_align(ic, LV_ALIGN_LEFT_MID, 7, 0); lv_obj_set_style_radius(ic, 20, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(ic, 0), &hi_font_16, 0);
    lv_obj_t *t = lv_obj_get_child(c, 1);
    lv_obj_set_style_text_font(t, &hi_font_16, 0); lv_obj_set_size(t, HI_W - 12 - 56 - 30, lv_font_get_line_height(&hi_font_16));
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 56, detalhe ? 7 : 16);
    if (detalhe) { lv_obj_set_width(lv_obj_get_child(c, 2), HI_W - 12 - 56 - 30); lv_obj_align(lv_obj_get_child(c, 2), LV_ALIGN_TOP_LEFT, 56, 29); }
    lv_obj_align(lv_obj_get_child(c, detalhe ? 3 : 2), LV_ALIGN_RIGHT_MID, -14, 0);
    return c;
}
