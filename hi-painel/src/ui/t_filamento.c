/* Hi Painel: carregar / descarregar / trocar filamento e o banco de filamentos (novo, editar, usar) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"
#include "../dados/arquivos.h"

#define GRAU "\xC2\xB0"
int hi_fil_editando = -1;
static hi_fil_t fils[HI_MAX_FIL]; static int n_fils, ativo;
static int temp_op = 230;
static lv_obj_t *l_ativo, *l_temp, *l_status;
static int etapa;   /* 0 parado, 1 carregando, 2 descarregando */

static void carregar_banco(void) { n_fils = hi_fil_carregar(fils, HI_MAX_FIL, &ativo); }

static bool livre(void)
{
    if (hi_ocupada(&hi_e)) { hi_toast("Indisponível durante a impressão", COR_AVISO); return false; }
    if (strcmp(hi_e.klippy, "ready")) { hi_toast("Klipper não está pronto", COR_ERRO); return false; }
    return true;
}
/* Ajuste 13/11: carregar usa a MESMA macro da tela original (LOAD_MATERIAL: home se preciso, vai à posição de
 * descarte, aquece, purga, limpa o bico e volta). Antes usava só EXTRUDE_BEFORE_CLEAR, que extrudava onde o
 * cabeçote estivesse (o filamento caía na mesa). O pré-aquecimento usa a temperatura do filamento escolhido. */
static void fazer_carregar(void *u) { (void)u; etapa = 1; hi_gcode("M109 S%d\nLOAD_MATERIAL", temp_op); }
static void fazer_descarregar(void *u) { (void)u; etapa = 2; hi_gcode("M109 S%d\nQUIT_MATERIAL", temp_op); }
static void ev_carregar(lv_event_t *e)
{
    (void)e; if (!livre()) return;
    char m[200]; snprintf(m, sizeof m, "Vai à posição de descarte, aquece o bico a %d" GRAU "C, purga e limpa o bico (igual à tela original).\nColoque o filamento no extrusor.", temp_op);
    hi_confirmar("Carregar filamento", m, "Carregar", COR_OK, fazer_carregar, NULL);
}
static void ev_descarregar(lv_event_t *e)
{
    (void)e; if (!livre()) return;
    char m[200]; snprintf(m, sizeof m, "Aquece o bico a %d" GRAU "C, corta e recolhe o filamento.\nDepois puxe-o para fora.", temp_op);
    hi_confirmar("Descarregar filamento", m, "Descarregar", COR_AVISO, fazer_descarregar, NULL);
}
static void ev_extrudar(lv_event_t *e)
{
    int mm = (int)(intptr_t)lv_event_get_user_data(e);
    if (!livre()) return;
    if (hi_e.bico < 170) { hi_toast("Aqueça o bico (mínimo 170\xC2\xB0" "C) antes de mover o filamento", COR_AVISO); return; }
    hi_gcode("IF_NEED_HOME\nBOX_GO_TO_EXTRUDE_POS\nM83\nG1 E%d F%d\nM82", mm, mm > 0 ? 180 : 600);   /* na posição de descarte */
}
static void set_temp(double v, void *u) { (void)u; temp_op = (int)v; char b[32]; snprintf(b, sizeof b, "%d" GRAU "C", temp_op); lv_label_set_text(l_temp, b); }
static void ev_temp(lv_event_t *e) { (void)e; hi_teclado_num("Temperatura para carregar/descarregar", temp_op, 170, 300, 0, set_temp, NULL); }

static void atualizar(const hi_estado_t *e)
{
    char b[96];
    if (etapa && e->bico_alvo > 0 && e->bico < e->bico_alvo - 3) snprintf(b, sizeof b, LV_SYMBOL_CHARGE " Aquecendo: %.0f / %.0f" GRAU, e->bico, e->bico_alvo);
    else if (etapa && e->bico_alvo > 0) snprintf(b, sizeof b, LV_SYMBOL_REFRESH " %s...", etapa == 1 ? "Carregando" : "Descarregando");
    else if (etapa && e->bico_alvo == 0) { snprintf(b, sizeof b, LV_SYMBOL_OK " Pronto (bico esfriando: %.0f" GRAU ")", e->bico); }
    else snprintf(b, sizeof b, "Bico: %.0f" GRAU, e->bico);
    lv_label_set_text(l_status, b);
}

lv_obj_t *tela_filamento(void)
{
    carregar_banco();
    if (ativo >= 0) temp_op = fils[ativo].bico;
    etapa = 0;
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Filamento", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 200, 50);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
    hi_ao_clicar(c, tela_filamentos);
    lv_obj_t *n = hi_rotulo(c, "Filamento em uso  " LV_SYMBOL_RIGHT, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 10, 4);
    l_ativo = hi_rotulo(c, ativo >= 0 ? fils[ativo].nome : "nenhum", &hi_font_16, COR_TEXTO); lv_obj_set_pos(l_ativo, 10, 22);
    lv_obj_t *ct = hi_cartao(t, 214, 38, 98, 50);
    lv_obj_add_flag(ct, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(ct, COR_CARTAO2, LV_STATE_PRESSED);
    lv_obj_add_event_cb(ct, ev_temp, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tn = hi_rotulo(ct, "Temperatura", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(tn, 8, 4);
    char b[24]; snprintf(b, sizeof b, "%d" GRAU "C", temp_op);
    l_temp = hi_rotulo(ct, b, &hi_font_16, COR_BICO); lv_obj_set_pos(l_temp, 8, 22);
    lv_obj_t *ca = hi_botao(t, LV_SYMBOL_DOWNLOAD "  Carregar", COR_OK, 148, 56); lv_obj_set_pos(ca, 8, 96);
    lv_obj_add_event_cb(ca, ev_carregar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *de = hi_botao(t, LV_SYMBOL_UPLOAD "  Descarregar", COR_AVISO, 148, 56); lv_obj_set_pos(de, 164, 96);
    lv_obj_add_event_cb(de, ev_descarregar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *s = hi_cartao(t, 8, 160, 304, 34);
    l_status = hi_rotulo(s, "", &hi_font_14, COR_TEXTO); lv_obj_align(l_status, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *ex = hi_botao(t, LV_SYMBOL_DOWN " +10", COR_CARTAO2, 70, 34); lv_obj_set_pos(ex, 8, 200);
    lv_obj_add_event_cb(ex, ev_extrudar, LV_EVENT_CLICKED, (void *)10);
    lv_obj_t *rt = hi_botao(t, LV_SYMBOL_UP " -10", COR_CARTAO2, 70, 34); lv_obj_set_pos(rt, 82, 200);
    lv_obj_add_event_cb(rt, ev_extrudar, LV_EVENT_CLICKED, (void *)-10);
    lv_obj_t *bd = hi_botao(t, LV_SYMBOL_LIST "  Meus filamentos", COR_CARTAO2, 156, 34);
    lv_obj_set_pos(bd, 156, 200); hi_ao_clicar(bd, tela_filamentos);
    hi_set_atualizar(atualizar);
    return t;
}

/* ================= banco ================= */
static void ev_fil(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    hi_fil_editando = i;            /* índice na lista combinada; a tela de edição decide se é nosso ou da Creality */
    hi_ir(tela_filamento_editar);
}
static void ev_novo(lv_event_t *e) { (void)e; hi_fil_editando = -1; hi_ir(tela_filamento_editar); }

lv_obj_t *tela_filamentos(void)
{
    carregar_banco();
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Meus filamentos", true);
    lv_obj_t *nv = hi_botao(t, LV_SYMBOL_PLUS "  Novo", COR_DESTAQUE, 84, 24); lv_obj_align(nv, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_add_event_cb(nv, ev_novo, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = hi_lista(t, 32, HI_H - 32);
    for (int i = 0; i < n_fils; i++) {
        char d[96]; snprintf(d, sizeof d, "%s  %d/%d" GRAU "  PA %.3f  \xE2\x80\xA2  %s%s", fils[i].tipo, fils[i].bico, fils[i].mesa, fils[i].pa,
                             fils[i].origem, i == ativo ? "  \xE2\x80\xA2  em uso" : ""); hi_br(d);
        lv_obj_t *it = hi_item(l, i == ativo ? LV_SYMBOL_OK : LV_SYMBOL_SHUFFLE, i == ativo ? COR_OK : (!strcmp(fils[i].origem, "Hi") ? COR_BICO : COR_TEXTO2), fils[i].nome, d);
        lv_obj_add_event_cb(it, ev_fil, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    return t;
}

/* ================= novo / editar ================= */
static hi_fil_t ed; static int ed_nosso;   /* índice no nosso banco, -1 novo */
static lv_obj_t *v_nome, *v_tipo, *v_campo[8];
static void redesenhar(void)
{
    char b[8][24];
    snprintf(b[0], 24, "%d" GRAU, ed.bico); snprintf(b[1], 24, "%d" GRAU, ed.mesa); snprintf(b[2], 24, "%d-%d%%", ed.vent_min, ed.vent_max);
    snprintf(b[3], 24, "%.3f", ed.pa); snprintf(b[4], 24, "%.2f", ed.fluxo); snprintf(b[5], 24, "%.0f mm\xC2\xB3/s", ed.vol_max);
    snprintf(b[6], 24, "%.1f mm", ed.retracao); snprintf(b[7], 24, "%.1f mm", ed.zhop);
    for (int i = 0; i < 8; i++) lv_label_set_text(v_campo[i], hi_br(b[i]));
    lv_label_set_text(v_nome, ed.nome[0] ? ed.nome : "(toque para dar nome)");
    char t[24]; snprintf(t, sizeof t, "%s " LV_SYMBOL_DOWN, ed.tipo); lv_label_set_text(v_tipo, t);
}
static void set_campo(double v, void *u)
{
    switch ((int)(intptr_t)u) {
        case 0: ed.bico = (int)v; ed.bico_1a = ed.bico + 5; break; case 1: ed.mesa = (int)v; break;
        case 2: ed.vent_min = (int)v; if (ed.vent_max < ed.vent_min) ed.vent_max = ed.vent_min; break;
        case 8: ed.vent_max = (int)v; break;
        case 3: ed.pa = v; break; case 4: ed.fluxo = v; break; case 5: ed.vol_max = v; break;
        case 6: ed.retracao = v; break; case 7: ed.zhop = v; break;
    }
    if ((int)(intptr_t)u == 2) { hi_teclado_num("Ventoinha máxima (%)", ed.vent_max, ed.vent_min, 100, 0, set_campo, (void *)8); }
    redesenhar();
}
static void ev_campo(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    const char *tit[8] = {"Bico (" GRAU "C)", "Mesa (" GRAU "C)", "Ventoinha mínima (%)", "Pressure Advance", "Fluxo (ex.: 0,95)", "Vazão máx. (mm\xC2\xB3/s)", "Retração (mm)", "Z-hop (mm)"};
    double ini[8] = {ed.bico, ed.mesa, ed.vent_min, ed.pa, ed.fluxo, ed.vol_max, ed.retracao, ed.zhop};
    double mn[8] = {150, 0, 0, 0, 0.5, 1, 0, 0}, mx[8] = {320, 120, 100, 1, 1.5, 60, 10, 2};
    int dec[8] = {0, 0, 0, 3, 2, 0, 1, 1};
    hi_teclado_num(tit[k], ini[k], mn[k], mx[k], dec[k], set_campo, (void *)(intptr_t)k);
}
static void set_nome(const char *v, void *u) { (void)u; snprintf(ed.nome, sizeof ed.nome, "%s", v); redesenhar(); }
static void ev_nome(lv_event_t *e) { (void)e; hi_teclado_texto("Nome do filamento", ed.nome, set_nome, NULL); }
static void ev_tipo(lv_event_t *e)
{
    (void)e; int i = 0; while (HI_TIPOS[i] && strcmp(HI_TIPOS[i], ed.tipo)) i++;
    i = HI_TIPOS[i] && HI_TIPOS[i + 1] ? i + 1 : 0;
    char nome[48]; snprintf(nome, sizeof nome, "%s", ed.nome);
    hi_fil_padrao(HI_TIPOS[i], &ed); snprintf(ed.nome, sizeof ed.nome, "%s", nome);
    redesenhar(); hi_toast("Valores padrão do tipo carregados", COR_DESTAQUE);
}
static void ev_salvar(lv_event_t *e)
{
    (void)e;
    if (!ed.nome[0]) { hi_toast("Dê um nome ao filamento", COR_AVISO); return; }
    snprintf(ed.origem, sizeof ed.origem, "Hi");
    if (hi_fil_salvar(&ed, ed_nosso)) { hi_toast("Filamento salvo", COR_OK); hi_voltar(); } else hi_toast("Erro ao salvar", COR_ERRO);
}
static void usar(void *u)
{
    (void)u;
    if (ed_nosso < 0 && !hi_fil_salvar(&ed, -1)) { hi_toast("Erro ao salvar", COR_ERRO); return; }
    hi_fil_ativar(ed.nome);
    hi_gcode("SET_PRESSURE_ADVANCE ADVANCE=%.4f", ed.pa);
    hi_toast("Filamento em uso. PA aplicado.", COR_OK); hi_voltar();
}
static void ev_usar(lv_event_t *e)
{
    (void)e; char m[200];
    snprintf(m, sizeof m, "Marca \"%s\" como filamento em uso e aplica o PA %.3f agora.\nAs temperaturas viram o padrão das calibrações e do carregar.", ed.nome, ed.pa); hi_br(m);
    hi_confirmar("Usar este filamento?", m, "Usar", COR_OK, usar, NULL);
}
static void apagar(void *u) { (void)u; hi_fil_apagar(ed.nome); hi_toast("Filamento apagado", COR_AVISO); hi_voltar(); }
static void ev_apagar(lv_event_t *e) { (void)e; hi_confirmar("Apagar?", ed.nome, "Apagar", COR_ERRO, apagar, NULL); }

lv_obj_t *tela_filamento_editar(void)
{
    carregar_banco();
    ed_nosso = -1;
    if (hi_fil_editando >= 0 && hi_fil_editando < n_fils) {
        ed = fils[hi_fil_editando];
        if (!strcmp(ed.origem, "Hi")) ed_nosso = hi_fil_editando;   /* os nossos vêm primeiro na lista */
        else { char n[48]; snprintf(n, sizeof n, "%s (cópia)", ed.nome); snprintf(ed.nome, sizeof ed.nome, "%s", n); }
    } else { memset(&ed, 0, sizeof ed); hi_fil_padrao("PETG", &ed); }
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, hi_fil_editando < 0 ? "Novo filamento" : ed_nosso >= 0 ? "Editar filamento" : "Copiar da Creality", true);
    lv_obj_t *nm = hi_cartao(t, 8, 36, 200, 40); lv_obj_add_flag(nm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(nm, ev_nome, LV_EVENT_CLICKED, NULL);
    lv_obj_t *n1 = hi_rotulo(nm, "Nome  " LV_SYMBOL_EDIT, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n1, 8, 2);
    v_nome = hi_rotulo(nm, "", &hi_font_14, COR_TEXTO); lv_obj_set_pos(v_nome, 8, 18);
    lv_label_set_long_mode(v_nome, LV_LABEL_LONG_DOT); lv_obj_set_width(v_nome, 184);
    lv_obj_t *tp = hi_cartao(t, 214, 36, 98, 40); lv_obj_add_flag(tp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tp, ev_tipo, LV_EVENT_CLICKED, NULL);
    lv_obj_t *t1 = hi_rotulo(tp, "Tipo", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(t1, 8, 2);
    v_tipo = hi_rotulo(tp, "", &hi_font_14, COR_DESTAQUE); lv_obj_set_pos(v_tipo, 8, 18);
    const char *rot[8] = {"Bico", "Mesa", "Ventoinha", "PA", "Fluxo", "Vazão máx", "Retração", "Z-hop"};
    for (int i = 0; i < 8; i++) {
        lv_obj_t *c = hi_cartao(t, 8 + (i % 4) * 77, 82 + (i / 4) * 48, 73, 43);
        lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED); lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(c, ev_campo, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *a = hi_rotulo(c, rot[i], &hi_font_12, COR_TEXTO2); lv_obj_set_pos(a, 6, 2);
        v_campo[i] = hi_rotulo(c, "", &hi_font_14, COR_TEXTO); lv_obj_set_pos(v_campo[i], 6, 20);
    }
    int x = 8;
    if (ed_nosso >= 0) { lv_obj_t *ap = hi_botao(t, LV_SYMBOL_TRASH, COR_CARTAO2, 44, 44); lv_obj_set_pos(ap, x, 184); lv_obj_add_event_cb(ap, ev_apagar, LV_EVENT_CLICKED, NULL); x += 50; }
    lv_obj_t *us = hi_botao(t, LV_SYMBOL_OK "  Usar", COR_DESTAQUE, 100, 44); lv_obj_set_pos(us, x, 184);
    lv_obj_add_event_cb(us, ev_usar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *sv = hi_botao(t, LV_SYMBOL_SAVE "  Salvar", COR_OK, 312 - (x + 106), 44); lv_obj_set_pos(sv, x + 106, 184);
    lv_obj_add_event_cb(sv, ev_salvar, LV_EVENT_CLICKED, NULL);
    redesenhar();
    return t;
}
