#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "moonraker.h"
#include "../../lib/cjson/cJSON.h"

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static hi_estado_t est;
static const char *HOST = "127.0.0.1";
static int PORTA = 7125;

double hi_agora(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec + t.tv_usec / 1e6; }

/* ---------------- HTTP mínimo ---------------- */
typedef struct { char *d; size_t n, cap; } buf_t;
static void buf_add(buf_t *b, const char *s, size_t n)
{
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2; b->d = realloc(b->d, b->cap); }
    memcpy(b->d + b->n, s, n); b->n += n; b->d[b->n] = 0;
}

/* devolve o corpo (malloc) ou NULL; *status recebe o código HTTP */
static char *http(const char *metodo, const char *caminho, const char *corpo, int *status, int timeout_s)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return NULL;
    struct timeval tv = {timeout_s, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(PORTA); inet_pton(AF_INET, HOST, &a.sin_addr);
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return NULL; }
    char cab[1024];
    int cl = corpo ? (int)strlen(corpo) : 0;
    int n = snprintf(cab, sizeof cab, "%s %s HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n%sContent-Length: %d\r\n\r\n",
                     metodo, caminho, corpo ? "Content-Type: application/x-www-form-urlencoded\r\n" : "", cl);
    if (send(s, cab, n, 0) != n || (corpo && send(s, corpo, cl, 0) != cl)) { close(s); return NULL; }
    buf_t b = {0};
    char tmp[4096]; ssize_t r;
    while ((r = recv(s, tmp, sizeof tmp, 0)) > 0) buf_add(&b, tmp, r);
    close(s);
    if (!b.d) return NULL;
    if (status) *status = atoi(b.d + 9);
    char *p = strstr(b.d, "\r\n\r\n");
    char *res = p ? strdup(p + 4) : NULL;
    free(b.d);
    return res;
}

static void urlcod(const char *s, char *o, size_t max)
{
    static const char *hex = "0123456789ABCDEF";
    size_t j = 0;
    for (; *s && j + 4 < max; s++) {
        unsigned char c = *s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("-_.~", c)) o[j++] = c;
        else { o[j++] = '%'; o[j++] = hex[c >> 4]; o[j++] = hex[c & 15]; }
    }
    o[j] = 0;
}

/* ---------------- fila de comandos ---------------- */
#define FILA 32
static struct { char metodo[8]; char caminho[512]; char corpo[1024]; } fila[FILA];
static int f_ini, f_fim;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static char erro_cmd[256]; static unsigned erro_cont;

static void enfileirar(const char *metodo, const char *caminho, const char *corpo)
{
    pthread_mutex_lock(&mx);
    int prox = (f_fim + 1) % FILA;
    if (prox != f_ini) {
        snprintf(fila[f_fim].metodo, 8, "%s", metodo);
        snprintf(fila[f_fim].caminho, 512, "%s", caminho);
        snprintf(fila[f_fim].corpo, 1024, "%s", corpo ? corpo : "");
        f_fim = prox;
    }
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mx);
}

void hi_gcode(const char *fmt, ...)
{
    char g[700], c[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(g, sizeof g, fmt, ap); va_end(ap);
    urlcod(g, c + 7, sizeof c - 7); memcpy(c, "script=", 7);
    enfileirar("POST", "/printer/gcode/script", c);
}
void hi_req(const char *metodo, const char *caminho) { enfileirar(metodo, caminho, NULL); }

unsigned hi_erro_cmd(char *dst, int tam)
{
    pthread_mutex_lock(&mx); snprintf(dst, tam, "%s", erro_cmd); unsigned c = erro_cont; pthread_mutex_unlock(&mx);
    return c;
}

/* thread de comandos: separada do polling, porque um G-code longo (PID, shaper) bloqueia minutos */
static void *thread_cmd(void *x)
{
    (void)x;
    for (;;) {
        pthread_mutex_lock(&mx);
        while (f_ini == f_fim) pthread_cond_wait(&cond, &mx);
        char metodo[8], caminho[512], corpo[1024];
        strcpy(metodo, fila[f_ini].metodo); strcpy(caminho, fila[f_ini].caminho); strcpy(corpo, fila[f_ini].corpo);
        f_ini = (f_ini + 1) % FILA;
        pthread_mutex_unlock(&mx);
        int st = 0;
        char *r = http(metodo, caminho, corpo[0] ? corpo : NULL, &st, 3600);
        if (st != 200 && r) {
            cJSON *j = cJSON_Parse(r);
            cJSON *m = j ? cJSON_GetObjectItem(cJSON_GetObjectItem(j, "error"), "message") : NULL;
            pthread_mutex_lock(&mx);
            snprintf(erro_cmd, sizeof erro_cmd, "%s", cJSON_IsString(m) ? m->valuestring : "erro no comando");
            erro_cont++;
            pthread_mutex_unlock(&mx);
            cJSON_Delete(j);
        }
        free(r);
    }
    return NULL;
}

/* ---------------- polling do estado ---------------- */
static double num(cJSON *o, const char *k, double padrao)
{
    cJSON *v = cJSON_GetObjectItem(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : padrao;
}
static void str(cJSON *o, const char *k, char *dst, size_t tam)
{
    cJSON *v = cJSON_GetObjectItem(o, k);
    if (cJSON_IsString(v)) snprintf(dst, tam, "%s", v->valuestring);
}

static void ler_rede(hi_estado_t *e)
{
    FILE *p = popen("iw dev wlan0 link 2>/dev/null; ip -4 -o addr show 2>/dev/null", "r");
    if (!p) return;
    char l[256]; e->ssid[0] = 0; e->ip[0] = 0; e->sinal_dbm = 0; e->freq_mhz = 0;
    while (fgets(l, sizeof l, p)) {
        char *q;
        if ((q = strstr(l, "SSID: "))) { snprintf(e->ssid, sizeof e->ssid, "%s", q + 6); e->ssid[strcspn(e->ssid, "\n")] = 0; }
        else if ((q = strstr(l, "freq: "))) e->freq_mhz = atoi(q + 6);
        else if ((q = strstr(l, "signal: "))) e->sinal_dbm = atoi(q + 8);
        else if (strstr(l, "inet ") && !strstr(l, " lo ") && !e->ip[0]) {
            q = strstr(l, "inet ") + 5; snprintf(e->ip, sizeof e->ip, "%s", q); e->ip[strcspn(e->ip, "/ \n")] = 0;
        }
    }
    pclose(p);
    if (e->ssid[0]) snprintf(e->rede, sizeof e->rede, "%s GHz  %d dBm", e->freq_mhz > 4000 ? "5" : "2,4", e->sinal_dbm);
    else if (e->ip[0]) snprintf(e->rede, sizeof e->rede, "cabo");
    else snprintf(e->rede, sizeof e->rede, "sem rede");
}

static const char *CONSULTA =
    "/printer/objects/query?webhooks&print_stats&extruder&heater_bed&toolhead&gcode_move&virtual_sdcard"
    "&display_status&output_pin%20LED&output_pin%20fan0&system_stats"
    "&temperature_sensor%20mcu_temp&idle_timeout&filament_switch_sensor%20filament_sensor&configfile=save_config_pending";
/* Ajuste 13/11 (desempenho): a matriz da malha (49-81 números) só a cada 10 ciclos; com a tela da Creality na frente
 * ou em descanso, consulta a cada 4 s em vez de 0,7 s (menos CPU no Klipper/Moonraker e menos log). */
static const char *CONSULTA_MALHA = "&bed_mesh=profile_name,probed_matrix";
volatile int hi_poll_lento;

static void *thread_poll(void *x)
{
    (void)x;
    int ciclo = 0;
    for (;;) {
        hi_estado_t e;
        pthread_mutex_lock(&mx); e = est; pthread_mutex_unlock(&mx);
        int st = 0;
        int com_malha = !hi_poll_lento && (ciclo % 10 == 0);
        char q[640]; snprintf(q, sizeof q, "%s%s", CONSULTA, com_malha ? CONSULTA_MALHA : "");
        char *r = http("GET", q, NULL, &st, 3);
        e.conectado = r && st == 200;
        if (!r) snprintf(e.klippy, sizeof e.klippy, "offline");
        else if (st != 200) snprintf(e.klippy, sizeof e.klippy, "startup");
        if (r && st == 200) {
            cJSON *j = cJSON_Parse(r);
            cJSON *s = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "result"), "status");
            cJSON *o;
            if ((o = cJSON_GetObjectItem(s, "webhooks"))) { str(o, "state", e.klippy, sizeof e.klippy); str(o, "state_message", e.klippy_msg, sizeof e.klippy_msg); }
            if ((o = cJSON_GetObjectItem(s, "extruder"))) { e.bico = num(o, "temperature", 0); e.bico_alvo = num(o, "target", 0); e.bico_pot = num(o, "power", 0); e.pa = num(o, "pressure_advance", 0); }
            if ((o = cJSON_GetObjectItem(s, "heater_bed"))) { e.mesa = num(o, "temperature", 0); e.mesa_alvo = num(o, "target", 0); e.mesa_pot = num(o, "power", 0); }
            if ((o = cJSON_GetObjectItem(s, "temperature_sensor mcu_temp"))) e.mcu = num(o, "temperature", 0);
            if ((o = cJSON_GetObjectItem(s, "print_stats"))) {
                str(o, "state", e.impr_estado, sizeof e.impr_estado); str(o, "filename", e.impr_arquivo, sizeof e.impr_arquivo);
                str(o, "message", e.impr_msg, sizeof e.impr_msg);
                e.impr_duracao = num(o, "print_duration", 0); e.total_duracao = num(o, "total_duration", 0);
                e.filamento_usado = num(o, "filament_used", 0); e.power_loss = (int)num(o, "power_loss", 0);
                cJSON *inf = cJSON_GetObjectItem(o, "info");
                e.camada = (int)num(inf, "current_layer", 0); e.camadas = (int)num(inf, "total_layer", 0);
            }
            if ((o = cJSON_GetObjectItem(s, "virtual_sdcard"))) {
                e.progresso = num(o, "progress", 0);
                if (!e.camadas) { e.camada = (int)num(o, "layer", 0); e.camadas = (int)num(o, "layer_count", 0); }
            }
            if ((o = cJSON_GetObjectItem(s, "display_status")) && num(o, "progress", 0) > 0) e.progresso = num(o, "progress", 0);
            if ((o = cJSON_GetObjectItem(s, "toolhead"))) {
                str(o, "homed_axes", e.homed, sizeof e.homed);
                cJSON *p = cJSON_GetObjectItem(o, "position");
                for (int i = 0; i < 4 && cJSON_GetArrayItem(p, i); i++) e.pos[i] = cJSON_GetArrayItem(p, i)->valuedouble;
                e.max_accel = num(o, "max_accel", 0); e.max_vel = num(o, "max_velocity", 0);
            }
            if ((o = cJSON_GetObjectItem(s, "gcode_move"))) {
                e.vel_fator = num(o, "speed_factor", 1); e.fluxo_fator = num(o, "extrude_factor", 1); e.vel_atual = num(o, "speed", 0) / 60.0;
                cJSON *h = cJSON_GetObjectItem(o, "homing_origin");
                if (cJSON_GetArrayItem(h, 2)) e.z_offset = cJSON_GetArrayItem(h, 2)->valuedouble;
            }
            if ((o = cJSON_GetObjectItem(s, "output_pin LED"))) e.led = num(o, "value", 0);
            if ((o = cJSON_GetObjectItem(s, "output_pin fan0"))) e.ventoinha = num(o, "value", 0);
            if ((o = cJSON_GetObjectItem(s, "filament_switch_sensor filament_sensor"))) e.filamento_no_bico = cJSON_IsTrue(cJSON_GetObjectItem(o, "filament_detected"));
            if ((o = cJSON_GetObjectItem(s, "configfile"))) e.config_pendente = cJSON_IsTrue(cJSON_GetObjectItem(o, "save_config_pending"));
            if ((o = cJSON_GetObjectItem(s, "idle_timeout"))) str(o, "state", e.ocioso, sizeof e.ocioso);
            if ((o = cJSON_GetObjectItem(s, "system_stats"))) { e.carga = num(o, "sysload", 0); e.mem_livre_kb = num(o, "memavail", 0); }
            if ((o = cJSON_GetObjectItem(s, "bed_mesh"))) {
                str(o, "profile_name", e.malha_perfil, sizeof e.malha_perfil);
                cJSON *m = cJSON_GetObjectItem(o, "probed_matrix");
                if (m) {
                int nl = cJSON_GetArraySize(m); if (nl > HI_MAX_MALHA) nl = HI_MAX_MALHA;
                e.malha_lin = nl; e.malha_col = 0; e.malha_min = 1e9; e.malha_max = -1e9;
                for (int i = 0; i < nl; i++) {
                    cJSON *lin = cJSON_GetArrayItem(m, i);
                    int nc = cJSON_GetArraySize(lin); if (nc > HI_MAX_MALHA) nc = HI_MAX_MALHA;
                    e.malha_col = nc;
                    for (int k = 0; k < nc; k++) {
                        double v = cJSON_GetArrayItem(lin, k)->valuedouble; e.malha[i][k] = v;
                        if (v < e.malha_min) e.malha_min = v; if (v > e.malha_max) e.malha_max = v;
                    }
                }
                if (!nl) e.malha_min = e.malha_max = 0;
                }
            }
            cJSON_Delete(j);
        }
        free(r);
        if (ciclo % 10 == 0) ler_rede(&e);
        e.versao++;
        pthread_mutex_lock(&mx); est = e; pthread_mutex_unlock(&mx);
        ciclo++;
        if (hi_poll_lento) { for (int k = 0; k < 20 && hi_poll_lento; k++) usleep(200000); }   /* até 4 s, acorda rápido ao abrir */
        else usleep(700000);
    }
    return NULL;
}

void hi_estado_copiar(hi_estado_t *dst) { pthread_mutex_lock(&mx); *dst = est; pthread_mutex_unlock(&mx); }

/* ---------------- listas sob demanda ---------------- */
static hi_arquivo_t arqs[HI_MAX_ARQ]; static int n_arqs; static unsigned v_arqs; static volatile int pedir_arq;
static hi_historico_t hist[HI_MAX_HIST]; static int n_hist; static unsigned v_hist; static volatile int pedir_hist;

void hi_pedir_arquivos(void) { pedir_arq = 1; }
void hi_pedir_historico(void) { pedir_hist = 1; }

int hi_arquivos(hi_arquivo_t *dst, int max, unsigned *versao)
{
    pthread_mutex_lock(&mx); int n = n_arqs < max ? n_arqs : max; memcpy(dst, arqs, n * sizeof *dst); if (versao) *versao = v_arqs; pthread_mutex_unlock(&mx);
    return n;
}
int hi_historico(hi_historico_t *dst, int max, unsigned *versao)
{
    pthread_mutex_lock(&mx); int n = n_hist < max ? n_hist : max; memcpy(dst, hist, n * sizeof *dst); if (versao) *versao = v_hist; pthread_mutex_unlock(&mx);
    return n;
}

static int cmp_arq(const void *a, const void *b) { double d = ((hi_arquivo_t *)b)->modificado - ((hi_arquivo_t *)a)->modificado; return d > 0 ? 1 : d < 0 ? -1 : 0; }

static void buscar_arquivos(void)
{
    int st = 0; char *r = http("GET", "/server/files/list?root=gcodes", NULL, &st, 10);
    if (!r) return;
    cJSON *j = cJSON_Parse(r); free(r);
    cJSON *l = cJSON_GetObjectItem(j, "result");
    static hi_arquivo_t tmp[HI_MAX_ARQ]; int n = 0;
    cJSON *it;
    cJSON_ArrayForEach(it, l) {
        if (n >= HI_MAX_ARQ) break;
        cJSON *p = cJSON_GetObjectItem(it, "path");
        if (!cJSON_IsString(p)) continue;
        const char *ext = strrchr(p->valuestring, '.');
        if (!ext || strcasecmp(ext, ".gcode")) continue;
        memset(&tmp[n], 0, sizeof tmp[n]);
        snprintf(tmp[n].nome, sizeof tmp[n].nome, "%s", p->valuestring);
        tmp[n].tamanho = num(it, "size", 0); tmp[n].modificado = num(it, "modified", 0);
        n++;
    }
    cJSON_Delete(j);
    qsort(tmp, n, sizeof tmp[0], cmp_arq);
    /* metadados dos 30 mais recentes (tempo estimado, filamento, camadas) */
    for (int i = 0; i < n && i < 30; i++) {
        char c[400], q[600]; urlcod(tmp[i].nome, c, sizeof c);
        snprintf(q, sizeof q, "/server/files/metadata?filename=%s", c);
        char *m = http("GET", q, NULL, &st, 5);
        if (!m) continue;
        cJSON *jm = cJSON_Parse(m); free(m);
        cJSON *res = cJSON_GetObjectItem(jm, "result");
        tmp[i].tempo_estimado = num(res, "estimated_time", 0);
        tmp[i].filamento_mm = num(res, "filament_total", 0);
        tmp[i].mesa_temp = num(res, "first_layer_bed_temp", 0);
        double lh = num(res, "layer_height", 0), oh = num(res, "object_height", 0);
        tmp[i].camadas = lh > 0 ? (int)(oh / lh + 0.5) : 0;
        cJSON_Delete(jm);
    }
    pthread_mutex_lock(&mx); memcpy(arqs, tmp, n * sizeof tmp[0]); n_arqs = n; v_arqs++; pthread_mutex_unlock(&mx);
}

static void buscar_historico(void)
{
    int st = 0; char *r = http("GET", "/server/history/list?limit=30&order=desc", NULL, &st, 10);
    if (!r) return;
    cJSON *j = cJSON_Parse(r); free(r);
    cJSON *l = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "result"), "jobs");
    hi_historico_t tmp[HI_MAX_HIST]; int n = 0; cJSON *it;
    cJSON_ArrayForEach(it, l) {
        if (n >= HI_MAX_HIST) break;
        memset(&tmp[n], 0, sizeof tmp[n]);
        str(it, "filename", tmp[n].nome, sizeof tmp[n].nome); str(it, "status", tmp[n].estado, sizeof tmp[n].estado);
        tmp[n].inicio = num(it, "start_time", 0); tmp[n].duracao = num(it, "print_duration", 0); tmp[n].filamento_mm = num(it, "filament_used", 0);
        n++;
    }
    cJSON_Delete(j);
    pthread_mutex_lock(&mx); memcpy(hist, tmp, n * sizeof tmp[0]); n_hist = n; v_hist++; pthread_mutex_unlock(&mx);
}

int hi_console(char *dst, int tam, double desde)
{
    int st = 0; char *r = http("GET", "/server/gcode_store?count=200", NULL, &st, 5);
    dst[0] = 0;
    if (!r) return 0;
    cJSON *j = cJSON_Parse(r); free(r);
    cJSON *l = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "result"), "gcode_store");
    int n = 0, usado = 0; cJSON *it;
    cJSON_ArrayForEach(it, l) {
        if (num(it, "time", 0) < desde) continue;
        cJSON *m = cJSON_GetObjectItem(it, "message");
        if (!cJSON_IsString(m)) continue;
        int k = snprintf(dst + usado, tam - usado, "%s\n", m->valuestring);
        if (k < 0 || usado + k >= tam) break;
        usado += k; n++;
    }
    cJSON_Delete(j);
    return n;
}

static void *thread_listas(void *x)
{
    (void)x;
    for (;;) {
        if (pedir_arq) { pedir_arq = 0; buscar_arquivos(); }
        if (pedir_hist) { pedir_hist = 0; buscar_historico(); }
        usleep(200000);
    }
    return NULL;
}

void hi_dados_iniciar(void)
{
    const char *h = getenv("HI_MOONRAKER");   /* teste fora da impressora: HI_MOONRAKER=<ip-da-impressora> */
    if (h && *h) HOST = h;
    snprintf(est.klippy, sizeof est.klippy, "offline");
    pthread_t t;
    pthread_create(&t, NULL, thread_poll, NULL); pthread_detach(t);
    pthread_create(&t, NULL, thread_cmd, NULL); pthread_detach(t);
    pthread_create(&t, NULL, thread_listas, NULL); pthread_detach(t);
}

int hi_klippy(const char *metodo, char *resp, int tam)
{
    int s = socket(AF_UNIX, SOCK_STREAM, 0); if (s < 0) return -1;
    struct sockaddr_un a = {0}; a.sun_family = AF_UNIX; snprintf(a.sun_path, sizeof a.sun_path, "/tmp/klippy_uds");
    struct timeval tv = {5, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    char req[256]; int n = snprintf(req, sizeof req, "{\"id\": 7001, \"method\": \"%s\", \"params\": {}}\x03", metodo);
    if (send(s, req, n, 0) != n) { close(s); return -1; }
    int usado = 0; resp[0] = 0;
    while (usado < tam - 1) {
        ssize_t r = recv(s, resp + usado, tam - 1 - usado, 0); if (r <= 0) break;
        usado += r; resp[usado] = 0;
        char *fim = memchr(resp, 0x03, usado);
        if (fim) { *fim = 0; if (strstr(resp, "\"id\": 7001") || strstr(resp, "\"id\":7001")) break; /* resposta nossa */
                   memmove(resp, fim + 1, usado - (fim + 1 - resp)); usado -= fim + 1 - resp; resp[usado] = 0; }
    }
    close(s);
    return resp[0] ? 0 : -1;
}

double hi_horas_impressas(void)
{
    int st = 0; char *r = http("GET", "/server/history/totals", NULL, &st, 5);
    if (!r) return -1;
    cJSON *j = cJSON_Parse(r); free(r);
    double t = num(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "result"), "job_totals"), "total_print_time", 0);
    cJSON_Delete(j);
    return t / 3600.0;
}
