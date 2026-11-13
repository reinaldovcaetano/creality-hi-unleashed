/* Hi Painel: arquivos, detalhes, histórico, impressão em andamento e ajustes ao vivo */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"
#include "../dados/arquivos.h"
#include "../dados/miniatura.h"

#define GRAU "\xC2\xB0"
char hi_arquivo_sel[160];

static void urlcod(const char *s, char *o, size_t max)
{
    static const char *hex = "0123456789ABCDEF"; size_t j = 0;
    for (; *s && j + 4 < max; s++) {
        unsigned char c = *s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("-_.~/", c)) o[j++] = c;
        else { o[j++] = '%'; o[j++] = hex[c >> 4]; o[j++] = hex[c & 15]; }
    }
    o[j] = 0;
}
/* coloca a miniatura do arquivo dentro de uma caixa (substitui o ícone); devolve 1 se tinha miniatura */
static int por_miniatura(lv_obj_t *caixa, const char *nome, int grande, int lado)
{
    const lv_img_dsc_t *m = hi_miniatura(nome, grande);
    if (!m) return 0;
    lv_obj_clean(caixa);
    lv_obj_set_style_bg_opa(caixa, LV_OPA_TRANSP, 0);
    lv_obj_t *img = lv_img_create(caixa);
    lv_img_set_src(img, m);
    int maior = m->header.w > m->header.h ? m->header.w : m->header.h;
    lv_img_set_zoom(img, (uint16_t)(256 * lado / (maior ? maior : 1)));
    lv_img_set_antialias(img, true);
    lv_obj_center(img);
    return 1;
}
static const char *base(const char *p) { const char *b = strrchr(p, '/'); return b ? b + 1 : p; }
static void quando(double t, char *d, int n)
{
    double dif = difftime(time(NULL), (time_t)t);
    if (dif < 3600) snprintf(d, n, "agora há pouco"); else if (dif < 86400) snprintf(d, n, "há %.0fh", dif / 3600);
    else snprintf(d, n, "há %.0f dias", dif / 86400);
}

/* ================= lista de arquivos ================= */
static lv_obj_t *lista_arq, *l_carregando;
static unsigned versao_vista;
static hi_arquivo_t arqs[HI_MAX_ARQ];

static void ev_arq(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    snprintf(hi_arquivo_sel, sizeof hi_arquivo_sel, "%s", arqs[i].nome);
    hi_ir(tela_arquivo);
}

static void preencher(void)
{
    unsigned v; int n = hi_arquivos(arqs, HI_MAX_ARQ, &v);
    if (v == versao_vista) return;
    versao_vista = v;
    lv_obj_clean(lista_arq);
    hi_miniatura_limpar_cache();
    if (!n) { hi_rotulo(lista_arq, "Nenhum arquivo .gcode na impressora.\nEnvie pelo fatiador ou Fluidd.", &hi_font_14, COR_TEXTO2); return; }
    for (int i = 0; i < n; i++) {
        char d[96], q[32], tt[32];
        quando(arqs[i].modificado, q, sizeof q);
        if (arqs[i].tempo_estimado > 0) {
            hi_fmt_tempo(arqs[i].tempo_estimado, tt, sizeof tt);
            snprintf(d, sizeof d, "%s  \xE2\x80\xA2  %.1f m  \xE2\x80\xA2  %s", tt, arqs[i].filamento_mm / 1000, q); hi_br(d);
        } else snprintf(d, sizeof d, "%.1f MB  \xE2\x80\xA2  %s", arqs[i].tamanho / 1048576, q), hi_br(d);
        lv_obj_t *it = hi_item_grande(lista_arq, LV_SYMBOL_FILE, COR_OK, base(arqs[i].nome), d);
        lv_obj_set_style_radius(it, 12, 0);
        lv_obj_t *cx = lv_obj_get_child(it, 0); lv_obj_set_style_radius(cx, 8, 0);
        por_miniatura(cx, arqs[i].nome, 0, 40);
        lv_obj_add_event_cb(it, ev_arq, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}
static void atualizar_lista(const hi_estado_t *e) { (void)e; preencher(); }

lv_obj_t *tela_imprimir(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Imprimir", true);
    lv_obj_t *h = hi_botao(t, LV_SYMBOL_LIST "  Histórico", COR_CARTAO2, 108, 24); lv_obj_align(h, LV_ALIGN_TOP_RIGHT, -8, 4);
    hi_ao_clicar(h, tela_historico_impr);
    lista_arq = hi_lista(t, 32, HI_H - 32);
    l_carregando = hi_rotulo(lista_arq, "Carregando arquivos...", &hi_font_14, COR_TEXTO2);
    versao_vista = (unsigned)-1; hi_arquivos(arqs, 0, &versao_vista);   /* só mostra quando chegar a lista nova */
    hi_pedir_arquivos();
    hi_set_atualizar(atualizar_lista);
    return t;
}

/* ================= detalhes do arquivo ================= */
static void iniciar(void *ud)
{
    (void)ud;
    char c[400], q[512]; urlcod(hi_arquivo_sel, c, sizeof c);
    snprintf(q, sizeof q, "/printer/print/start?filename=%s", c);
    hi_req("POST", q);
    hi_toast("Iniciando impressão...", COR_OK);
    hi_definir_raiz(tela_inicio); hi_ir(tela_imprimindo);
}
static void ev_imprimir(lv_event_t *e)
{
    (void)e;
    if (hi_ocupada(&hi_e)) { hi_toast("Já existe uma impressão em andamento", COR_AVISO); return; }
    char m[400]; snprintf(m, sizeof m, "%s\n\nConfira se a mesa está limpa e o filamento carregado.", base(hi_arquivo_sel));
    /* malha medida em outra temperatura: avisa (não bloqueia) */
    int med = hi_pref("malha_temp", 0), mt = 0; int n = hi_arquivos(arqs, HI_MAX_ARQ, NULL);
    for (int i = 0; i < n; i++) if (!strcmp(arqs[i].nome, hi_arquivo_sel)) mt = (int)(arqs[i].mesa_temp + 0.5);
    if (med > 0 && mt > 0 && abs(mt - med) >= 10)
        snprintf(m + strlen(m), sizeof m - strlen(m), "\n\n" LV_SYMBOL_WARNING " A malha foi medida com a mesa a %d" GRAU " e este arquivo usa %d" GRAU ". Para a melhor 1a camada, refaça a malha a %d" GRAU ".", med, mt, mt);
    hi_confirmar("Imprimir?", m, LV_SYMBOL_PLAY "  Imprimir", COR_OK, iniciar, NULL);
}

lv_obj_t *tela_arquivo(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Arquivo", true);
    hi_arquivo_t a = {0}; int n = hi_arquivos(arqs, HI_MAX_ARQ, NULL);
    for (int i = 0; i < n; i++) if (!strcmp(arqs[i].nome, hi_arquivo_sel)) a = arqs[i];
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 140);
    lv_obj_t *mc = lv_obj_create(c); lv_obj_remove_style_all(mc); lv_obj_set_size(mc, 120, 120); lv_obj_set_pos(mc, 8, 9);
    lv_obj_set_style_bg_color(mc, COR_CARTAO2, 0); lv_obj_set_style_bg_opa(mc, LV_OPA_COVER, 0); lv_obj_set_style_radius(mc, 10, 0);
    lv_obj_clear_flag(mc, LV_OBJ_FLAG_SCROLLABLE);
    int tem_img = por_miniatura(mc, hi_arquivo_sel, 1, 116);
    if (!tem_img) { lv_obj_t *ic = hi_rotulo(mc, LV_SYMBOL_FILE, &hi_font_40, COR_TEXTO2); lv_obj_center(ic); }
    int xi = 136;
    lv_obj_t *nm = hi_rotulo(c, base(hi_arquivo_sel), &hi_font_14, COR_TEXTO);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_WRAP); lv_obj_set_width(nm, 304 - xi - 10); lv_obj_set_pos(nm, xi, 8);
    char b[200], tt[32], q[32];
    hi_fmt_tempo(a.tempo_estimado, tt, sizeof tt); quando(a.modificado, q, sizeof q);
    snprintf(b, sizeof b, LV_SYMBOL_LOOP "  %s\n" LV_SYMBOL_SHUFFLE "  %.2f m\n" LV_SYMBOL_LIST "  %d camadas\n" LV_SYMBOL_FILE "  %.1f MB\n      %s",
             a.tempo_estimado > 0 ? tt : "?", a.filamento_mm / 1000, a.camadas, a.tamanho / 1048576, q);
    hi_br(b);
    lv_obj_t *d = hi_rotulo(c, b, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(d, xi, 46);
    lv_obj_set_style_text_line_space(d, 4, 0);
    lv_obj_t *p = hi_botao(t, LV_SYMBOL_PLAY "  Imprimir", COR_OK, 304, 44); lv_obj_set_pos(p, 8, 188);
    lv_obj_add_event_cb(p, ev_imprimir, LV_EVENT_CLICKED, NULL);
    return t;
}

/* ================= histórico ================= */
static hi_historico_t hist[HI_MAX_HIST];
static lv_obj_t *lista_hist; static unsigned v_hist_vista;
static void ev_hist(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    snprintf(hi_arquivo_sel, sizeof hi_arquivo_sel, "%s", hist[i].nome);
    hi_pedir_arquivos(); hi_ir(tela_arquivo);
}
static void atualizar_hist(const hi_estado_t *e)
{
    (void)e;
    unsigned v; int n = hi_historico(hist, HI_MAX_HIST, &v);
    if (v == v_hist_vista) return; v_hist_vista = v;
    lv_obj_clean(lista_hist);
    if (!n) { hi_rotulo(lista_hist, "Nenhuma impressão registrada.", &hi_font_14, COR_TEXTO2); return; }
    for (int i = 0; i < n; i++) {
        char d[96], tt[32], dt[24]; time_t ti = (time_t)hist[i].inicio;
        strftime(dt, sizeof dt, "%d/%m %H:%M", localtime(&ti)); hi_fmt_tempo(hist[i].duracao, tt, sizeof tt);
        int ok = !strcmp(hist[i].estado, "completed");
        snprintf(d, sizeof d, "%s  \xE2\x80\xA2  %s  \xE2\x80\xA2  %s", dt, tt, ok ? "concluída" : !strcmp(hist[i].estado, "cancelled") ? "cancelada" : hist[i].estado);
        lv_obj_t *it = hi_item(lista_hist, ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE, ok ? COR_OK : COR_AVISO, base(hist[i].nome), d);
        lv_obj_add_event_cb(it, ev_hist, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}
lv_obj_t *tela_historico_impr(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Histórico de impressões", true);
    lista_hist = hi_lista(t, 32, HI_H - 32);
    hi_rotulo(lista_hist, "Carregando...", &hi_font_14, COR_TEXTO2);
    v_hist_vista = (unsigned)-1; hi_historico(hist, 0, &v_hist_vista);
    hi_pedir_historico();
    hi_set_atualizar(atualizar_hist);
    return t;
}

/* ================= imprimindo ================= */
static lv_obj_t *l_nome, *l_pct, *barra, *l_tempo, *l_rest, *l_camada, *l_temps, *l_vel, *b_pausa, *l_pausa;
static void pausar(void *ud) { (void)ud; hi_req("POST", "/printer/print/pause"); hi_toast("Pausando...", COR_AVISO); }
static void cancelar(void *ud) { (void)ud; hi_req("POST", "/printer/print/cancel"); hi_toast("Cancelando a impressão...", COR_ERRO); }
static void ev_pausa(lv_event_t *e)
{
    (void)e;
    if (!strcmp(hi_e.impr_estado, "paused")) { hi_req("POST", "/printer/print/resume"); hi_toast("Retomando...", COR_OK); }
    else hi_confirmar("Pausar?", "A impressão pausa no ponto atual.", "Pausar", COR_AVISO, pausar, NULL);
}
static void ev_cancelar(lv_event_t *e) { (void)e; hi_confirmar("Cancelar impressão?", "Não dá para desfazer.", "Cancelar", COR_ERRO, cancelar, NULL); }

static void atualizar_impr(const hi_estado_t *e)
{
    char b[128], t1[32], t2[32];
    lv_label_set_text(l_nome, e->impr_arquivo[0] ? base(e->impr_arquivo) : "(nenhuma impressão)");
    snprintf(b, sizeof b, "%.0f%%", e->progresso * 100); lv_label_set_text(l_pct, b);
    lv_bar_set_value(barra, (int)(e->progresso * 1000), LV_ANIM_OFF);
    hi_fmt_tempo(e->impr_duracao, t1, sizeof t1); snprintf(b, sizeof b, LV_SYMBOL_LOOP " %s", t1); lv_label_set_text(l_tempo, b);
    if (e->progresso > 0.01) { hi_fmt_tempo(e->impr_duracao / e->progresso - e->impr_duracao, t2, sizeof t2); snprintf(b, sizeof b, "faltam %s", t2); }
    else snprintf(b, sizeof b, "calculando...");
    lv_label_set_text(l_rest, b);
    if (e->camadas) snprintf(b, sizeof b, "camada %d / %d", e->camada, e->camadas); else b[0] = 0;
    lv_label_set_text(l_camada, b);
    snprintf(b, sizeof b, "Bico %.0f/%.0f" GRAU "   Mesa %.0f/%.0f" GRAU, e->bico, e->bico_alvo, e->mesa, e->mesa_alvo);
    lv_label_set_text(l_temps, b);
    snprintf(b, sizeof b, "vel %.0f%%  fluxo %.0f%%  Z %+.2f", e->vel_fator * 100, e->fluxo_fator * 100, e->z_offset); hi_br(b);
    lv_label_set_text(l_vel, b);
    int pausada = !strcmp(e->impr_estado, "paused");
    lv_label_set_text(l_pausa, pausada ? LV_SYMBOL_PLAY "  Retomar" : LV_SYMBOL_PAUSE "  Pausar");
    lv_obj_set_style_bg_color(b_pausa, pausada ? COR_OK : COR_CARTAO2, 0);
    if (pausada && e->impr_msg[0]) { char m[200]; snprintf(m, sizeof m, LV_SYMBOL_WARNING " %s", e->impr_msg); lv_label_set_text(l_temps, m); }
    if (!hi_ocupada(e) && e->conectado) {
        lv_color_t c; lv_label_set_text(l_rest, hi_estado_txt(e, &c));
    }
}

lv_obj_t *tela_imprimindo(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Impressão", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 116);
    int xi = 10;
    hi_estado_t e0; hi_estado_copiar(&e0);
    if (e0.impr_arquivo[0]) {
        lv_obj_t *mc = lv_obj_create(c); lv_obj_remove_style_all(mc); lv_obj_set_size(mc, 56, 56); lv_obj_set_pos(mc, 8, 6);
        lv_obj_clear_flag(mc, LV_OBJ_FLAG_SCROLLABLE);
        if (por_miniatura(mc, e0.impr_arquivo, 0, 56)) xi = 72; else lv_obj_del(mc);
    }
    l_nome = hi_rotulo(c, "", &hi_font_14, COR_TEXTO); lv_label_set_long_mode(l_nome, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l_nome, 294 - xi); lv_obj_set_pos(l_nome, xi, 6);
    l_pct = hi_rotulo(c, "0%", &hi_font_28, COR_DESTAQUE); lv_obj_set_pos(l_pct, xi, 26);
    l_tempo = hi_rotulo(c, "", &hi_font_14, COR_TEXTO); lv_obj_align(l_tempo, LV_ALIGN_TOP_RIGHT, -10, 30);
    l_rest = hi_rotulo(c, "", &hi_font_12, COR_TEXTO2); lv_obj_align(l_rest, LV_ALIGN_TOP_RIGHT, -10, 50);
    barra = lv_bar_create(c); lv_obj_set_size(barra, 284, 10); lv_obj_set_pos(barra, 10, 68); lv_bar_set_range(barra, 0, 1000);
    lv_obj_set_style_bg_color(barra, COR_CARTAO2, 0); lv_obj_set_style_bg_color(barra, COR_DESTAQUE, LV_PART_INDICATOR);
    l_camada = hi_rotulo(c, "", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(l_camada, 10, 84);
    l_vel = hi_rotulo(c, "", &hi_font_12, COR_TEXTO2); lv_obj_align(l_vel, LV_ALIGN_TOP_RIGHT, -10, 84);
    l_temps = hi_rotulo(t, "", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(l_temps, 12, 160);
    b_pausa = hi_botao(t, "", COR_CARTAO2, 100, 44); lv_obj_set_pos(b_pausa, 8, 186);
    l_pausa = lv_obj_get_child(b_pausa, 0);
    lv_obj_add_event_cb(b_pausa, ev_pausa, LV_EVENT_CLICKED, NULL);
    lv_obj_t *aj = hi_botao(t, LV_SYMBOL_SETTINGS "  Ajustar", COR_DESTAQUE, 100, 44); lv_obj_set_pos(aj, 112, 186);
    hi_ao_clicar(aj, tela_ajustes_impr);
    lv_obj_t *cn = hi_botao(t, LV_SYMBOL_STOP "  Cancelar", COR_ERRO, 96, 44); lv_obj_set_pos(cn, 216, 186);
    lv_obj_add_event_cb(cn, ev_cancelar, LV_EVENT_CLICKED, NULL);
    hi_set_atualizar(atualizar_impr);
    return t;
}

/* ================= ajustes durante a impressão ================= */
static lv_obj_t *v_vel, *v_fluxo, *v_z, *v_bico, *v_mesa, *v_vent;
static void set_vel(double v, void *u) { (void)u; hi_gcode("M220 S%.0f", v); }
static void set_fluxo(double v, void *u) { (void)u; hi_gcode("M221 S%.0f", v); }
static void set_bico(double v, void *u) { (void)u; hi_gcode("M104 S%.0f", v); }
static void set_mesa(double v, void *u) { (void)u; hi_gcode("M140 S%.0f", v); }
static void ev_num(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k == 0) hi_teclado_num("Velocidade (%)", hi_e.vel_fator * 100, 10, 300, 0, set_vel, NULL);
    if (k == 1) hi_teclado_num("Fluxo (%)", hi_e.fluxo_fator * 100, 50, 150, 0, set_fluxo, NULL);
    if (k == 2) hi_teclado_num("Bico (" GRAU "C)", hi_e.bico_alvo, 0, 300, 0, set_bico, NULL);
    if (k == 3) hi_teclado_num("Mesa (" GRAU "C)", hi_e.mesa_alvo, 0, 110, 0, set_mesa, NULL);
    if (k == 4) hi_ventoinha_popup();
}
static void ev_z(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    double d = k == 0 ? -0.05 : k == 1 ? -0.01 : k == 2 ? 0.01 : 0.05;
    hi_gcode("SET_GCODE_OFFSET Z_ADJUST=%.3f MOVE=1", d);
}
static lv_obj_t *campo(lv_obj_t *t, int x, int y, const char *nome, int k)
{
    lv_obj_t *c = hi_cartao(t, x, y, 98, 46);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, ev_num, LV_EVENT_CLICKED, (void *)(intptr_t)k);
    lv_obj_t *n = hi_rotulo(c, nome, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 8, 3);
    lv_obj_t *v = hi_rotulo(c, "", &hi_font_16, COR_TEXTO); lv_obj_set_pos(v, 8, 20);
    return v;
}
static void atualizar_aj(const hi_estado_t *e)
{
    char b[32];
    snprintf(b, sizeof b, "%.0f%%", e->vel_fator * 100); lv_label_set_text(v_vel, b);
    snprintf(b, sizeof b, "%.0f%%", e->fluxo_fator * 100); lv_label_set_text(v_fluxo, b);
    snprintf(b, sizeof b, "%+.3f mm", e->z_offset); hi_br(b); lv_label_set_text(v_z, b);
    snprintf(b, sizeof b, "%.0f/%.0f" GRAU, e->bico, e->bico_alvo); lv_label_set_text(v_bico, b);
    snprintf(b, sizeof b, "%.0f/%.0f" GRAU, e->mesa, e->mesa_alvo); lv_label_set_text(v_mesa, b);
    snprintf(b, sizeof b, "%.0f%%", e->ventoinha * 100); lv_label_set_text(v_vent, b);
}
lv_obj_t *tela_ajustes_impr(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Ajustar ao vivo", true);
    v_vel = campo(t, 8, 38, "Velocidade", 0); v_fluxo = campo(t, 111, 38, "Fluxo", 1); v_vent = campo(t, 214, 38, "Ventoinha", 4);
    v_bico = campo(t, 8, 90, "Bico", 2); v_mesa = campo(t, 111, 90, "Mesa", 3);
    lv_obj_t *zc = hi_cartao(t, 214, 90, 98, 46);
    lv_obj_t *zn = hi_rotulo(zc, "Z fino", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(zn, 8, 3);
    v_z = hi_rotulo(zc, "", &hi_font_14, COR_TEXTO); lv_obj_set_pos(v_z, 8, 22);
    const char *zt[4] = {"-0,05", "-0,01", "+0,01", "+0,05"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = hi_botao(t, zt[i], i < 2 ? COR_CARTAO2 : COR_CARTAO2, 73, 40); lv_obj_set_pos(b, 8 + i * 77, 146);
        lv_obj_add_event_cb(b, ev_z, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_t *dica = hi_rotulo(t, "Z fino: negativo aproxima o bico da mesa.\nToque num valor para mudar.", &hi_font_12, COR_TEXTO2);
    lv_obj_set_pos(dica, 10, 194);
    hi_set_atualizar(atualizar_aj);
    return t;
}
