/* Hi Painel: lembretes de manutenção (por horas de impressão) e retomada após queda de energia. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"
#include "../dados/arquivos.h"

/* ================= manutenção ================= */
typedef struct { const char *id, *nome, *como; int horas; } item_man_t;
static const item_man_t ITENS[] = {
    {"man_mesa",     "Limpar a mesa",                 "Lave a placa PEI com água e detergente; seque bem.", 30},
    {"man_bico",     "Limpar bico e ventoinhas",      "Tire resíduos do bico, da ventoinha da peça e do dissipador.", 100},
    {"man_eixos",    "Limpar e lubrificar os eixos",  "Limpe as guias e passe lubrificante leve nas hastes.", 150},
    {"man_correias", "Verificar correias e polias",   "Confira a tensão das correias e os parafusos das polias.", 250},
    {"man_trocar",   "Trocar o bico",                 "Bico gasto deixa a extrusão irregular. Troque com o bico quente.", 600},
};
#define N_ITENS (int)(sizeof ITENS / sizeof ITENS[0])

static double horas_total(void)
{
    double h = hi_horas_impressas();
    return (h < 0 ? 0 : h) + hi_pref("man_horas_base", 0);   /* base = horas impressas antes do histórico */
}
int hi_manutencao_pendentes(void)
{
    static double h_cache = -1; static int n_cache; static double quando;
    if (h_cache < 0 || hi_agora() - quando > 300) {
        h_cache = horas_total(); quando = hi_agora(); n_cache = 0;
        for (int i = 0; i < N_ITENS; i++) if (h_cache - hi_pref(ITENS[i].id, 0) >= ITENS[i].horas) n_cache++;
    }
    return n_cache;
}

static int item_sel;
static void feito(void *u)
{
    (void)u; double h = horas_total(); hi_pref_set(ITENS[item_sel].id, (int)h);
    char r[96]; snprintf(r, sizeof r, "%s (com %.0f h)", ITENS[item_sel].nome, h); hi_calib_registrar("Manutenção", r);
    hi_toast("Registrado. O próximo lembrete conta a partir de agora.", COR_OK); hi_trocar(tela_manutencao);
}
static void ev_item(lv_event_t *e)
{
    item_sel = (int)(intptr_t)lv_event_get_user_data(e);
    char m[220]; snprintf(m, sizeof m, "%s\n\nMarcar como feito agora?", ITENS[item_sel].como);
    hi_confirmar(ITENS[item_sel].nome, m, LV_SYMBOL_OK "  Feito", COR_OK, feito, NULL);
}
static void set_base(double v, void *u) { (void)u; hi_pref_set("man_horas_base", (int)v); hi_trocar(tela_manutencao); }
static void ev_base(lv_event_t *e) { (void)e; hi_teclado_num("Horas já impressas antes (estimativa)", hi_pref("man_horas_base", 0), 0, 50000, 0, set_base, NULL); }

lv_obj_t *tela_manutencao(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Manutenção", true);
    double h = horas_total();
    char b[96]; snprintf(b, sizeof b, "%.0f h " LV_SYMBOL_EDIT, h);
    lv_obj_t *hb = hi_botao(t, b, COR_CARTAO2, 96, 24); lv_obj_align(hb, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_add_event_cb(hb, ev_base, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = hi_lista(t, 32, HI_H - 32);
    for (int i = 0; i < N_ITENS; i++) {
        double desde = h - hi_pref(ITENS[i].id, 0), falta = ITENS[i].horas - desde;
        char d[96];
        if (falta <= 0) snprintf(d, sizeof d, "PENDENTE  \xE2\x80\xA2  a cada %d h de impressão", ITENS[i].horas);
        else snprintf(d, sizeof d, "faltam %.0f h  \xE2\x80\xA2  a cada %d h", falta, ITENS[i].horas);
        lv_obj_t *it = hi_item_grande(l, falta <= 0 ? LV_SYMBOL_WARNING : LV_SYMBOL_OK, falta <= 0 ? COR_AVISO : COR_OK, ITENS[i].nome, d);
        lv_obj_t *bar = lv_bar_create(it); lv_obj_set_size(bar, 200, 4); lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 56, -5);
        lv_bar_set_range(bar, 0, ITENS[i].horas); lv_bar_set_value(bar, (int)(desde > ITENS[i].horas ? ITENS[i].horas : desde < 0 ? 0 : desde), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, COR_CARTAO2, 0); lv_obj_set_style_bg_color(bar, falta <= 0 ? COR_AVISO : COR_DESTAQUE, LV_PART_INDICATOR);
        lv_obj_add_event_cb(it, ev_item, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    return t;
}

/* ================= queda de energia ================= */
char hi_ql_arquivo[200];        /* preenchido por main_fb.c ao detectar */
static void continuar(void *u)
{
    (void)u;
    const char *rel = hi_ql_arquivo;
    const char *pre = "/mnt/UDISK/printer_data/gcodes/";
    if (!strncmp(rel, pre, strlen(pre))) rel += strlen(pre);
    hi_gcode("SDCARD_PRINT_FILE FILENAME=\"%s\" ISCONTINUEPRINT=True", rel);    /* mesmo comando do master-server */
    hi_toast("Retomando a impressão...", COR_OK);
    hi_definir_raiz(tela_inicio); hi_ir(tela_imprimindo);
}
static void descartar(void *u)
{
    (void)u; char r[512];
    if (hi_klippy("pause_resume/cancel_continue_print", r, sizeof r) == 0) hi_toast("Impressão interrompida descartada", COR_AVISO);
    else hi_toast("Não consegui falar com o Klipper", COR_ERRO);
    hi_ir_raiz(tela_inicio);
}
static void ev_cont(lv_event_t *e) { (void)e; hi_confirmar("Continuar a impressão?", "A Hi aquece, volta ao ponto onde parou e segue imprimindo. Confira se a peça ainda está presa na mesa.", LV_SYMBOL_PLAY "  Continuar", COR_OK, continuar, NULL); }
static void ev_desc(lv_event_t *e) { (void)e; hi_confirmar("Descartar?", "A impressão interrompida não poderá ser retomada.", "Descartar", COR_ERRO, descartar, NULL); }
static void ev_creality(lv_event_t *e) { (void)e; if (hi_sair_cb) hi_sair_cb(); }

lv_obj_t *tela_queda_energia(void)
{
    lv_obj_t *t = hi_tela_nova();
    lv_obj_t *c = hi_cartao(t, 10, 8, 300, 224);
    lv_obj_set_style_border_color(c, COR_AVISO, 0);
    lv_obj_t *i = hi_rotulo(c, LV_SYMBOL_WARNING, &hi_font_28, COR_AVISO); lv_obj_align(i, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t *a = hi_rotulo(c, "Impressão interrompida", &hi_font_16, COR_TEXTO); lv_obj_align(a, LV_ALIGN_TOP_MID, 0, 44);
    const char *nome = strrchr(hi_ql_arquivo, '/'); nome = nome ? nome + 1 : hi_ql_arquivo;
    char b[260]; snprintf(b, sizeof b, "A energia caiu durante:\n%s", nome[0] ? nome : "(arquivo desconhecido)");
    lv_obj_t *d = hi_rotulo(c, b, &hi_font_12, COR_TEXTO2);
    lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP); lv_obj_set_width(d, 270);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 68);
    lv_obj_t *s = hi_botao(c, LV_SYMBOL_PLAY "  Continuar", COR_OK, 136, 44); lv_obj_align(s, LV_ALIGN_BOTTOM_RIGHT, -10, -38);
    lv_obj_add_event_cb(s, ev_cont, LV_EVENT_CLICKED, NULL);
    lv_obj_t *n = hi_botao(c, "Descartar", COR_CARTAO2, 136, 44); lv_obj_align(n, LV_ALIGN_BOTTOM_LEFT, 10, -38);
    lv_obj_add_event_cb(n, ev_desc, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cr = hi_rotulo(c, "decidir na tela Creality", &hi_font_12, COR_DESTAQUE); lv_obj_align(cr, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(cr, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(cr, ev_creality, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(cr, 12);
    return t;
}
