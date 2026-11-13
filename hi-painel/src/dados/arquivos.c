#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#include "arquivos.h"
#include "../../lib/cjson/cJSON.h"

#define ARQ_FIL   HI_DIR "/filamentos.json"
#define ARQ_CAL   HI_DIR "/calibracoes.json"
#define ARQ_PREF  HI_DIR "/painel.json"
#define DB_CREALITY "/mnt/UDISK/creality/userdata/box/material_database.json"

const char *HI_TIPOS[] = {"PLA", "PETG", "ABS", "ASA", "TPU", "PA", "PC", "PLA-CF", "PETG-CF", "Outro", NULL};

static char *ler(const char *p)
{
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); if (fread(b, 1, n, f) != (size_t)n) n = 0; b[n] = 0; fclose(f); return b;
}
static bool gravar(const char *p, const char *conteudo)
{
    mkdir(HI_DIR, 0755);
    char tmp[256]; snprintf(tmp, sizeof tmp, "%s.tmp", p);
    FILE *f = fopen(tmp, "wb"); if (!f) return false;
    fputs(conteudo, f); fflush(f); fclose(f);
    return rename(tmp, p) == 0;
}
static cJSON *ler_json(const char *p) { char *b = ler(p); cJSON *j = b ? cJSON_Parse(b) : NULL; free(b); return j; }
static bool gravar_json(const char *p, cJSON *j) { char *s = cJSON_Print(j); bool ok = s && gravar(p, s); free(s); return ok; }
static double num(cJSON *o, const char *k, double d) { cJSON *v = cJSON_GetObjectItem(o, k); return cJSON_IsNumber(v) ? v->valuedouble : cJSON_IsString(v) ? atof(v->valuestring) : d; }

/* ---------------- filamentos ---------------- */
void hi_fil_padrao(const char *tipo, hi_fil_t *f)
{
    struct { const char *t; int b, m, vmin, vmax; double pa, fl, vol, ret; } p[] = {
        {"PLA", 215, 60, 60, 100, 0.040, 0.98, 18, 0.8}, {"PETG", 235, 75, 20, 40, 0.060, 0.95, 11, 1.2},
        {"ABS", 250, 100, 0, 30, 0.040, 0.95, 16, 0.8}, {"ASA", 255, 100, 0, 30, 0.040, 0.95, 16, 0.8},
        {"TPU", 225, 50, 30, 60, 0.000, 1.00, 4, 0.0}, {"PA", 265, 90, 0, 20, 0.030, 0.96, 10, 1.0},
        {"PC", 270, 110, 0, 20, 0.030, 0.95, 10, 0.8}, {"PLA-CF", 220, 60, 50, 90, 0.030, 0.96, 12, 0.8},
        {"PETG-CF", 240, 75, 20, 40, 0.050, 0.95, 9, 1.0}, {"Outro", 220, 60, 30, 70, 0.040, 1.00, 10, 0.8}};
    int i = 0; while (i < 9 && strcasecmp(p[i].t, tipo)) i++;
    snprintf(f->tipo, sizeof f->tipo, "%s", p[i].t);
    f->bico = p[i].b; f->bico_1a = p[i].b + 5; f->mesa = p[i].m; f->vent_min = p[i].vmin; f->vent_max = p[i].vmax;
    f->pa = p[i].pa; f->fluxo = p[i].fl; f->vol_max = p[i].vol; f->retracao = p[i].ret; f->zhop = 0.2;
}

static void fil_de_json(cJSON *o, hi_fil_t *f)
{
    memset(f, 0, sizeof *f);
    cJSON *v;
    snprintf(f->nome, sizeof f->nome, "%s", cJSON_IsString(v = cJSON_GetObjectItem(o, "nome")) ? v->valuestring : "?");
    snprintf(f->tipo, sizeof f->tipo, "%s", cJSON_IsString(v = cJSON_GetObjectItem(o, "tipo")) ? v->valuestring : "Outro");
    snprintf(f->origem, sizeof f->origem, "Hi");
    f->bico = num(o, "bico", 220); f->bico_1a = num(o, "bico_1a", f->bico); f->mesa = num(o, "mesa", 60);
    f->vent_min = num(o, "vent_min", 30); f->vent_max = num(o, "vent_max", 70);
    f->pa = num(o, "pa", 0.04); f->fluxo = num(o, "fluxo", 1); f->vol_max = num(o, "vol_max", 10);
    f->retracao = num(o, "retracao", 0.8); f->zhop = num(o, "zhop", 0.2);
}
static cJSON *fil_para_json(const hi_fil_t *f)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "nome", f->nome); cJSON_AddStringToObject(o, "tipo", f->tipo);
    cJSON_AddNumberToObject(o, "bico", f->bico); cJSON_AddNumberToObject(o, "bico_1a", f->bico_1a); cJSON_AddNumberToObject(o, "mesa", f->mesa);
    cJSON_AddNumberToObject(o, "vent_min", f->vent_min); cJSON_AddNumberToObject(o, "vent_max", f->vent_max);
    cJSON_AddNumberToObject(o, "pa", f->pa); cJSON_AddNumberToObject(o, "fluxo", f->fluxo); cJSON_AddNumberToObject(o, "vol_max", f->vol_max);
    cJSON_AddNumberToObject(o, "retracao", f->retracao); cJSON_AddNumberToObject(o, "zhop", f->zhop);
    return o;
}

static cJSON *banco(void)
{
    cJSON *j = ler_json(ARQ_FIL);
    if (j) return j;
    /* primeira vez: cria com o filamento do dono da Hi de referência como exemplo */
    j = cJSON_CreateObject();
    cJSON *l = cJSON_AddArrayToObject(j, "filamentos");
    hi_fil_t f; hi_fil_padrao("PETG", &f); snprintf(f.nome, sizeof f.nome, "PETG MasterPrint");
    f.bico = 230; f.bico_1a = 235; f.mesa = 75; f.vent_min = 20; f.vent_max = 40; f.pa = 0.072; f.fluxo = 0.94; f.vol_max = 11; f.retracao = 1.5;
    cJSON_AddItemToArray(l, fil_para_json(&f));
    cJSON_AddStringToObject(j, "ativo", f.nome);
    gravar_json(ARQ_FIL, j);
    return j;
}

int hi_fil_carregar(hi_fil_t *dst, int max, int *ativo)
{
    int n = 0; if (ativo) *ativo = -1;
    cJSON *j = banco();
    cJSON *at = cJSON_GetObjectItem(j, "ativo"), *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItem(j, "filamentos")) {
        if (n >= max) break;
        fil_de_json(it, &dst[n]);
        if (ativo && cJSON_IsString(at) && !strcmp(at->valuestring, dst[n].nome)) *ativo = n;
        n++;
    }
    cJSON_Delete(j);
    /* banco da Creality (só leitura): nome, tipo, temperaturas */
    cJSON *c = ler_json(DB_CREALITY);
    cJSON *lista = cJSON_GetObjectItem(cJSON_GetObjectItem(c, "result"), "list");
    cJSON_ArrayForEach(it, lista) {
        if (n >= max) break;
        cJSON *base = cJSON_GetObjectItem(it, "base"), *kv = cJSON_GetObjectItem(it, "kvParam");
        hi_fil_t *f = &dst[n]; memset(f, 0, sizeof *f);
        cJSON *nm = cJSON_GetObjectItem(base, "name"), *tp = cJSON_GetObjectItem(base, "meterialType");
        if (!cJSON_IsString(nm)) continue;
        snprintf(f->nome, sizeof f->nome, "%s", nm->valuestring);
        snprintf(f->tipo, sizeof f->tipo, "%s", cJSON_IsString(tp) ? tp->valuestring : "?");
        snprintf(f->origem, sizeof f->origem, "Creality");
        f->bico = num(kv, "nozzle_temperature", num(base, "maxTemp", 220));
        f->bico_1a = num(kv, "nozzle_temperature_initial_layer", f->bico);
        f->mesa = num(kv, "hot_plate_temp", num(kv, "textured_plate_temp", 60));
        f->vent_min = num(kv, "fan_min_speed", 30); f->vent_max = num(kv, "fan_max_speed", 70);
        f->pa = num(kv, "pressure_advance", 0.04); f->fluxo = num(kv, "filament_flow_ratio", 1);
        f->vol_max = num(kv, "filament_max_volumetric_speed", 10);
        f->retracao = num(kv, "filament_retraction_length", 0.8); f->zhop = num(kv, "filament_z_hop", 0.2);
        n++;
    }
    cJSON_Delete(c);
    return n;
}

bool hi_fil_salvar(const hi_fil_t *f, int indice_nosso)
{
    cJSON *j = banco(); cJSON *l = cJSON_GetObjectItem(j, "filamentos");
    if (indice_nosso >= 0 && indice_nosso < cJSON_GetArraySize(l)) cJSON_ReplaceItemInArray(l, indice_nosso, fil_para_json(f));
    else cJSON_AddItemToArray(l, fil_para_json(f));
    bool ok = gravar_json(ARQ_FIL, j); cJSON_Delete(j); return ok;
}
bool hi_fil_apagar(const char *nome)
{
    cJSON *j = banco(); cJSON *l = cJSON_GetObjectItem(j, "filamentos");
    for (int i = 0; i < cJSON_GetArraySize(l); i++) {
        cJSON *n = cJSON_GetObjectItem(cJSON_GetArrayItem(l, i), "nome");
        if (cJSON_IsString(n) && !strcmp(n->valuestring, nome)) { cJSON_DeleteItemFromArray(l, i); break; }
    }
    bool ok = gravar_json(ARQ_FIL, j); cJSON_Delete(j); return ok;
}
bool hi_fil_ativar(const char *nome)
{
    cJSON *j = banco(); cJSON_DeleteItemFromObject(j, "ativo"); cJSON_AddStringToObject(j, "ativo", nome);
    bool ok = gravar_json(ARQ_FIL, j); cJSON_Delete(j); return ok;
}

/* ---------------- histórico de calibrações ---------------- */
void hi_calib_registrar(const char *tipo, const char *resumo)
{
    cJSON *j = ler_json(ARQ_CAL); if (!j) { j = cJSON_CreateObject(); cJSON_AddArrayToObject(j, "itens"); }
    cJSON *o = cJSON_CreateObject();
    char d[32]; time_t t = time(NULL); strftime(d, sizeof d, "%d/%m/%Y %H:%M", localtime(&t));
    cJSON_AddStringToObject(o, "data", d); cJSON_AddStringToObject(o, "tipo", tipo); cJSON_AddStringToObject(o, "resumo", resumo);
    cJSON *l = cJSON_GetObjectItem(j, "itens"); cJSON_InsertItemInArray(l, 0, o);
    while (cJSON_GetArraySize(l) > 100) cJSON_DeleteItemFromArray(l, 100);
    gravar_json(ARQ_CAL, j); cJSON_Delete(j);
}
int hi_calib_listar(char linhas[][96], int max)
{
    cJSON *j = ler_json(ARQ_CAL); int n = 0; cJSON *it;
    cJSON_ArrayForEach(it, cJSON_GetObjectItem(j, "itens")) {
        if (n >= max) break;
        snprintf(linhas[n++], 96, "%s  %s: %s", cJSON_GetObjectItem(it, "data")->valuestring,
                 cJSON_GetObjectItem(it, "tipo")->valuestring, cJSON_GetObjectItem(it, "resumo")->valuestring);
    }
    cJSON_Delete(j); return n;
}
bool hi_calib_ultima(const char *tipo, char *dst, int tam)
{
    cJSON *j = ler_json(ARQ_CAL); cJSON *it; bool ok = false;
    cJSON_ArrayForEach(it, cJSON_GetObjectItem(j, "itens")) {
        if (!strcmp(cJSON_GetObjectItem(it, "tipo")->valuestring, tipo)) {
            snprintf(dst, tam, "%s", cJSON_GetObjectItem(it, "data")->valuestring); ok = true; break;
        }
    }
    cJSON_Delete(j); return ok;
}

/* ---------------- preferências ---------------- */
int hi_pref(const char *chave, int padrao)
{
    cJSON *j = ler_json(ARQ_PREF); int v = (int)num(j, chave, padrao); cJSON_Delete(j); return v;
}
void hi_pref_set(const char *chave, int valor)
{
    cJSON *j = ler_json(ARQ_PREF); if (!j) j = cJSON_CreateObject();
    cJSON_DeleteItemFromObject(j, chave); cJSON_AddNumberToObject(j, chave, valor);
    gravar_json(ARQ_PREF, j); cJSON_Delete(j);
}

/* ---------------- printer.cfg / SAVE_CONFIG ---------------- */
bool hi_cfg_ler_pid(const char *secao, double pid[3])
{
    char *b = ler(HI_PRINTERCFG); if (!b) return false;
    const char *ch[3] = {"pid_kp", "pid_ki", "pid_kd"}; bool achou = false;
    pid[0] = pid[1] = pid[2] = 0;
    char cab[64]; snprintf(cab, sizeof cab, "[%s]", secao);
    /* valores do autosave têm prioridade: lemos o corpo e depois o bloco #*# */
    for (int fase = 0; fase < 2; fase++) {
        char *p = b, *linha; int dentro = 0;
        while ((linha = strsep(&p, "\n"))) {
            char *l = linha;
            if (fase == 1) { if (strncmp(l, "#*# ", 4)) continue; l += 4; }
            else if (!strncmp(l, "#*#", 3)) break;
            while (*l == ' ' || *l == '\t') l++;
            if (*l == '[') { dentro = !strncasecmp(l, cab, strlen(cab)); continue; }
            if (!dentro || *l == '#' || *l == ';') continue;
            for (int i = 0; i < 3; i++)
                if (!strncasecmp(l, ch[i], 6)) { char *v = strpbrk(l, ":="); if (v) { pid[i] = atof(v + 1); achou = true; } }
        }
        free(b); b = ler(HI_PRINTERCFG);
    }
    free(b);
    return achou;
}

bool hi_cfg_autosave(const char *secao, const char *chaves[], const char *valores[], int n, char *erro, int tam_erro)
{
    char *b = ler(HI_PRINTERCFG);
    if (!b) { snprintf(erro, tam_erro, "não consegui ler o printer.cfg"); return false; }
    char bak[256]; time_t t = time(NULL); char d[32]; strftime(d, sizeof d, "%Y%m%d-%H%M%S", localtime(&t));
    snprintf(bak, sizeof bak, "%s.hi-painel-%s", HI_PRINTERCFG, d);
    if (!gravar(bak, b)) { free(b); snprintf(erro, tam_erro, "não consegui criar o backup"); return false; }

    size_t cap = strlen(b) + 4096; char *o = malloc(cap); o[0] = 0; size_t on = 0;
#define ADD(s) do { size_t _l = strlen(s); if (on + _l + 2 >= cap) { cap = (on + _l) * 2; o = realloc(o, cap); } memcpy(o + on, s, _l); on += _l; o[on] = 0; } while (0)
    char cab[64]; snprintf(cab, sizeof cab, "[%s]", secao);
    char *p = b, *linha; int dentro = 0, no_auto = 0, sec_auto = 0, escrito = 0;
    bool tem_auto = strstr(b, "#*# <---------------------- SAVE_CONFIG") != NULL;
    while ((linha = strsep(&p, "\n"))) {
        if (!p && !*linha) break;
        char *l = linha;
        if (!strncmp(l, "#*# <---------------------- SAVE_CONFIG", 39)) no_auto = 1;
        if (!no_auto) {
            char *x = l; while (*x == ' ' || *x == '\t') x++;
            if (*x == '[') dentro = !strncasecmp(x, cab, strlen(cab));
            int comentar = 0;
            if (dentro) for (int i = 0; i < n; i++) if (!strncasecmp(x, chaves[i], strlen(chaves[i])) && strchr(":= ", x[strlen(chaves[i])])) comentar = 1;
            if (comentar) ADD("#hi-painel: ");
            ADD(l); ADD("\n");
            continue;
        }
        /* dentro do bloco autosave */
        if (!strncmp(l, "#*# [", 5)) {
            if (sec_auto && !escrito) { for (int i = 0; i < n; i++) { char s[160]; snprintf(s, sizeof s, "#*# %s = %s\n", chaves[i], valores[i]); ADD(s); } escrito = 1; }
            sec_auto = !strncasecmp(l + 4, cab, strlen(cab));
            ADD(l); ADD("\n"); continue;
        }
        if (sec_auto) {
            int pular = 0;
            for (int i = 0; i < n; i++) if (!strncasecmp(l + 4, chaves[i], strlen(chaves[i]))) pular = 1;
            if (pular) continue;
            if (!strcmp(l, "#*#") && !escrito) { for (int i = 0; i < n; i++) { char s[160]; snprintf(s, sizeof s, "#*# %s = %s\n", chaves[i], valores[i]); ADD(s); } escrito = 1; }
        }
        ADD(l); ADD("\n");
    }
    if (sec_auto && !escrito) { for (int i = 0; i < n; i++) { char s[160]; snprintf(s, sizeof s, "#*# %s = %s\n", chaves[i], valores[i]); ADD(s); } escrito = 1; }
    if (!escrito) {
        if (!tem_auto) ADD("\n#*# <---------------------- SAVE_CONFIG ---------------------->\n#*# DO NOT EDIT THIS BLOCK OR BELOW. The contents are auto-generated.\n#*#\n");
        char s[160]; snprintf(s, sizeof s, "#*# %s\n", cab); ADD(s);
        for (int i = 0; i < n; i++) { snprintf(s, sizeof s, "#*# %s = %s\n", chaves[i], valores[i]); ADD(s); }
        ADD("#*#\n");
    }
#undef ADD
    bool ok = gravar(HI_PRINTERCFG, o);
    free(o); free(b);
    if (!ok) snprintf(erro, tam_erro, "não consegui gravar o printer.cfg (backup em %s)", bak);
    return ok;
}

bool hi_cfg_valor(const char *secao, const char *chave, char *dst, int tam)
{
    char *b = ler(HI_PRINTERCFG); if (!b) return false;
    char cab[64]; snprintf(cab, sizeof cab, "[%s]", secao); bool achou = false; size_t lc = strlen(chave);
    for (int fase = 0; fase < 2; fase++) {
        char *copia = strdup(b), *p = copia, *linha; int dentro = 0;
        while ((linha = strsep(&p, "\n"))) {
            char *l = linha;
            if (fase == 1) { if (strncmp(l, "#*# ", 4)) continue; l += 4; }
            else if (!strncmp(l, "#*#", 3)) break;
            while (*l == ' ' || *l == '\t') l++;
            if (*l == '[') { dentro = !strncasecmp(l, cab, strlen(cab)); continue; }
            if (!dentro || *l == '#' || *l == ';') continue;
            if (!strncasecmp(l, chave, lc) && strchr(":= \t", l[lc])) {
                char *v = strpbrk(l, ":="); if (!v) continue; v++; while (*v == ' ') v++;
                snprintf(dst, tam, "%s", v); dst[strcspn(dst, "#;\r")] = 0; achou = true;
            }
        }
        free(copia);
    }
    free(b); return achou;
}

#define HI_MACROS "/mnt/UDISK/printer_data/config/gcode_macro.cfg"
static char *trocar_tudo(const char *txt, const char *de, const char *para)
{
    size_t ld = strlen(de), lp = strlen(para), n = 0;
    for (const char *p = txt; (p = strstr(p, de)); p += ld) n++;
    char *o = malloc(strlen(txt) + n * (lp > ld ? lp - ld : 0) + 1), *w = o; const char *p = txt, *q;
    while ((q = strstr(p, de))) { memcpy(w, p, q - p); w += q - p; memcpy(w, para, lp); w += lp; p = q + ld; }
    strcpy(w, p); return o;
}
bool hi_cfg_macros_acel(int nova, char *erro, int tam_erro)
{
    char *b = ler(HI_MACROS);
    if (!b) { snprintf(erro, tam_erro, "não consegui ler o gcode_macro.cfg"); return false; }
    char *p = strstr(b, "variable_default_acc:");
    int antiga = p ? atoi(p + 21) : 0;
    if (antiga <= 0) { free(b); snprintf(erro, tam_erro, "teto de aceleração não encontrado nas macros"); return true; }  /* nada a mudar */
    if (antiga == nova) { free(b); return true; }
    char bak[256], d[32]; time_t t = time(NULL); strftime(d, sizeof d, "%Y%m%d-%H%M%S", localtime(&t));
    snprintf(bak, sizeof bak, "%s.hi-painel-%s", HI_MACROS, d);
    if (!gravar(bak, b)) { free(b); snprintf(erro, tam_erro, "não consegui criar o backup das macros"); return false; }
    char de[64], para[64]; char *x = b, *y;
    snprintf(de, sizeof de, "variable_default_acc: %d", antiga); snprintf(para, sizeof para, "variable_default_acc: %d", nova);
    y = trocar_tudo(x, de, para); x = y;
    snprintf(de, sizeof de, "ACCEL=%d ACCEL_TO_DECEL=%d", antiga, antiga); snprintf(para, sizeof para, "ACCEL=%d ACCEL_TO_DECEL=%d", nova, nova);
    y = trocar_tudo(x, de, para); free(x); x = y;
    snprintf(de, sizeof de, "|float, %d]", antiga); snprintf(para, sizeof para, "|float, %d]", nova);
    y = trocar_tudo(x, de, para); free(x); x = y;
    bool ok = gravar(HI_MACROS, x);
    free(x); free(b);
    if (!ok) snprintf(erro, tam_erro, "não consegui gravar as macros (backup em %s)", bak);
    return ok;
}

int hi_cfg_ventoinha_max(void)
{
    char *b = ler(HI_MACROS); if (!b) return -1;
    char *p = strstr(b, "variable_fan0_max_pct:"); int v = p ? atoi(p + 22) : -1;
    free(b); return v;
}
bool hi_cfg_ventoinha_max_gravar(int pct, char *erro, int tam_erro)
{
    char *b = ler(HI_MACROS);
    if (!b) { snprintf(erro, tam_erro, "não consegui ler o gcode_macro.cfg"); return false; }
    char *p = strstr(b, "variable_fan0_max_pct:");
    if (!p) { free(b); snprintf(erro, tam_erro, "as macros não têm a velocidade máxima (atualize o pacote)"); return false; }
    char bak[256], d[32]; time_t t = time(NULL); strftime(d, sizeof d, "%Y%m%d-%H%M%S", localtime(&t));
    snprintf(bak, sizeof bak, "%s.hi-painel-%s", HI_MACROS, d);
    if (!gravar(bak, b)) { free(b); snprintf(erro, tam_erro, "não consegui criar o backup"); return false; }
    char *fim = p + 22; while (*fim == ' ') fim++; while (*fim >= '0' && *fim <= '9') fim++;
    size_t n = strlen(b) + 32; char *o = malloc(n);
    snprintf(o, n, "%.*svariable_fan0_max_pct: %d%s", (int)(p - b), b, pct, fim);
    bool ok = gravar(HI_MACROS, o); free(o); free(b);
    if (!ok) snprintf(erro, tam_erro, "não consegui gravar as macros (backup em %s)", bak);
    return ok;
}
