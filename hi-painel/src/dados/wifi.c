/* Hi Painel: conectar em rede Wi-Fi pelo socket de controle do wpa_supplicant (o mesmo que o wifi-server da
 * Creality usa). Segurança: a rede nova é testada; se não conectar em 25 s, as outras redes são reativadas
 * e a impressora volta para a rede anterior. Só grava (SAVE_CONFIG) quando conectou. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <stdbool.h>
#include "wifi.h"

#define CTRL "/etc/wifi/wpa_supplicant/sockets/wlan0"
static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static char status[160]; static int estado;   /* 0 parado, 1 conectando, 2 ok, 3 falhou */

static int wpa(const char *cmd, char *resp, size_t tam)
{
    int s = socket(AF_UNIX, SOCK_DGRAM, 0); if (s < 0) return -1;
    struct sockaddr_un local = {0}, dest = {0};
    local.sun_family = AF_UNIX; snprintf(local.sun_path, sizeof local.sun_path, "/tmp/hi_wpa_%d", getpid());
    unlink(local.sun_path);
    if (bind(s, (struct sockaddr *)&local, sizeof local) < 0) { close(s); return -1; }
    dest.sun_family = AF_UNIX; snprintf(dest.sun_path, sizeof dest.sun_path, "%s", CTRL);
    if (connect(s, (struct sockaddr *)&dest, sizeof dest) < 0) { close(s); unlink(local.sun_path); return -1; }
    struct timeval tv = {5, 0}; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    int ok = -1;
    if (send(s, cmd, strlen(cmd), 0) >= 0) {
        for (int i = 0; i < 5; i++) {
            ssize_t n = recv(s, resp, tam - 1, 0);
            if (n <= 0) break;
            resp[n] = 0;
            if (resp[0] == '<') continue;          /* mensagem não solicitada */
            ok = 0; break;
        }
    }
    close(s); unlink(local.sun_path);
    return ok;
}
static int wpa_ok(const char *cmd) { char r[256]; return wpa(cmd, r, sizeof r) == 0 && !strncmp(r, "OK", 2); }

static void set_status(int e, const char *fmt, const char *a)
{
    pthread_mutex_lock(&mx); estado = e; snprintf(status, sizeof status, fmt, a ? a : ""); pthread_mutex_unlock(&mx);
}

typedef struct { char ssid[64], senha[80]; int usar_salva; } pedido_t;

static void *conectar(void *x)
{
    pedido_t *p = x; char r[2048], c[256]; int id = -1;
    set_status(1, "Conectando em %s...", p->ssid);
    /* já existe? */
    if (wpa("LIST_NETWORKS", r, sizeof r) == 0) {
        char *l = strtok(r, "\n");
        while ((l = strtok(NULL, "\n"))) {
            int n; char nome[80] = "";
            if (sscanf(l, "%d\t%79[^\t]", &n, nome) == 2 && !strcmp(nome, p->ssid)) { id = n; break; }
        }
    }
    int existia = id >= 0, id_antigo = -1;
    if (existia && !p->usar_salva) { id_antigo = id; id = -1; }   /* senha nova: testa numa entrada separada */
    if (id < 0) { if (wpa("ADD_NETWORK", r, sizeof r) != 0) { set_status(3, "Wi-Fi não responde%s", NULL); free(p); return NULL; } id = atoi(r); }
    snprintf(c, sizeof c, "SET_NETWORK %d ssid \"%s\"", id, p->ssid); wpa_ok(c);
    if (p->usar_salva && id >= 0) { /* mantém a senha guardada */ }
    else if (p->senha[0]) { snprintf(c, sizeof c, "SET_NETWORK %d psk \"%s\"", id, p->senha); if (!wpa_ok(c)) { set_status(3, "Senha inválida (8 a 63 caracteres)%s", NULL); free(p); return NULL; } }
    else { snprintf(c, sizeof c, "SET_NETWORK %d key_mgmt NONE", id); wpa_ok(c); }
    snprintf(c, sizeof c, "SET_NETWORK %d scan_ssid 1", id); wpa_ok(c);
    snprintf(c, sizeof c, "SET_NETWORK %d priority 20", id); wpa_ok(c);
    snprintf(c, sizeof c, "SELECT_NETWORK %d", id); wpa_ok(c);
    int ok = 0;
    for (int i = 0; i < 25 && !ok; i++) {
        sleep(1);
        if (wpa("STATUS", r, sizeof r) == 0 && strstr(r, "wpa_state=COMPLETED")) {
            char *s = strstr(r, "\nssid="); if (s && !strncmp(s + 6, p->ssid, strlen(p->ssid)) && s[6 + strlen(p->ssid)] == '\n') ok = 1;
        }
    }
    wpa_ok("ENABLE_NETWORK all");          /* as outras redes voltam a valer (anterior como reserva) */
    if (ok) {
        if (id_antigo >= 0) { snprintf(c, sizeof c, "REMOVE_NETWORK %d", id_antigo); wpa_ok(c); }   /* a senha nova substitui a antiga */
        wpa_ok("SAVE_CONFIG"); set_status(2, "Conectado em %s", p->ssid);
    }
    else {
        if (!p->usar_salva) { snprintf(c, sizeof c, "REMOVE_NETWORK %d", id); wpa_ok(c); }   /* a entrada de teste sai; a salva fica como estava */
        wpa_ok("REASSOCIATE");
        set_status(3, "Não conectou em %s (senha errada ou sinal fraco). Voltando para a rede anterior.", p->ssid);
    }
    free(p);
    return NULL;
}

void hi_wifi_conectar(const char *ssid, const char *senha)
{
    pedido_t *p = calloc(1, sizeof *p);
    snprintf(p->ssid, sizeof p->ssid, "%s", ssid); snprintf(p->senha, sizeof p->senha, "%s", senha);
    pthread_t t; pthread_create(&t, NULL, conectar, p); pthread_detach(t);
}
int hi_wifi_status(char *dst, int tam) { pthread_mutex_lock(&mx); snprintf(dst, tam, "%s", status); int e = estado; pthread_mutex_unlock(&mx); return e; }
void hi_wifi_limpar(void) { pthread_mutex_lock(&mx); estado = 0; status[0] = 0; pthread_mutex_unlock(&mx); }

void hi_wifi_conectar_salva(const char *ssid)
{
    pedido_t *p = calloc(1, sizeof *p); snprintf(p->ssid, sizeof p->ssid, "%s", ssid); p->usar_salva = 1;
    pthread_t t; pthread_create(&t, NULL, conectar, p); pthread_detach(t);
}

int hi_wifi_salvas(char nomes[][64], int max)
{
    char r[2048]; int n = 0;
    if (wpa("LIST_NETWORKS", r, sizeof r) != 0) return 0;
    char *l = strtok(r, "\n");
    while ((l = strtok(NULL, "\n")) && n < max) {
        int id; char nome[80] = "";
        if (sscanf(l, "%d\t%79[^\t]", &id, nome) == 2) snprintf(nomes[n++], 64, "%s", nome);
    }
    return n;
}

bool hi_wifi_esquecer(const char *ssid)
{
    char r[2048], c[64]; int id = -1;
    if (wpa("LIST_NETWORKS", r, sizeof r) != 0) return false;
    char *l = strtok(r, "\n");
    while ((l = strtok(NULL, "\n"))) { int n; char nome[80] = ""; if (sscanf(l, "%d\t%79[^\t]", &n, nome) == 2 && !strcmp(nome, ssid)) { id = n; break; } }
    if (id < 0) return false;
    snprintf(c, sizeof c, "REMOVE_NETWORK %d", id);
    bool ok = wpa_ok(c); wpa_ok("SAVE_CONFIG");
    set_status(ok ? 2 : 3, ok ? "Rede \"%s\" esquecida" : "Não consegui esquecer \"%s\"", ssid);
    return ok;
}
