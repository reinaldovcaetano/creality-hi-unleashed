/* Hi Painel: calibrações com valor padrão editável, execução ao vivo, resultado comparado e confirmação.
 * Conclusão de cada calibração: o Klipper fica "Printing" (idle_timeout) enquanto executa e volta a
 * "Ready"/"Idle" ao terminar; o resultado é lido do console (gcode_store) a partir do início. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glob.h>
#include <sys/stat.h>
#include "tema.h"
#include "comum.h"
#include "telas.h"
#include "../dados/arquivos.h"

#define GRAU "\xC2\xB0"
int hi_pid_mesa;

/* ---------------- execução genérica ---------------- */
typedef struct { int ativo, viu_ocupado, fim; double inicio; char console[8192]; int erro; } job_t;
static job_t job;
static void job_iniciar(void) { memset(&job, 0, sizeof job); job.ativo = 1; job.inicio = hi_agora() - 1; }
/* devolve 1 quando terminou (console carregado em job.console) */
static int job_passo(const hi_estado_t *e)
{
    if (!job.ativo || job.fim) return job.fim;
    if (!strcmp(e->ocioso, "Printing")) job.viu_ocupado = 1;
    static int cont; if (++cont % 4 == 0 || (job.viu_ocupado && strcmp(e->ocioso, "Printing"))) hi_console(job.console, sizeof job.console, job.inicio);
    if (strstr(job.console, "!! ")) job.erro = 1;
    if ((job.viu_ocupado && strcmp(e->ocioso, "Printing")) || job.erro || strcmp(e->klippy, "ready")) {
        hi_console(job.console, sizeof job.console, job.inicio);
        if (strstr(job.console, "!! ") || strcmp(e->klippy, "ready")) job.erro = 1;
        job.fim = 1;
    }
    return job.fim;
}
static const char *job_erro_txt(void)
{
    static char b[200]; char *p = strstr(job.console, "!! ");
    if (p) { snprintf(b, sizeof b, "%s", p + 3); b[strcspn(b, "\n")] = 0; } else snprintf(b, sizeof b, "%s", hi_e.klippy_msg);
    return b;
}
static bool pode_calibrar(void)
{
    if (hi_ocupada(&hi_e)) { hi_toast("Não dá para calibrar durante a impressão", COR_AVISO); return false; }
    if (strcmp(hi_e.klippy, "ready")) { hi_toast("Klipper não está pronto", COR_ERRO); return false; }
    if (!strcmp(hi_e.ocioso, "Printing")) { hi_toast("A impressora está executando outro comando", COR_AVISO); return false; }
    return true;
}
static void interromper(void *u) { (void)u; hi_req("POST", "/printer/emergency_stop"); hi_gcode("FIRMWARE_RESTART"); job.ativo = 0; hi_voltar(); hi_reiniciando("parada de emergência", "Interrompido. Klipper pronto."); }
static void ev_interromper(lv_event_t *e) { (void)e; hi_confirmar("Interromper?", "Para tudo na hora (parada de emergência) e reinicia o Klipper.", "Interromper", COR_ERRO, interromper, NULL); }
static void salvar_config(void *u) { hi_gcode("SAVE_CONFIG"); hi_reiniciando("gravando as calibrações (SAVE_CONFIG)", LV_SYMBOL_OK "  Salvo. Klipper pronto."); hi_calib_registrar((const char *)u, "salvo"); hi_voltar(); }

static lv_obj_t *grafico(lv_obj_t *t, int x, int y, int w, int h, int pontos, int ymin, int ymax)
{
    lv_obj_t *ch = lv_chart_create(t);
    lv_obj_set_size(ch, w, h); lv_obj_set_pos(ch, x, y);
    lv_obj_set_style_bg_color(ch, COR_CARTAO, 0); lv_obj_set_style_border_color(ch, COR_BORDA, 0);
    lv_obj_set_style_radius(ch, 10, 0); lv_obj_set_style_line_color(ch, COR_BORDA, LV_PART_MAIN);
    lv_chart_set_div_line_count(ch, 3, 4); lv_chart_set_point_count(ch, pontos);
    lv_chart_set_range(ch, LV_CHART_AXIS_PRIMARY_Y, ymin, ymax);
    lv_obj_set_style_size(ch, 0, LV_PART_INDICATOR);
    lv_chart_set_update_mode(ch, LV_CHART_UPDATE_MODE_SHIFT);
    return ch;
}

/* ---------------- aviso: calibração que encosta o bico na mesa precisa de bico sem filamento ---------------- */
static void (*depois_sem_filamento)(void);
static void op_filamento(int op, void *u) { (void)u; if (op == 0) hi_ir(tela_filamento); }
/* devolve true se pode seguir; se houver filamento no bico, mostra o aviso com o atalho para descarregar */
static bool sem_filamento_ou_avisar(void)
{
    if (!hi_e.filamento_no_bico) return true;
    const char *op[2] = {LV_SYMBOL_UPLOAD "  Descarregar agora", LV_SYMBOL_LEFT "  Voltar"};
    lv_color_t cor[2] = {COR_AVISO, COR_CARTAO2};
    hi_menu("Retire o filamento", "O bico é limpo e encosta no sensor da mesa.", op, cor, 2, op_filamento, NULL);
    return false;
}

/* ================= menu ================= */
static void ev_pid(lv_event_t *e) { hi_pid_mesa = (int)(intptr_t)lv_event_get_user_data(e); hi_ir(tela_pid); }

lv_obj_t *tela_calibracoes(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Calibrações", true);
    lv_obj_t *l = hi_lista(t, 32, HI_H - 32);
    double p[3]; char a[96], b[96], c[96], m[64], tx[16] = "?", fx[16] = "?", ty[16] = "?", fy[16] = "?", d[32];
    if (hi_cfg_ler_pid("extruder", p)) snprintf(a, sizeof a, "Kp %.2f  Ki %.3f  Kd %.2f", p[0], p[1], p[2]); else snprintf(a, sizeof a, "sem valores salvos");
    if (hi_cfg_ler_pid("heater_bed", p)) snprintf(b, sizeof b, "Kp %.2f  Ki %.3f  Kd %.2f", p[0], p[1], p[2]); else snprintf(b, sizeof b, "sem valores salvos");
    hi_cfg_valor("input_shaper", "shaper_type_x", tx, sizeof tx); hi_cfg_valor("input_shaper", "shaper_freq_x", fx, sizeof fx);
    hi_cfg_valor("input_shaper", "shaper_type_y", ty, sizeof ty); hi_cfg_valor("input_shaper", "shaper_freq_y", fy, sizeof fy);
    snprintf(c, sizeof c, "X %s %s Hz   Y %s %s Hz", tx, fx, ty, fy);
    snprintf(m, sizeof m, "sem filamento  \xE2\x80\xA2  variação %.3f mm", hi_e.malha_max - hi_e.malha_min);
    hi_br(a); hi_br(b); hi_br(c); hi_br(m);
    /* "Calibrar tudo" primeiro: quem quer rapidez resolve com um toque */
    lv_obj_t *tudo = hi_item_grande(l, LV_SYMBOL_REFRESH, COR_DESTAQUE, "Calibrar tudo", "home, vibração, PID e malha em sequência");
    lv_obj_set_style_bg_color(tudo, lv_color_mix(COR_DESTAQUE, COR_CARTAO, 70), 0);
    lv_obj_set_style_border_color(tudo, COR_DESTAQUE, 0);
    hi_ao_clicar(tudo, tela_calibrar_tudo);
    lv_obj_add_event_cb(hi_item_grande(l, LV_SYMBOL_CHARGE, COR_BICO, "PID do bico", a), ev_pid, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(hi_item_grande(l, LV_SYMBOL_CHARGE, COR_MESA, "PID da mesa", b), ev_pid, LV_EVENT_CLICKED, (void *)1);
    hi_ao_clicar(hi_item_grande(l, LV_SYMBOL_AUDIO, COR_DESTAQUE, "Vibração (shaper)", c), tela_shaper);
    hi_ao_clicar(hi_item_grande(l, LV_SYMBOL_IMAGE, COR_OK, "Nivelamento e malha", m), tela_malha);
    hi_ao_clicar(hi_item_grande(l, LV_SYMBOL_DOWNLOAD, COR_MESA, "Nivelamento Z", hi_calib_ultima("Nivelamento Z", d, sizeof d) ? d : "alinha os dois motores Z"), tela_ztilt);
    snprintf(a, sizeof a, "atual %.3f", hi_e.pa); hi_br(a);
    hi_ao_clicar(hi_item_grande(l, LV_SYMBOL_UP, COR_AVISO, "Pressure Advance", a), tela_pa);
    { char v[64]; snprintf(v, sizeof v, "%.0f mm/s  \xE2\x80\xA2  %.0f mm/s\xC2\xB2", hi_e.max_vel, hi_e.max_accel); hi_ao_clicar(hi_item_grande(l, LV_SYMBOL_GPS, COR_TEXTO2, "Velocidade ideal", v), tela_velocidade); }
    hi_ao_clicar(hi_item_grande(l, LV_SYMBOL_LIST, COR_TEXTO2, "Histórico", "todas as calibrações com data"), tela_historico_calib);
    return t;
}

/* ================= PID ================= */
static int pid_temp; static double pid_atual[3], pid_novo[3];
static lv_obj_t *pid_area, *l_pid_temp, *ch_pid, *l_pid_info;
static lv_chart_series_t *s_pid;
static int pid_fase;   /* 0 configurar, 1 rodando, 2 resultado */
static void pid_desenhar(void);
static void set_pid_temp(double v, void *u) { (void)u; pid_temp = (int)v; pid_desenhar(); }
static void ev_pid_temp(lv_event_t *e) { (void)e; hi_teclado_num(hi_pid_mesa ? "Temperatura da mesa" : "Temperatura do bico", pid_temp, hi_pid_mesa ? 40 : 150, hi_pid_mesa ? 110 : 300, 0, set_pid_temp, NULL); }
static void ev_pid_padrao(lv_event_t *e)
{
    (void)e; hi_fil_t f[HI_MAX_FIL]; int at; int n = hi_fil_carregar(f, HI_MAX_FIL, &at); (void)n;
    pid_temp = at >= 0 ? (hi_pid_mesa ? f[at].mesa : f[at].bico) : (hi_pid_mesa ? 60 : 230);
    pid_desenhar(); hi_toast("Temperatura padrão do filamento em uso", COR_DESTAQUE);
}
static void pid_iniciar(void *u)
{
    (void)u; job_iniciar(); pid_fase = 1;
    if (!hi_pid_mesa) hi_gcode("M106 S255");    /* ventoinha da peça no máximo configurado (Ajustes > Ventoinha da peça), como na impressão */
    hi_gcode("PID_CALIBRATE HEATER=%s TARGET=%d", hi_pid_mesa ? "heater_bed" : "extruder", pid_temp);
    pid_desenhar();
}
static void ev_pid_iniciar(lv_event_t *e)
{
    (void)e; if (!pode_calibrar()) return;
    char m[160]; snprintf(m, sizeof m, "Aquece %s a %d" GRAU "C e ajusta o controle.\nLeva de 5 a 10 minutos. Não toque no %s.", hi_pid_mesa ? "a mesa" : "o bico", pid_temp, hi_pid_mesa ? "mesa" : "bico");
    hi_confirmar("Calibrar PID?", m, LV_SYMBOL_PLAY "  Iniciar", COR_DESTAQUE, pid_iniciar, NULL);
}
static void pid_salvar_editado(void)
{
    char v[3][24], erro[160]; const char *k[3] = {"pid_kp", "pid_ki", "pid_kd"}; const char *vals[3] = {v[0], v[1], v[2]};
    for (int i = 0; i < 3; i++) snprintf(v[i], 24, "%.3f", pid_novo[i]);
    if (hi_cfg_autosave(hi_pid_mesa ? "heater_bed" : "extruder", k, vals, 3, erro, sizeof erro)) {
        char r[96]; snprintf(r, sizeof r, "Kp %s Ki %s Kd %s (%d" GRAU ")", v[0], v[1], v[2], pid_temp); hi_br(r);
        hi_calib_registrar(hi_pid_mesa ? "PID da mesa" : "PID do bico", r);
        hi_gcode("FIRMWARE_RESTART"); hi_voltar(); hi_reiniciando("PID gravado no printer.cfg", LV_SYMBOL_OK "  PID salvo. Klipper pronto.");
    } else hi_toast(erro, COR_ERRO);
}
static void pid_salvar(void *u) { (void)u; hi_gcode("TURN_OFF_HEATERS\nM107"); pid_salvar_editado(); }
static void ev_pid_salvar(lv_event_t *e) { (void)e; hi_confirmar("Salvar?", "Grava os novos valores no printer.cfg (com backup) e reinicia o Klipper.", LV_SYMBOL_SAVE "  Salvar", COR_OK, pid_salvar, NULL); }
static void set_pid_val(double v, void *u) { pid_novo[(int)(intptr_t)u] = v; pid_desenhar(); }
static void ev_pid_editar(lv_event_t *e) { int i = (int)(intptr_t)lv_event_get_user_data(e); const char *n[3] = {"Kp", "Ki", "Kd"}; hi_teclado_num(n[i], pid_novo[i], 0, 1000, 3, set_pid_val, (void *)(intptr_t)i); }
static void ev_pid_descartar(lv_event_t *e)
{
    (void)e; hi_gcode("TURN_OFF_HEATERS\nM107\nFIRMWARE_RESTART");   /* reinicia para o resultado não ficar pendente */
    hi_voltar(); hi_reiniciando("descartando o resultado", "Descartado. Os valores atuais continuam.");
}

static void pid_desenhar(void)
{
    lv_obj_clean(pid_area);
    char b[128];
    if (pid_fase == 0) {
        lv_obj_t *c = hi_cartao(pid_area, 8, 4, 170, 70);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(c, ev_pid_temp, LV_EVENT_CLICKED, NULL);
        lv_obj_t *n = hi_rotulo(c, "Temperatura  " LV_SYMBOL_EDIT, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 10, 6);
        snprintf(b, sizeof b, "%d " GRAU "C", pid_temp);
        l_pid_temp = hi_rotulo(c, b, &hi_font_28, hi_pid_mesa ? COR_MESA : COR_BICO); lv_obj_set_pos(l_pid_temp, 10, 26);
        lv_obj_t *pd = hi_botao(pid_area, "Padrão do\nfilamento", COR_CARTAO2, 128, 70); lv_obj_set_pos(pd, 184, 4);
        lv_obj_add_event_cb(pd, ev_pid_padrao, LV_EVENT_CLICKED, NULL);
        lv_obj_t *at = hi_cartao(pid_area, 8, 80, 304, 64);
        char u[32]; bool tem = hi_calib_ultima(hi_pid_mesa ? "PID da mesa" : "PID do bico", u, sizeof u);
        snprintf(b, sizeof b, "Valores atuais%s%s", tem ? "  \xE2\x80\xA2  última: " : "", tem ? u : "");
        lv_obj_t *a1 = hi_rotulo(at, b, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(a1, 10, 6);
        snprintf(b, sizeof b, "Kp %.2f    Ki %.3f    Kd %.2f", pid_atual[0], pid_atual[1], pid_atual[2]); hi_br(b);
        lv_obj_t *a2 = hi_rotulo(at, b, &hi_font_16, COR_TEXTO); lv_obj_set_pos(a2, 10, 30);
        lv_obj_t *go = hi_botao(pid_area, LV_SYMBOL_PLAY "  Iniciar calibração", COR_DESTAQUE, 304, 44); lv_obj_set_pos(go, 8, 150);
        lv_obj_add_event_cb(go, ev_pid_iniciar, LV_EVENT_CLICKED, NULL);
    } else if (pid_fase == 1) {
        ch_pid = grafico(pid_area, 8, 4, 304, 120, 120, 0, hi_pid_mesa ? 120 : 300);
        s_pid = lv_chart_add_series(ch_pid, hi_pid_mesa ? COR_MESA : COR_BICO, LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_set_all_value(ch_pid, s_pid, LV_CHART_POINT_NONE);
        l_pid_info = hi_rotulo(pid_area, "Calibrando...", &hi_font_14, COR_TEXTO); lv_obj_set_pos(l_pid_info, 10, 130);
        lv_obj_t *x = hi_botao(pid_area, LV_SYMBOL_STOP "  Interromper", COR_ERRO, 304, 40); lv_obj_set_pos(x, 8, 154);
        lv_obj_add_event_cb(x, ev_interromper, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_t *c = hi_cartao(pid_area, 8, 4, 304, 140);
        lv_obj_t *h1 = hi_rotulo(c, "atual", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(h1, 70, 6);
        lv_obj_t *h2 = hi_rotulo(c, "novo (toque para editar)", &hi_font_12, COR_OK); lv_obj_set_pos(h2, 160, 6);
        const char *nm[3] = {"Kp", "Ki", "Kd"};
        for (int i = 0; i < 3; i++) {
            lv_obj_t *a = hi_rotulo(c, nm[i], &hi_font_16, COR_TEXTO2); lv_obj_set_pos(a, 12, 30 + i * 36);
            snprintf(b, sizeof b, "%.3f", pid_atual[i]); hi_br(b);
            lv_obj_t *v = hi_rotulo(c, b, &hi_font_16, COR_TEXTO2); lv_obj_set_pos(v, 70, 30 + i * 36);
            snprintf(b, sizeof b, "%.3f  " LV_SYMBOL_EDIT, pid_novo[i]); hi_br(b);
            lv_obj_t *bn = hi_botao(c, b, COR_CARTAO2, 130, 30); lv_obj_set_pos(bn, 160, 26 + i * 36);
            lv_obj_add_event_cb(bn, ev_pid_editar, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        lv_obj_t *x = hi_botao(pid_area, "Descartar", COR_CARTAO2, 120, 44); lv_obj_set_pos(x, 8, 150);
        lv_obj_add_event_cb(x, ev_pid_descartar, LV_EVENT_CLICKED, NULL);
        lv_obj_t *s = hi_botao(pid_area, LV_SYMBOL_SAVE "  Salvar", COR_OK, 176, 44); lv_obj_set_pos(s, 136, 150);
        lv_obj_add_event_cb(s, ev_pid_salvar, LV_EVENT_CLICKED, NULL);
    }
}
static void pid_atualizar(const hi_estado_t *e)
{
    if (pid_fase != 1) return;
    lv_chart_set_next_value(ch_pid, s_pid, (lv_coord_t)(hi_pid_mesa ? e->mesa : e->bico));
    char b[128]; double dt = hi_agora() - job.inicio;
    snprintf(b, sizeof b, "%.0f / %d" GRAU "C   \xE2\x80\xA2   %dmin %02ds", hi_pid_mesa ? e->mesa : e->bico, pid_temp, (int)dt / 60, (int)dt % 60);
    lv_label_set_text(l_pid_info, b);
    if (job_passo(e)) {
        char *p = strstr(job.console, "pid_Kp=");
        if (p && sscanf(p, "pid_Kp=%lf pid_Ki=%lf pid_Kd=%lf", &pid_novo[0], &pid_novo[1], &pid_novo[2]) == 3) {
            pid_fase = 2; hi_gcode("M107"); pid_desenhar();
        } else {
            pid_fase = 0; hi_gcode("TURN_OFF_HEATERS\nM107");
            char m[240]; snprintf(m, sizeof m, "A calibração não terminou: %s", job.erro ? job_erro_txt() : "resultado não encontrado");
            hi_toast(m, COR_ERRO); pid_desenhar();
        }
        job.ativo = 0;
    }
}
lv_obj_t *tela_pid(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, hi_pid_mesa ? "PID da mesa" : "PID do bico", true);
    hi_cfg_ler_pid(hi_pid_mesa ? "heater_bed" : "extruder", pid_atual);
    hi_fil_t f[HI_MAX_FIL]; int at; hi_fil_carregar(f, HI_MAX_FIL, &at);
    pid_temp = at >= 0 ? (hi_pid_mesa ? f[at].mesa : f[at].bico) : (hi_pid_mesa ? 60 : 230);
    pid_fase = 0;
    pid_area = lv_obj_create(t); lv_obj_remove_style_all(pid_area); lv_obj_set_size(pid_area, HI_W, HI_H - 34); lv_obj_set_pos(pid_area, 0, 34);
    lv_obj_clear_flag(pid_area, LV_OBJ_FLAG_SCROLLABLE);
    pid_desenhar();
    hi_set_atualizar(pid_atualizar);
    return t;
}

/* ================= vibração (input shaper) ================= */
typedef struct { char nome[8]; double hz, vib, suav, acel; } shp_t;
static shp_t shp[2][8]; static int n_shp[2], rec_shp[2], esc_shp[2];
static int sh_fase, sh_eixos, sh_eixo_vista;   /* eixos: 1=X 2=Y 3=ambos */
static lv_obj_t *sh_area, *l_sh_info;

static void sh_desenhar(void);
static void parse_shaper(const char *txt, int ax)
{
    n_shp[ax] = 0; rec_shp[ax] = 0;
    const char *p = txt;
    while ((p = strstr(p, "Fitted shaper '"))) {
        shp_t s = {0};
        if (sscanf(p, "Fitted shaper '%7[^']' frequency = %lf Hz (vibrations = %lf%%, smoothing ~= %lf)", s.nome, &s.hz, &s.vib, &s.suav) >= 2 && n_shp[ax] < 8) {
            const char *q = strstr(p, "max_accel <= ");
            if (q && q - p < 300) s.acel = atof(q + 13);
            shp[ax][n_shp[ax]++] = s;
        }
        p++;
    }
    const char *r = strstr(txt, "Recommended shaper");
    char nm[8] = "";
    if (r && (sscanf(r, "Recommended shaper_type_%*c = %7[^,]", nm) == 1 || sscanf(r, "Recommended shaper is %7s", nm) == 1))
        for (int i = 0; i < n_shp[ax]; i++) if (!strcmp(shp[ax][i].nome, nm)) rec_shp[ax] = i;
    esc_shp[ax] = rec_shp[ax];
}
static void sh_iniciar(void *u)
{
    (void)u; job_iniciar(); sh_fase = 1;
    if (sh_eixos == 3) hi_gcode("G28\nSHAPER_CALIBRATE");
    else hi_gcode("G28\nSHAPER_CALIBRATE AXIS=%s", sh_eixos == 1 ? "X" : "Y");
    sh_desenhar();
}
static void ev_sh_eixo(lv_event_t *e) { sh_eixos = (int)(intptr_t)lv_event_get_user_data(e); sh_desenhar(); }
static void ev_sh_iniciar(lv_event_t *e)
{
    (void)e; if (!pode_calibrar()) return;
    hi_confirmar("Medir vibração?", "Faz Home e vibra o cabeçote (barulho alto por ~1 min por eixo). Deixe a mesa livre.", LV_SYMBOL_PLAY "  Iniciar", COR_DESTAQUE, sh_iniciar, NULL);
}
static void ev_sh_vista(lv_event_t *e) { sh_eixo_vista = (int)(intptr_t)lv_event_get_user_data(e); sh_desenhar(); }
static void ev_sh_escolher(lv_event_t *e) { esc_shp[sh_eixo_vista] = (int)(intptr_t)lv_event_get_user_data(e); sh_desenhar(); }
static void sh_aplicar(void *u)
{
    (void)u;
    const char *k[4]; const char *v[4]; char vb[4][24]; int n = 0;
    for (int ax = 0; ax < 2; ax++) {
        if (!n_shp[ax]) continue;
        shp_t *s = &shp[ax][esc_shp[ax]];
        k[n] = ax ? "shaper_type_y" : "shaper_type_x"; snprintf(vb[n], 24, "%s", s->nome); v[n] = vb[n]; n++;
        k[n] = ax ? "shaper_freq_y" : "shaper_freq_x"; snprintf(vb[n], 24, "%.1f", s->hz); v[n] = vb[n]; n++;
    }
    char erro[160];
    if (n && hi_cfg_autosave("input_shaper", k, v, n, erro, sizeof erro)) {
        char r[96] = "";
        for (int ax = 0; ax < 2; ax++) if (n_shp[ax]) { char x[48]; snprintf(x, sizeof x, "%s %s %.1f Hz  ", ax ? "Y" : "X", shp[ax][esc_shp[ax]].nome, shp[ax][esc_shp[ax]].hz); strcat(r, x); }
        hi_calib_registrar("Vibração", hi_br(r));
        hi_gcode("FIRMWARE_RESTART"); hi_voltar(); hi_reiniciando("shaper gravado no printer.cfg", LV_SYMBOL_OK "  Shaper salvo. Klipper pronto.");
    } else hi_toast(n ? erro : "Nada para aplicar", COR_ERRO);
}
static void ev_sh_aplicar(lv_event_t *e) { (void)e; hi_confirmar("Aplicar e salvar?", "Grava o shaper escolhido de cada eixo (com backup do printer.cfg) e reinicia o Klipper.", LV_SYMBOL_SAVE "  Salvar", COR_OK, sh_aplicar, NULL); }

/* gráfico a partir do CSV do Klipper (/tmp/calibration_data_<eixo>_*.csv): freq, psd_x, psd_y, psd_z, psd_xyz, shapers... */
static void sh_grafico(lv_obj_t *pai, int ax)
{
    glob_t g; char pad[64]; snprintf(pad, sizeof pad, "/tmp/calibration_data_%c_*.csv", ax ? 'y' : 'x');
    lv_obj_t *ch = grafico(pai, 8, 4, 186, 120, 90, 0, 100);
    lv_chart_set_update_mode(ch, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_chart_series_t *s1 = lv_chart_add_series(ch, COR_DESTAQUE, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t *s2 = lv_chart_add_series(ch, COR_OK, LV_CHART_AXIS_PRIMARY_Y);
    if (glob(pad, 0, NULL, &g) || !g.gl_pathc) { lv_obj_t *l = hi_rotulo(ch, "gráfico indisponível", &hi_font_12, COR_TEXTO2); lv_obj_center(l); return; }
    const char *arq = g.gl_pathv[g.gl_pathc - 1];
    struct stat st, sm; for (size_t i = 0; i < g.gl_pathc; i++) if (!stat(g.gl_pathv[i], &st) && !stat(arq, &sm) && st.st_mtime > sm.st_mtime) arq = g.gl_pathv[i];
    FILE *f = fopen(arq, "r"); globfree(&g); if (!f) return;
    char cab[1024]; if (!fgets(cab, sizeof cab, f)) { fclose(f); return; }
    /* coluna do shaper escolhido */
    int col_sh = -1, col = 0; char *tok, *sp, cc[1024]; snprintf(cc, sizeof cc, "%s", cab);
    for (tok = strtok_r(cc, ",\n", &sp); tok; tok = strtok_r(NULL, ",\n", &sp), col++)
        if (n_shp[ax] && !strncmp(tok, shp[ax][esc_shp[ax]].nome, strlen(shp[ax][esc_shp[ax]].nome)) && tok[strlen(shp[ax][esc_shp[ax]].nome)] == '(') col_sh = col;
    double fr[400], psd[400], resp[400]; int n = 0; double mx = 1e-12; char l[1024];
    while (n < 400 && fgets(l, sizeof l, f)) {
        double v[16] = {0}; int k = 0; for (tok = strtok_r(l, ",", &sp); tok && k < 16; tok = strtok_r(NULL, ",", &sp)) v[k++] = atof(tok);
        if (k < 5) continue;
        fr[n] = v[0]; psd[n] = v[4]; resp[n] = col_sh > 0 && col_sh < k ? v[col_sh] : 1; if (psd[n] > mx) mx = psd[n]; n++;
    }
    fclose(f);
    for (int i = 0; i < 90; i++) {                    /* reamostra 0..90 Hz em 90 pontos */
        double alvo = i, val = 0, filt = 0;
        for (int j = 0; j < n; j++) if (fr[j] >= alvo) { val = psd[j] / mx * 100; filt = val * resp[j]; break; }
        lv_chart_set_value_by_id(ch, s1, i, (lv_coord_t)val); lv_chart_set_value_by_id(ch, s2, i, (lv_coord_t)filt);
    }
}

static void sh_desenhar(void)
{
    lv_obj_clean(sh_area);
    char b[128];
    if (sh_fase == 0) {
        lv_obj_t *i = hi_rotulo(sh_area, "Mede a ressonância com os acelerômetros da Hi\n(um por eixo) e recomenda o filtro e a\naceleração ideal de cada eixo.", &hi_font_12, COR_TEXTO2);
        lv_obj_set_pos(i, 10, 4);
        const char *n[3] = {"Eixo X", "Eixo Y", "X e Y"};
        for (int k = 0; k < 3; k++) {
            lv_obj_t *bt = hi_botao(sh_area, n[k], sh_eixos == k + 1 ? COR_DESTAQUE : COR_CARTAO2, 98, 44); lv_obj_set_pos(bt, 8 + k * 103, 64);
            lv_obj_add_event_cb(bt, ev_sh_eixo, LV_EVENT_CLICKED, (void *)(intptr_t)(k + 1));
        }
        char x[16] = "?", fx[16] = "?", y[16] = "?", fy[16] = "?";
        hi_cfg_valor("input_shaper", "shaper_type_x", x, sizeof x); hi_cfg_valor("input_shaper", "shaper_freq_x", fx, sizeof fx);
        hi_cfg_valor("input_shaper", "shaper_type_y", y, sizeof y); hi_cfg_valor("input_shaper", "shaper_freq_y", fy, sizeof fy);
        snprintf(b, sizeof b, "Atual:  X %s %s Hz    Y %s %s Hz\nAceleração máxima: %.0f mm/s\xC2\xB2", x, fx, y, fy, hi_e.max_accel); hi_br(b);
        lv_obj_t *a = hi_rotulo(sh_area, b, &hi_font_12, COR_TEXTO); lv_obj_set_pos(a, 10, 116);
        lv_obj_t *go = hi_botao(sh_area, LV_SYMBOL_PLAY "  Medir", COR_DESTAQUE, 304, 44); lv_obj_set_pos(go, 8, 156);
        lv_obj_add_event_cb(go, ev_sh_iniciar, LV_EVENT_CLICKED, NULL);
    } else if (sh_fase == 1) {
        lv_obj_t *sp = lv_spinner_create(sh_area, 1200, 60); lv_obj_set_size(sp, 60, 60); lv_obj_align(sp, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_arc_color(sp, COR_DESTAQUE, LV_PART_INDICATOR);
        l_sh_info = hi_rotulo(sh_area, "Medindo...", &hi_font_14, COR_TEXTO); lv_obj_align(l_sh_info, LV_ALIGN_TOP_MID, 0, 80);
        lv_obj_t *x = hi_botao(sh_area, LV_SYMBOL_STOP "  Interromper", COR_ERRO, 304, 40); lv_obj_set_pos(x, 8, 156);
        lv_obj_add_event_cb(x, ev_interromper, LV_EVENT_CLICKED, NULL);
    } else {
        int ax = sh_eixo_vista;
        if (n_shp[0] && n_shp[1]) {
            for (int k = 0; k < 2; k++) {
                lv_obj_t *bt = hi_botao(sh_area, k ? "Y" : "X", ax == k ? COR_DESTAQUE : COR_CARTAO2, 40, 24);
                lv_obj_set_pos(bt, 200 + k * 46, 4); lv_obj_add_event_cb(bt, ev_sh_vista, LV_EVENT_CLICKED, (void *)(intptr_t)k);
            }
        }
        sh_grafico(sh_area, ax);
        lv_obj_t *c = hi_cartao(sh_area, 200, n_shp[0] && n_shp[1] ? 32 : 4, 112, n_shp[0] && n_shp[1] ? 92 : 120);
        if (n_shp[ax]) {
            shp_t *s = &shp[ax][esc_shp[ax]];
            snprintf(b, sizeof b, "%s %s\n%.1f Hz", ax ? "Y" : "X", s->nome, s->hz); hi_br(b);
            lv_obj_t *r = hi_rotulo(c, b, &hi_font_14, esc_shp[ax] == rec_shp[ax] ? COR_OK : COR_AVISO); lv_obj_set_pos(r, 8, 4);
            snprintf(b, sizeof b, "acel. ideal\n%.0f mm/s\xC2\xB2\nvibra %.1f%%", s->acel, s->vib); hi_br(b);
            lv_obj_t *a = hi_rotulo(c, b, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(a, 8, 40);
        } else { lv_obj_t *a = hi_rotulo(c, "sem resultado", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(a, 8, 8); }
        /* escolha entre os shapers calculados */
        lv_obj_t *lin = lv_obj_create(sh_area); lv_obj_remove_style_all(lin);
        lv_obj_set_size(lin, 304, 30); lv_obj_set_pos(lin, 8, 128); lv_obj_set_flex_flow(lin, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(lin, 4, 0); lv_obj_set_scroll_dir(lin, LV_DIR_HOR);
        for (int i = 0; i < n_shp[ax]; i++) {
            lv_obj_t *bt = hi_botao(lin, shp[ax][i].nome, i == esc_shp[ax] ? COR_DESTAQUE : COR_CARTAO2, 56, 28);
            if (i == rec_shp[ax]) lv_obj_set_style_border_color(bt, COR_OK, 0), lv_obj_set_style_border_width(bt, 2, 0);
            lv_obj_add_event_cb(bt, ev_sh_escolher, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        double lim = 1e9; for (int k = 0; k < 2; k++) if (n_shp[k] && shp[k][esc_shp[k]].acel > 0 && shp[k][esc_shp[k]].acel < lim) lim = shp[k][esc_shp[k]].acel;
        if (lim < 1e9) { snprintf(b, sizeof b, "limite seguro da máquina: %.0f mm/s\xC2\xB2", lim); lv_obj_t *li = hi_rotulo(sh_area, b, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(li, 10, 162); }
        lv_obj_t *ap = hi_botao(sh_area, LV_SYMBOL_SAVE "  Aplicar e salvar", COR_OK, 304, 34); lv_obj_set_pos(ap, 8, 172);
        lv_obj_add_event_cb(ap, ev_sh_aplicar, LV_EVENT_CLICKED, NULL);
    }
}
static void sh_atualizar(const hi_estado_t *e)
{
    if (sh_fase != 1) return;
    double dt = hi_agora() - job.inicio; char b[96];
    snprintf(b, sizeof b, "Medindo %s...  %dmin %02ds", sh_eixos == 3 ? "X e Y" : sh_eixos == 1 ? "X" : "Y", (int)dt / 60, (int)dt % 60);
    lv_label_set_text(l_sh_info, b);
    if (job_passo(e)) {
        job.ativo = 0;
        /* separa o texto por eixo (o Klipper calcula X e depois Y) */
        char *py = strstr(job.console, "Calculating the best input shaper parameters for y axis");
        char *px = strstr(job.console, "Calculating the best input shaper parameters for x axis");
        memset(n_shp, 0, sizeof n_shp);
        if (px) { char tmp = 0; if (py && py > px) { tmp = *py; *py = 0; } parse_shaper(px, 0); if (py && tmp) *py = tmp; }
        if (py) parse_shaper(py, 1);
        if (!px && !py) parse_shaper(job.console, sh_eixos == 2 ? 1 : 0);
        if (n_shp[0] || n_shp[1]) {
            sh_fase = 2; sh_eixo_vista = n_shp[0] ? 0 : 1;
            for (int ax = 0; ax < 2; ax++) if (n_shp[ax] && shp[ax][rec_shp[ax]].acel > 0) hi_pref_set(ax ? "acel_rec_y" : "acel_rec_x", (int)shp[ax][rec_shp[ax]].acel);
        }
        else { sh_fase = 0; char m[240]; snprintf(m, sizeof m, "Sem resultado: %s", job.erro ? job_erro_txt() : "formato desconhecido (veja o console no Fluidd)"); hi_toast(m, COR_ERRO); }
        sh_desenhar();
    }
}
lv_obj_t *tela_shaper(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Vibração (input shaper)", true);
    sh_fase = 0; if (!sh_eixos) sh_eixos = 3;
    sh_area = lv_obj_create(t); lv_obj_remove_style_all(sh_area); lv_obj_set_size(sh_area, HI_W, HI_H - 34); lv_obj_set_pos(sh_area, 0, 34);
    lv_obj_clear_flag(sh_area, LV_OBJ_FLAG_SCROLLABLE);
    sh_desenhar();
    hi_set_atualizar(sh_atualizar);
    return t;
}

/* ================= malha ================= */
static int mh_fase, mh_temp;
static lv_obj_t *mh_area, *l_mh_info;
static void mh_desenhar(void);
/* Ajuste 13/11: mesma calibração da tela original: macro G29 da Creality = limpa o bico (NOZ_CLEAR), G28 Z,
 * Z_TILT_ADJUST, BED_MESH_CALIBRATE e grava sozinha (CXSAVE_CONFIG). Mesa a 60 °C (definido pela macro). */
static void mh_iniciar(void *u) { (void)u; job_iniciar(); mh_fase = 1; hi_gcode("G29"); mh_desenhar(); }
static void op_pendente(int op, void *u)
{
    (void)u;
    if (op == 0) mh_iniciar(NULL);                                        /* gravar junto */
    else if (op == 1) { hi_gcode("FIRMWARE_RESTART"); hi_reiniciando("descartando pendências", "Pendências descartadas. Toque em Calibrar."); }
}
static void ev_mh_iniciar(lv_event_t *e)
{
    (void)e; if (!pode_calibrar() || !sem_filamento_ou_avisar()) return;
    if (hi_e.config_pendente) {
        const char *op[3] = {LV_SYMBOL_SAVE "  Gravar junto", LV_SYMBOL_TRASH "  Descartar pendências", LV_SYMBOL_LEFT "  Voltar"};
        lv_color_t cor[3] = {COR_OK, COR_AVISO, COR_CARTAO2};
        hi_menu("Calibração não salva", "Há um resultado calculado e não gravado.", op, cor, 3, op_pendente, NULL);
        return;
    }
    hi_confirmar("Nivelar e medir a malha?", "Igual à tela original: limpa o bico, nivela os dois Z e mede a malha com o sensor de pressão (mesa a 60\xC2\xB0" "C). Grava sozinha no fim. Mesa limpa e livre.", LV_SYMBOL_PLAY "  Iniciar", COR_DESTAQUE, mh_iniciar, NULL);
}
static void mh_mapa(lv_obj_t *pai)
{
    int nl = hi_e.malha_lin, nc = hi_e.malha_col;
    lv_obj_t *g = hi_cartao(pai, 8, 4, 196, 196);
    if (!nl || !nc) { lv_obj_t *l = hi_rotulo(g, "sem malha", &hi_font_14, COR_TEXTO2); lv_obj_center(l); return; }
    int cel = 184 / (nl > nc ? nl : nc);
    double meio = (hi_e.malha_max + hi_e.malha_min) / 2, amp = fmax(0.05, (hi_e.malha_max - hi_e.malha_min) / 2);
    for (int y = 0; y < nl; y++) for (int x = 0; x < nc; x++) {
        double v = hi_e.malha[nl - 1 - y][x], r = (v - meio) / amp;   /* -1..1 */
        lv_color_t cor = r < 0 ? lv_color_mix(COR_MESA, COR_OK, (uint8_t)(fmin(1, -r) * 255)) : lv_color_mix(COR_ERRO, COR_OK, (uint8_t)(fmin(1, r) * 255));
        lv_obj_t *q = lv_obj_create(g); lv_obj_remove_style_all(q);
        lv_obj_set_size(q, cel - 2, cel - 2); lv_obj_set_pos(q, 6 + x * cel, 6 + y * cel);
        lv_obj_set_style_radius(q, 3, 0); lv_obj_set_style_bg_color(q, cor, 0); lv_obj_set_style_bg_opa(q, LV_OPA_COVER, 0);
        if (cel >= 34) { char b[16]; snprintf(b, sizeof b, "%+.2f", v); hi_br(b); lv_obj_t *l = hi_rotulo(q, b, &hi_font_12, lv_color_black()); lv_obj_center(l); }
    }
}
static void mh_desenhar(void)
{
    lv_obj_clean(mh_area);
    char b[128];
    mh_mapa(mh_area);
    lv_obj_t *c = hi_cartao(mh_area, 210, 4, 102, 92);
    snprintf(b, sizeof b, "Variação\n%.3f mm", hi_e.malha_max - hi_e.malha_min); hi_br(b);
    lv_obj_t *v = hi_rotulo(c, b, &hi_font_14, COR_TEXTO); lv_obj_set_pos(v, 8, 6);
    snprintf(b, sizeof b, "%dx%d  \xE2\x80\xA2  %s", hi_e.malha_col, hi_e.malha_lin, hi_e.malha_perfil);
    lv_obj_t *p = hi_rotulo(c, b, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(p, 8, 52);
    if (mh_fase == 1) {
        l_mh_info = hi_rotulo(mh_area, "Medindo...", &hi_font_12, COR_TEXTO); lv_obj_set_pos(l_mh_info, 212, 104);
        lv_obj_t *x = hi_botao(mh_area, LV_SYMBOL_STOP " Parar", COR_ERRO, 102, 44); lv_obj_set_pos(x, 210, 156);
        lv_obj_add_event_cb(x, ev_interromper, LV_EVENT_CLICKED, NULL);
        return;
    }
    lv_obj_t *aviso = hi_rotulo(mh_area, hi_e.filamento_no_bico ? LV_SYMBOL_WARNING " retire o\nfilamento antes" : LV_SYMBOL_OK " sem filamento\nno bico", &hi_font_12, hi_e.filamento_no_bico ? COR_AVISO : COR_OK);
    lv_obj_set_pos(aviso, 212, 102);
    lv_obj_t *m = hi_botao(mh_area, LV_SYMBOL_REFRESH " Calibrar", COR_DESTAQUE, 102, 44); lv_obj_set_pos(m, 210, 152);
    lv_obj_add_event_cb(m, ev_mh_iniciar, LV_EVENT_CLICKED, NULL);
}
static void mh_atualizar(const hi_estado_t *e)
{
    if (mh_fase != 1) return;
    char b[96]; double dt = hi_agora() - job.inicio;
    if (e->mesa_alvo > 0 && e->mesa < e->mesa_alvo - 1.5) snprintf(b, sizeof b, "Aquecendo\n%.0f/%.0f" GRAU, e->mesa, e->mesa_alvo);
    else snprintf(b, sizeof b, "Medindo...\n%dmin %02ds", (int)dt / 60, (int)dt % 60);
    lv_label_set_text(l_mh_info, b);
    if (job_passo(e)) {
        job.ativo = 0; mh_fase = job.erro ? 0 : 2;
        if (job.erro) { char m[240]; snprintf(m, sizeof m, "Falhou: %s", job_erro_txt()); hi_toast(m, COR_ERRO); }
        else { char r[64]; snprintf(r, sizeof r, "variação %.3f mm", e->malha_max - e->malha_min); hi_calib_registrar("Nivelamento e malha", hi_br(r)); hi_toast("Nivelado e medido. Já ficou gravado.", COR_OK); }
        mh_desenhar();
    }
}
lv_obj_t *tela_malha(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Nivelamento e malha", true);
    hi_fil_t f[HI_MAX_FIL]; int at; hi_fil_carregar(f, HI_MAX_FIL, &at); mh_temp = at >= 0 ? f[at].mesa : 60;
    mh_fase = 0;
    mh_area = lv_obj_create(t); lv_obj_remove_style_all(mh_area); lv_obj_set_size(mh_area, HI_W, HI_H - 34); lv_obj_set_pos(mh_area, 0, 34);
    lv_obj_clear_flag(mh_area, LV_OBJ_FLAG_SCROLLABLE);
    mh_desenhar();
    hi_set_atualizar(mh_atualizar);
    return t;
}

/* ================= nivelamento Z (dois motores) ================= */
static lv_obj_t *zt_info; static int zt_rodando;
static void zt_iniciar(void *u) { (void)u; job_iniciar(); zt_rodando = 1; hi_gcode("G28\nZ_TILT_ADJUST"); lv_label_set_text(zt_info, "Medindo e ajustando os dois motores Z..."); }
static void ev_zt(lv_event_t *e) { (void)e; if (!pode_calibrar()) return; hi_confirmar("Nivelar Z?", "Faz Home, mede os dois lados da mesa e corrige a diferença entre os motores Z.", LV_SYMBOL_PLAY "  Iniciar", COR_DESTAQUE, zt_iniciar, NULL); }
static void zt_atualizar(const hi_estado_t *e)
{
    if (!zt_rodando || !job_passo(e)) return;
    zt_rodando = 0; job.ativo = 0;
    char res[400] = ""; const char *p = job.console, *q; int n = 0;
    while ((q = strstr(p, "range:")) && n < 6) { double r = atof(q + 6); char x[48]; snprintf(x, sizeof x, "tentativa %d: desnível %.4f mm\n", ++n, r); strcat(res, x); p = q + 6; }
    if (job.erro) snprintf(res, sizeof res, "Falhou: %s", job_erro_txt());
    else if (!n) snprintf(res, sizeof res, "Concluído (sem detalhes no console).");
    hi_br(res); lv_label_set_text(zt_info, res);
    if (!job.erro && n) { char r[64]; const char *u = strrchr(job.console, ':'); (void)u; snprintf(r, sizeof r, "%d tentativa(s)", n); hi_calib_registrar("Nivelamento Z", r); }
}
lv_obj_t *tela_ztilt(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Nivelamento Z", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 140);
    zt_info = hi_rotulo(c, "A Hi tem dois motores Z independentes.\nEste ajuste mede a mesa nos dois lados\ne iguala a altura dos motores.", &hi_font_14, COR_TEXTO2);
    lv_obj_set_pos(zt_info, 10, 8);
    lv_obj_t *go = hi_botao(t, LV_SYMBOL_PLAY "  Nivelar", COR_DESTAQUE, 304, 44); lv_obj_set_pos(go, 8, 186);
    lv_obj_add_event_cb(go, ev_zt, LV_EVENT_CLICKED, NULL);
    zt_rodando = 0;
    hi_set_atualizar(zt_atualizar);
    return t;
}

/* ================= Pressure Advance ================= */
static lv_obj_t *l_pa;
static void set_pa(double v, void *u)
{
    (void)u; hi_gcode("SET_PRESSURE_ADVANCE ADVANCE=%.4f", v);
    hi_fil_t f[HI_MAX_FIL]; int at; hi_fil_carregar(f, HI_MAX_FIL, &at);
    if (at >= 0) { f[at].pa = v; hi_fil_salvar(&f[at], at); }
    char r[48]; snprintf(r, sizeof r, "%.4f%s", v, at >= 0 ? " (salvo no filamento)" : ""); hi_calib_registrar("Pressure Advance", hi_br(r));
    hi_toast("PA aplicado", COR_OK);
}
static void ev_pa(lv_event_t *e) { (void)e; hi_teclado_num("Pressure Advance", hi_e.pa, 0, 1, 4, set_pa, NULL); }
static void pa_atualizar(const hi_estado_t *e) { char b[48]; snprintf(b, sizeof b, "%.4f", e->pa); lv_label_set_text(l_pa, hi_br(b)); }
lv_obj_t *tela_pa(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Pressure Advance", true);
    lv_obj_t *c = hi_cartao(t, 8, 38, 304, 70);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(c, ev_pa, LV_EVENT_CLICKED, NULL);
    lv_obj_t *n = hi_rotulo(c, "Valor atual (toque para mudar)  " LV_SYMBOL_EDIT, &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 10, 6);
    l_pa = hi_rotulo(c, "", &hi_font_28, COR_AVISO); lv_obj_set_pos(l_pa, 10, 26);
    lv_obj_t *i = hi_rotulo(t, "O valor é aplicado na hora e guardado no filamento\nem uso. O fatiador também pode definir o PA.\n\nCalibração automática pelo sensor de pressão\nda mesa (AUTO_APAX da Creality): próxima versão.", &hi_font_12, COR_TEXTO2);
    lv_obj_set_pos(i, 10, 118);
    hi_set_atualizar(pa_atualizar);
    return t;
}

/* ================= velocidade ideal ================= */
static int vl_vel, vl_acel; static double vl_canto;
static lv_obj_t *v_vl[3], *l_vl_rec;
static void vl_desenhar(void)
{
    char b[48];
    snprintf(b, sizeof b, "%d mm/s", vl_vel); lv_label_set_text(v_vl[0], b);
    snprintf(b, sizeof b, "%d mm/s\xC2\xB2", vl_acel); lv_label_set_text(v_vl[1], b);
    snprintf(b, sizeof b, "%.1f mm/s", vl_canto); lv_label_set_text(v_vl[2], hi_br(b));
}
static void set_vl(double v, void *u)
{
    int k = (int)(intptr_t)u;
    if (k == 0) vl_vel = (int)v; else if (k == 1) vl_acel = (int)v; else vl_canto = v;
    vl_desenhar();
}
static void ev_vl(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    if (k == 0) hi_teclado_num("Velocidade máxima (mm/s)", vl_vel, 50, 600, 0, set_vl, (void *)0);
    if (k == 1) hi_teclado_num("Aceleração máxima (mm/s\xC2\xB2)", vl_acel, 500, 20000, 0, set_vl, (void *)1);
    if (k == 2) hi_teclado_num("Velocidade de canto (mm/s)", vl_canto, 1, 20, 1, set_vl, (void *)2);
}
static void ev_vl_rec(lv_event_t *e)
{
    (void)e; int x = hi_pref("acel_rec_x", 0), y = hi_pref("acel_rec_y", 0);
    if (!x && !y) { hi_toast("Meça a vibração primeiro (Calibrações > Vibração)", COR_AVISO); return; }
    int m = x && y ? (x < y ? x : y) : (x ? x : y);
    vl_acel = m / 100 * 100;   /* arredonda para baixo, em centenas */
    vl_desenhar(); hi_toast("Aceleração recomendada pela vibração aplicada no campo", COR_DESTAQUE);
}
static void vl_aplicar(void *u)
{
    (void)u; char erro[200], v1[16], v2[16], v3[16], v4[16];
    snprintf(v1, sizeof v1, "%d", vl_vel); snprintf(v2, sizeof v2, "%d", vl_acel); snprintf(v3, sizeof v3, "%d", vl_acel); snprintf(v4, sizeof v4, "%.1f", vl_canto);
    const char *k[4] = {"max_velocity", "max_accel", "max_accel_to_decel", "square_corner_velocity"}, *v[4] = {v1, v2, v3, v4};
    if (!hi_cfg_autosave("printer", k, v, 4, erro, sizeof erro)) { hi_toast(erro, COR_ERRO); return; }
    if (!hi_cfg_macros_acel(vl_acel, erro, sizeof erro)) { hi_toast(erro, COR_ERRO); return; }
    char r[96]; snprintf(r, sizeof r, "vel %d mm/s  acel %d mm/s2  canto %.1f", vl_vel, vl_acel, vl_canto); hi_calib_registrar("Velocidade", hi_br(r));
    hi_gcode("SET_VELOCITY_LIMIT VELOCITY=%d ACCEL=%d ACCEL_TO_DECEL=%d SQUARE_CORNER_VELOCITY=%.1f", vl_vel, vl_acel, vl_acel, vl_canto);
    hi_gcode("FIRMWARE_RESTART");
    hi_voltar(); hi_reiniciando("velocidade gravada como padrão", LV_SYMBOL_OK "  Velocidade aplicada. Klipper pronto.");
}
static void ev_vl_aplicar(lv_event_t *e)
{
    (void)e;
    if (hi_ocupada(&hi_e)) { hi_toast("Indisponível durante a impressão", COR_AVISO); return; }
    char m[220]; snprintf(m, sizeof m, "Grava como padrão: %d mm/s, %d mm/s\xC2\xB2, canto %.1f.\nAtualiza também o teto das macros. Backup do printer.cfg e das macros. Reinicia o Klipper.", vl_vel, vl_acel, vl_canto); hi_br(m);
    hi_confirmar("Aplicar como padrão?", m, LV_SYMBOL_SAVE "  Aplicar", COR_OK, vl_aplicar, NULL);
}
lv_obj_t *tela_velocidade(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Velocidade ideal", true);
    vl_vel = (int)hi_e.max_vel; vl_acel = (int)hi_e.max_accel;
    char b[32]; vl_canto = hi_cfg_valor("printer", "square_corner_velocity", b, sizeof b) ? atof(b) : 5;
    const char *nome[3] = {"Vel. máx.", "Acel. máx.", "Canto"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = hi_cartao(t, 8 + i * 103, 38, 98, 54);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE); lv_obj_set_style_bg_color(c, COR_CARTAO2, LV_STATE_PRESSED);
        lv_obj_add_event_cb(c, ev_vl, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *n = hi_rotulo(c, nome[i], &hi_font_12, COR_TEXTO2); lv_obj_set_pos(n, 8, 4);
        v_vl[i] = hi_rotulo(c, "", &hi_font_14, COR_TEXTO); lv_obj_set_pos(v_vl[i], 8, 26);
    }
    int x = hi_pref("acel_rec_x", 0), y = hi_pref("acel_rec_y", 0);
    lv_obj_t *rc = hi_cartao(t, 8, 98, 304, 58);
    if (x || y) snprintf(b, sizeof b, "X %d  \xE2\x80\xA2  Y %d mm/s\xC2\xB2", x, y); else snprintf(b, sizeof b, "meça a vibração para obter");
    lv_obj_t *r1 = hi_rotulo(rc, "Limite seguro (pela vibração)", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(r1, 10, 4);
    l_vl_rec = hi_rotulo(rc, b, &hi_font_14, COR_OK); lv_obj_set_pos(l_vl_rec, 10, 26);
    lv_obj_t *ur = hi_botao(rc, "Usar", COR_DESTAQUE, 70, 30); lv_obj_align(ur, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_add_event_cb(ur, ev_vl_rec, LV_EVENT_CLICKED, NULL);
    lv_obj_t *i = hi_rotulo(t, "Toque num valor para editar.", &hi_font_12, COR_TEXTO2);
    lv_obj_set_pos(i, 10, 160);
    lv_obj_t *ap = hi_botao(t, LV_SYMBOL_SAVE "  Aplicar como padrão", COR_OK, 304, 40); lv_obj_set_pos(ap, 8, 194);
    lv_obj_add_event_cb(ap, ev_vl_aplicar, LV_EVENT_CLICKED, NULL);
    vl_desenhar();
    return t;
}

/* ================= histórico ================= */
lv_obj_t *tela_historico_calib(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Histórico de calibrações", true);
    lv_obj_t *l = hi_lista(t, 32, HI_H - 32);
    static char linhas[60][96]; int n = hi_calib_listar(linhas, 60);
    if (!n) hi_rotulo(l, "Nenhuma calibração feita pelo Hi Painel ainda.", &hi_font_14, COR_TEXTO2);
    for (int i = 0; i < n; i++) {
        lv_obj_t *r = hi_rotulo(l, linhas[i], &hi_font_12, COR_TEXTO);
        lv_label_set_long_mode(r, LV_LABEL_LONG_WRAP); lv_obj_set_width(r, HI_W - 16);
    }
    return t;
}

/* ================= calibrar tudo ================= */
/* Ordem definida com o usuário (13/11/2026): home primeiro (G28), depois vibração, PID do bico, PID da mesa
 * e malha por último (a mesa já está quente do PID). O nivelamento Z separado é opcional (logo após o home). */
static int ct_sel[5] = {0, 1, 1, 1, 1}, ct_rodando, ct_etapa;   /* etapa -1 = home */
static lv_obj_t *ct_btn[5], *ct_info;
static const char *ct_nome[5] = {"Nível Z", "Vibração", "PID do bico", "PID da mesa", "Nivel.+malha"};
static void ev_ct_sel(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e); ct_sel[i] = !ct_sel[i];
    lv_obj_set_style_bg_color(ct_btn[i], ct_sel[i] ? COR_DESTAQUE : COR_CARTAO2, 0);
}
static void ct_proxima(void)
{
    while (ct_etapa >= 0 && ct_etapa < 5 && !ct_sel[ct_etapa]) ct_etapa++;
    if (ct_etapa >= 5) {
        ct_rodando = 0; hi_gcode("TURN_OFF_HEATERS\nM107");
        lv_label_set_text(ct_info, "Tudo pronto. Toque em Salvar tudo para gravar\n(SAVE_CONFIG) e reiniciar o Klipper.");
        hi_calib_registrar("Calibrar tudo", "concluído"); return;
    }
    hi_fil_t f[HI_MAX_FIL]; int at; hi_fil_carregar(f, HI_MAX_FIL, &at);
    int tb = at >= 0 ? f[at].bico : 230, tm = at >= 0 ? f[at].mesa : 60;
    job_iniciar();
    switch (ct_etapa) {
        case -1: hi_gcode("G28"); break;
        case 0: hi_gcode("Z_TILT_ADJUST"); break;
        case 1: hi_gcode("SHAPER_CALIBRATE"); break;
        case 2: hi_gcode("M106 S255\nPID_CALIBRATE HEATER=extruder TARGET=%d\nM107", tb); break;
        case 3: hi_gcode("PID_CALIBRATE HEATER=heater_bed TARGET=%d", tm); break;
        case 4: hi_gcode("G29"); break;   /* limpa bico, Z, malha e grava (igual à tela original) */
    }
}
static void ct_iniciar(void *u) { (void)u; ct_rodando = 1; ct_etapa = -1; ct_proxima(); }
static void ev_ct_iniciar(lv_event_t *e) { (void)e; if (!pode_calibrar()) return; if (ct_sel[4] && !sem_filamento_ou_avisar()) return; hi_confirmar("Calibrar tudo?", "Home, depois as calibrações marcadas em sequência (20 a 40 min). Mesa livre. No fim você decide se salva.", LV_SYMBOL_PLAY "  Iniciar", COR_DESTAQUE, ct_iniciar, NULL); }
static void ev_ct_salvar(lv_event_t *e) { (void)e; if (ct_rodando) { hi_toast("Aguarde terminar", COR_AVISO); return; } hi_confirmar("Salvar tudo?", "Grava todos os resultados (SAVE_CONFIG) e reinicia o Klipper.", LV_SYMBOL_SAVE "  Salvar", COR_OK, salvar_config, "Calibrar tudo"); }
static void ct_atualizar(const hi_estado_t *e)
{
    if (!ct_rodando) return;
    char b[160]; double dt = hi_agora() - job.inicio;
    snprintf(b, sizeof b, "Etapa: %s  (%dmin %02ds)\nBico %.0f/%.0f" GRAU "   Mesa %.0f/%.0f" GRAU, ct_etapa < 0 ? "Home (G28)" : ct_nome[ct_etapa],
             (int)dt / 60, (int)dt % 60, e->bico, e->bico_alvo, e->mesa, e->mesa_alvo);
    lv_label_set_text(ct_info, b);
    if (job_passo(e)) {
        job.ativo = 0;
        if (job.erro) { ct_rodando = 0; snprintf(b, sizeof b, "Parou em %s:\n%s", ct_etapa < 0 ? "Home" : ct_nome[ct_etapa], job_erro_txt()); lv_label_set_text(ct_info, b); hi_gcode("TURN_OFF_HEATERS"); return; }
        ct_etapa++; ct_proxima();
    }
}
lv_obj_t *tela_calibrar_tudo(void)
{
    lv_obj_t *t = hi_tela_nova();
    hi_cabecalho(t, "Calibrar tudo", true);
    static const int ordem_tela[5] = {1, 2, 3, 4, 0};     /* na tela, na ordem de execução; Z (opcional) por último */
    for (int p = 0; p < 5; p++) {
        int i = ordem_tela[p];
        char rot[48];
        if (i) snprintf(rot, sizeof rot, "%d. %s", p + 1, ct_nome[i]); else snprintf(rot, sizeof rot, "%s (opcional)", ct_nome[i]);
        ct_btn[i] = hi_botao(t, rot, ct_sel[i] ? COR_DESTAQUE : COR_CARTAO2, p < 3 ? 98 : 150, 34);
        lv_obj_set_pos(ct_btn[i], p < 3 ? 8 + p * 103 : 8 + (p - 3) * 154, p < 3 ? 38 : 76);
        lv_obj_set_style_text_font(lv_obj_get_child(ct_btn[i], 0), &hi_font_12, 0);
        lv_obj_add_event_cb(ct_btn[i], ev_ct_sel, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_obj_t *c = hi_cartao(t, 8, 116, 304, 64);
    ct_info = hi_rotulo(c, "Começa com Home (G28). Azul = marcado.\nTemperaturas do filamento em uso.", &hi_font_12, COR_TEXTO2); lv_obj_set_pos(ct_info, 10, 8);
    lv_obj_t *go = hi_botao(t, LV_SYMBOL_PLAY "  Iniciar", COR_DESTAQUE, 148, 44); lv_obj_set_pos(go, 8, 188);
    lv_obj_add_event_cb(go, ev_ct_iniciar, LV_EVENT_CLICKED, NULL);
    lv_obj_t *sv = hi_botao(t, LV_SYMBOL_SAVE "  Salvar tudo", COR_OK, 148, 44); lv_obj_set_pos(sv, 164, 188);
    lv_obj_add_event_cb(sv, ev_ct_salvar, LV_EVENT_CLICKED, NULL);
    hi_set_atualizar(ct_atualizar);
    return t;
}
