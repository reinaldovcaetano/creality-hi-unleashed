/* Hi Painel: ajustes (rede, tela, sobre, logs, sistema) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"
#include "../dados/arquivos.h"
#include "../dados/wifi.h"

void hi_brilho(int v);          /* main_fb.c */
int  hi_brilho_atual(void);

static void reiniciar_fw(void *u) { (void)u; hi_gcode("FIRMWARE_RESTART"); hi_reiniciando("FIRMWARE_RESTART", NULL); }
static void ev_reiniciar(lv_event_t *e) { (void)e; if (hi_ocupada(&hi_e)) { hi_toast("Indisponível durante a impressão", COR_AVISO); return; } hi_confirmar("Reiniciar o Klipper?", "Reinicia o firmware (FIRMWARE_RESTART). Leva uns 20 s.", "Reiniciar", COR_AVISO, reiniciar_fw, NULL); }
static void reboot_sis(void *u) { (void)u; hi_toast("Reiniciando a impressora...", COR_AVISO); system("sync; (sleep 2; reboot) &"); }
static void ev_reboot(lv_event_t *e) { (void)e; if (hi_ocupada(&hi_e)) { hi_toast("Indisponível durante a impressão", COR_AVISO); return; } hi_confirmar("Reiniciar a impressora?", "Reinicia o sistema inteiro (leva ~1 min).", "Reiniciar", COR_AVISO, reboot_sis, NULL); }
static void ev_sair(lv_event_t *e) { (void)e; if (hi_sair_cb) hi_sair_cb(); }

static void exportar(void *u)
{
    (void)u;
    struct stat st;
    if (stat("/mnt/exUDISK", &st) || !S_ISDIR(st.st_mode)) { hi_toast("Coloque um pendrive na USB", COR_AVISO); return; }
    int r = system("D=/mnt/exUDISK/hi-logs-$(date +%Y%m%d-%H%M); mkdir -p $D && cp /mnt/UDISK/printer_data/logs/*.log /mnt/UDISK/printer_data/logs/*.txt $D/ 2>/dev/null; "
                   "cp -r /mnt/UDISK/creality/userdata/log $D/creality 2>/dev/null; dmesg > $D/dmesg.txt; sync");
    hi_toast(r == 0 ? "Logs copiados para o pendrive (pasta hi-logs-...)" : "Erro ao copiar os logs", r == 0 ? COR_OK : COR_ERRO);
}
static void ev_exportar(lv_event_t *e) { (void)e; hi_confirmar("Exportar logs?", "Copia os logs da impressora para o pendrive, para análise.", "Exportar", COR_DESTAQUE, exportar, NULL); }

lv_obj_t *tela_ajustes(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Ajustes", true);
    lv_obj_t *l = hi_lista(t, 32, HI_H - 32);
    char r[96]; snprintf(r, sizeof r, "%s  \xE2\x80\xA2  %s", hi_e.ssid[0] ? hi_e.ssid : "sem Wi-Fi", hi_e.ip[0] ? hi_e.ip : "sem IP");
    hi_ao_clicar(hi_item(l, LV_SYMBOL_WIFI, COR_OK, "Rede", r), tela_rede);
    hi_ao_clicar(hi_item(l, LV_SYMBOL_EYE_OPEN, COR_DESTAQUE, "Tela", "brilho e descanso"), tela_tela);
    { int v = hi_cfg_ventoinha_max(); char d[64]; if (v > 0) snprintf(d, sizeof d, "velocidade máxima %d%%%s", v, v == 50 ? " (padrão Creality)" : ""); else snprintf(d, sizeof d, "velocidade máxima");
      hi_ao_clicar(hi_item(l, LV_SYMBOL_REFRESH, COR_MESA, "Ventoinha da peça (bico)", d), tela_ventoinha); }
    { int p = hi_manutencao_pendentes(); char m[64]; if (p) snprintf(m, sizeof m, "%d item(ns) pendente(s)", p); else snprintf(m, sizeof m, "tudo em dia");
      hi_ao_clicar(hi_item(l, LV_SYMBOL_SETTINGS, p ? COR_AVISO : COR_OK, "Manutenção", m), tela_manutencao); }
    hi_ao_clicar(hi_item(l, LV_SYMBOL_LIST, COR_TEXTO2, "Sobre", "versões, sistema, memória"), tela_sobre);
    lv_obj_add_event_cb(hi_item(l, LV_SYMBOL_USB, COR_AVISO, "Exportar logs", "copia os logs para o pendrive"), ev_exportar, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(hi_item(l, LV_SYMBOL_REFRESH, COR_AVISO, "Reiniciar o Klipper", "FIRMWARE_RESTART"), ev_reiniciar, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(hi_item(l, LV_SYMBOL_POWER, COR_ERRO, "Reiniciar a impressora", "reinicia o sistema"), ev_reboot, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(hi_item(l, LV_SYMBOL_HOME, COR_TEXTO2, "Voltar para a tela Creality", "Wi-Fi, atualização, self-check, CFS"), ev_sair, LV_EVENT_CLICKED, NULL);
    return t;
}

/* ---------------- rede ---------------- */
static lv_obj_t *l_rede, *lista_redes, *l_wifi;
static char ssid_sel[64];
static void com_senha(const char *senha, void *u) { (void)u; hi_wifi_conectar(ssid_sel, senha); }
static char salvas[16][64]; static int n_salvas;
static int eh_salva(const char *ssid) { for (int i = 0; i < n_salvas; i++) if (!strcmp(salvas[i], ssid)) return 1; return 0; }
static void pedir_senha(void) { char t[96]; snprintf(t, sizeof t, "Senha de \"%s\" (vazio = rede aberta)", ssid_sel); hi_teclado_texto(t, "", com_senha, NULL); }
static void esquecer(void *u) { (void)u; hi_wifi_esquecer(ssid_sel); n_salvas = hi_wifi_salvas(salvas, 16); }
static void opcao_rede(int op, void *u)
{
    (void)u;
    if (op == 0) hi_wifi_conectar_salva(ssid_sel);
    else if (op == 1) pedir_senha();
    else if (op == 2) {
        char m[160]; snprintf(m, sizeof m, "A senha de \"%s\" será apagada da impressora.%s", ssid_sel,
                              !strcmp(ssid_sel, hi_e.ssid) ? "\nÉ a rede atual: a impressora vai desconectar!" : "");
        hi_confirmar("Esquecer a rede?", m, "Esquecer", COR_ERRO, esquecer, NULL);
    }
}
static void ev_rede(lv_event_t *e)
{
    lv_obj_t *it = lv_event_get_current_target(e);
    snprintf(ssid_sel, sizeof ssid_sel, "%s", lv_label_get_text(lv_obj_get_child(it, 1)));
    if (!eh_salva(ssid_sel)) { pedir_senha(); return; }
    int atual = !strcmp(ssid_sel, hi_e.ssid);
    const char *op[4] = {atual ? LV_SYMBOL_REFRESH "  Reconectar" : LV_SYMBOL_WIFI "  Conectar", LV_SYMBOL_EDIT "  Editar senha", LV_SYMBOL_TRASH "  Esquecer rede", LV_SYMBOL_LEFT "  Voltar"};
    lv_color_t cor[4] = {COR_OK, COR_CARTAO2, COR_ERRO, COR_CARTAO2};
    hi_menu(ssid_sel, atual ? "rede atual  \xE2\x80\xA2  senha salva" : "senha salva", op, cor, 4, opcao_rede, NULL);
}
static void rede_atualizar(const hi_estado_t *e)
{
    char b[256];
    char w[160]; int st = hi_wifi_status(w, sizeof w);
    if (st) { lv_label_set_text(l_wifi, w); lv_obj_set_style_text_color(l_wifi, st == 2 ? COR_OK : st == 3 ? COR_ERRO : COR_AVISO, 0); }
    snprintf(b, sizeof b, "Wi-Fi: %s\nFaixa: %s   Sinal: %d dBm\nIP: %s", e->ssid[0] ? e->ssid : "desconectado",
             e->freq_mhz > 4000 ? "5 GHz" : e->freq_mhz ? "2,4 GHz" : "-", e->sinal_dbm, e->ip[0] ? e->ip : "-");
    lv_label_set_text(l_rede, b);
}
static void procurar(lv_timer_t *tm)
{
    lv_timer_del(tm);
    lv_obj_clean(lista_redes);
    n_salvas = hi_wifi_salvas(salvas, 16);
    FILE *p = popen("iw dev wlan0 scan 2>/dev/null | awk '/^BSS/{f=\"\"} /freq:/{f=$2} /signal:/{s=$2} /SSID:/{sub(/^[ \\t]*SSID: /,\"\"); if($0!=\"\") print s\"|\"f\"|\"$0}' | sort -t'|' -k1 -rn | awk -F'|' '!v[$3]++' | head -15", "r");
    char l[200]; int n = 0;
    while (p && fgets(l, sizeof l, p)) {
        l[strcspn(l, "\n")] = 0;
        char *s = strtok(l, "|"), *f = strtok(NULL, "|"), *nome = strtok(NULL, "|");
        if (!nome) continue;
        char d[96]; snprintf(d, sizeof d, "%s  \xE2\x80\xA2  %s dBm%s", atoi(f) > 4000 ? "5 GHz" : "2,4 GHz", s,
                              !strcmp(nome, hi_e.ssid) ? "  \xE2\x80\xA2  conectada" : eh_salva(nome) ? "  \xE2\x80\xA2  salva" : "");
        lv_obj_t *it = hi_item(lista_redes, LV_SYMBOL_WIFI, atoi(f) > 4000 ? COR_OK : COR_TEXTO2, nome, d);
        lv_obj_add_event_cb(it, ev_rede, LV_EVENT_CLICKED, NULL);
        n++;
    }
    if (p) pclose(p);
    if (!n) hi_rotulo(lista_redes, "Nenhuma rede encontrada.", &hi_font_14, COR_TEXTO2);
}
static void ev_procurar(lv_event_t *e)
{
    (void)e; lv_obj_clean(lista_redes);
    hi_rotulo(lista_redes, "Procurando redes...", &hi_font_14, COR_TEXTO2);
    lv_timer_t *t = lv_timer_create(procurar, 50, NULL); lv_timer_set_repeat_count(t, 1);
}
lv_obj_t *tela_rede(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Rede", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 200, 60);
    l_rede = hi_rotulo(c, "", &hi_font_12, COR_TEXTO); lv_obj_set_pos(l_rede, 8, 4);
    lv_obj_t *b = hi_botao(t, LV_SYMBOL_REFRESH " Procurar", COR_DESTAQUE, 98, 60); lv_obj_set_pos(b, 214, 38);
    lv_obj_add_event_cb(b, ev_procurar, LV_EVENT_CLICKED, NULL);
    l_wifi = hi_rotulo(t, "Toque em Procurar e escolha a rede.", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(l_wifi, 10, 102);
    lv_label_set_long_mode(l_wifi, LV_LABEL_LONG_DOT); lv_obj_set_width(l_wifi, 300);
    lista_redes = hi_lista(t, 118, HI_H - 118);
    hi_wifi_limpar();
    hi_set_atualizar(rede_atualizar);
    return t;
}

/* ---------------- tela ---------------- */
static void ev_brilho(lv_event_t *e) { lv_obj_t *s = lv_event_get_target(e); int v = lv_slider_get_value(s); hi_brilho(v); hi_pref_set("brilho", v); }
static void ev_descanso(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e); hi_pref_set("descanso_min", v);
    lv_obj_t *pai = lv_obj_get_parent(lv_event_get_target(e));
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(pai); i++) lv_obj_set_style_bg_color(lv_obj_get_child(pai, i), COR_CARTAO2, 0);
    lv_obj_set_style_bg_color(lv_event_get_target(e), COR_DESTAQUE, 0);
}
lv_obj_t *tela_tela(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Tela", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 64);
    lv_obj_t *n = hi_rotulo(c, "Brilho", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 10, 6);
    lv_obj_t *s = lv_slider_create(c); lv_obj_set_size(s, 270, 12); lv_obj_set_pos(s, 16, 34);
    lv_slider_set_range(s, 20, 255); lv_slider_set_value(s, hi_brilho_atual(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s, COR_DESTAQUE, LV_PART_INDICATOR); lv_obj_set_style_bg_color(s, COR_TEXTO, LV_PART_KNOB);
    lv_obj_add_event_cb(s, ev_brilho, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *c2 = hi_cartao(t, 8, 110, 304, 80);
    lv_obj_t *n2 = hi_rotulo(c2, "Apagar a tela depois de", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n2, 10, 6);
    lv_obj_t *lin = lv_obj_create(c2); lv_obj_remove_style_all(lin); lv_obj_set_size(lin, 290, 40); lv_obj_set_pos(lin, 8, 30);
    lv_obj_set_flex_flow(lin, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(lin, 6, 0);
    int op[5] = {1, 3, 5, 10, 0}; const char *ot[5] = {"1 min", "3 min", "5 min", "10 min", "nunca"};
    int atual = hi_pref("descanso_min", 5);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = hi_botao(lin, ot[i], op[i] == atual ? COR_DESTAQUE : COR_CARTAO2, 52, 36);
        lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &hi_font_12, 0);
        lv_obj_add_event_cb(b, ev_descanso, LV_EVENT_CLICKED, (void *)(intptr_t)op[i]);
    }
    return t;
}

/* ---------------- sobre ---------------- */
static lv_obj_t *l_sobre;
static void sobre_atualizar(const hi_estado_t *e)
{
    char b[600], ver[160] = "?"; struct utsname u; uname(&u);
    FILE *f = fopen("/etc/hi-versao", "r"); if (f) { if (fgets(ver, sizeof ver, f)) ver[strcspn(ver, "\n")] = 0; fclose(f); }
    snprintf(b, sizeof b, "Hi Painel beta  \xE2\x80\xA2  LVGL 8.3\n%s\nKernel %s %s\nKlipper: %s\nIP %s  \xE2\x80\xA2  %s\nMemória livre %.0f MB  \xE2\x80\xA2  carga %.2f\nPlaca (MCU) %.0f\xC2\xB0" "C",
             ver, u.release, u.version, e->klippy, e->ip, e->rede, e->mem_livre_kb / 1024, e->carga, e->mcu);
    hi_br(b);
    lv_label_set_text(l_sobre, b);
}
lv_obj_t *tela_sobre(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Sobre", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 194);
    l_sobre = hi_rotulo(c, "", &hi_font_12, COR_TEXTO); lv_obj_set_pos(l_sobre, 10, 8);
    lv_label_set_long_mode(l_sobre, LV_LABEL_LONG_WRAP); lv_obj_set_width(l_sobre, 284);
    lv_obj_set_style_text_line_space(l_sobre, 3, 0);
    hi_set_atualizar(sobre_atualizar);
    return t;
}

/* ---------------- ventoinha da peça: velocidade máxima ---------------- */
static int vt_pct, vt_salvo, vt_testou, vt_gravado; static lv_obj_t *vt_valor, *vt_slider, *vt_real;
static void vt_mostrar(void) { char b[32]; snprintf(b, sizeof b, "%d%%", vt_pct); lv_label_set_text(vt_valor, b); lv_slider_set_value(vt_slider, vt_pct, LV_ANIM_OFF); }
static void ev_vt_slider(lv_event_t *e) { vt_pct = (lv_slider_get_value(lv_event_get_target(e)) + 2) / 5 * 5; vt_mostrar(); }
static void ev_vt_padrao(lv_event_t *e) { (void)e; vt_pct = 50; vt_mostrar(); hi_toast("Padrão Creality: 50% (ela dividia o valor por 2)", COR_DESTAQUE); }
static void vt_aplicar_vivo(void) { hi_gcode("SET_GCODE_VARIABLE MACRO=PRINTER_PARAM VARIABLE=fan0_max_pct VALUE=%d", vt_pct); }
static void ev_vt_testar(lv_event_t *e)
{
    (void)e; if (hi_ocupada(&hi_e)) { hi_toast("Indisponível durante a impressão", COR_AVISO); return; }
    vt_aplicar_vivo(); vt_testou = 1; hi_gcode("M106 S255"); hi_toast("Ventoinha no máximo escolhido (100% pedido)", COR_DESTAQUE);
}
static void ev_vt_parar(lv_event_t *e) { (void)e; if (!hi_ocupada(&hi_e)) hi_gcode("M107"); vt_testou = 0; hi_gcode("SET_GCODE_VARIABLE MACRO=PRINTER_PARAM VARIABLE=fan0_max_pct VALUE=%d", vt_salvo); }
static void ev_vt_salvar(lv_event_t *e)
{
    (void)e; char erro[160];
    if (!hi_cfg_ventoinha_max_gravar(vt_pct, erro, sizeof erro)) { hi_toast(erro, COR_ERRO); return; }
    vt_aplicar_vivo(); vt_gravado = 1;
    if (vt_testou && !hi_ocupada(&hi_e)) hi_gcode("M107");
    char r[48]; snprintf(r, sizeof r, "velocidade máxima %d%%", vt_pct); hi_calib_registrar("Ventoinha da peça", r);
    hi_toast("Salvo. Já vale para as próximas ordens de ventoinha.", COR_OK); hi_voltar();
}
/* ao sair da tela (Voltar) sem salvar: desliga a ventoinha do teste e desfaz o máximo de teste */
static void vt_ao_sair(lv_event_t *e)
{
    (void)e;
    if (vt_gravado) return;
    if (vt_testou && !hi_ocupada(&hi_e)) hi_gcode("M107");
    if (vt_testou) hi_gcode("SET_GCODE_VARIABLE MACRO=PRINTER_PARAM VARIABLE=fan0_max_pct VALUE=%d", vt_salvo);
}
static void vt_atualizar(const hi_estado_t *e) { char b[48]; snprintf(b, sizeof b, "agora: %.0f%% real", e->ventoinha * 100); lv_label_set_text(vt_real, b); }
lv_obj_t *tela_ventoinha(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Ventoinha da peça", true);
    vt_pct = hi_cfg_ventoinha_max(); if (vt_pct <= 0) vt_pct = 50;
    vt_salvo = vt_pct; vt_testou = 0; vt_gravado = 0;
    lv_obj_add_event_cb(t, vt_ao_sair, LV_EVENT_DELETE, NULL);
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 96);
    lv_obj_t *n = hi_rotulo(c, "Velocidade máxima real (100% no fatiador)", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 10, 6);
    vt_valor = hi_rotulo(c, "", &hi_font_28, COR_MESA); lv_obj_set_pos(vt_valor, 10, 24);
    vt_real = hi_rotulo(c, "", &hi_font_12, COR_TEXTO2); lv_obj_align(vt_real, LV_ALIGN_TOP_RIGHT, -10, 34);
    vt_slider = lv_slider_create(c); lv_obj_set_size(vt_slider, 270, 12); lv_obj_set_pos(vt_slider, 16, 72);
    lv_slider_set_range(vt_slider, 20, 100);
    lv_obj_set_style_bg_color(vt_slider, COR_MESA, LV_PART_INDICATOR); lv_obj_set_style_bg_color(vt_slider, COR_TEXTO, LV_PART_KNOB);
    lv_obj_add_event_cb(vt_slider, ev_vt_slider, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *i = hi_rotulo(t, "Creality: 50% (divide o pedido por 2, mín. 20%).", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(i, 10, 140);
    lv_obj_t *pd = hi_botao(t, "Padrão Creality", COR_CARTAO2, 120, 36); lv_obj_set_pos(pd, 8, 158);
    lv_obj_add_event_cb(pd, ev_vt_padrao, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ts = hi_botao(t, LV_SYMBOL_PLAY " Testar", COR_DESTAQUE, 88, 36); lv_obj_set_pos(ts, 134, 158);
    lv_obj_add_event_cb(ts, ev_vt_testar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pr = hi_botao(t, LV_SYMBOL_STOP " Parar", COR_CARTAO2, 84, 36); lv_obj_set_pos(pr, 228, 158);
    lv_obj_add_event_cb(pr, ev_vt_parar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *sv = hi_botao(t, LV_SYMBOL_SAVE "  Salvar", COR_OK, 304, 36); lv_obj_set_pos(sv, 8, 200);
    lv_obj_add_event_cb(sv, ev_vt_salvar, LV_EVENT_CLICKED, NULL);
    vt_mostrar();
    hi_set_atualizar(vt_atualizar);
    return t;
}
