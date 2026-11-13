/* Hi Painel: dados ao vivo da impressora via Moonraker (HTTP em 127.0.0.1:7125), numa thread própria.
 * A UI lê um retrato do estado (hi_estado_copiar) e envia comandos por fila (hi_gcode, hi_req). */
#ifndef HI_MOONRAKER_H
#define HI_MOONRAKER_H
#include <stdbool.h>

#define HI_MAX_ARQ   200
#define HI_MAX_MALHA 12
#define HI_MAX_HIST  30

typedef struct {
    char nome[160];
    double tamanho, modificado, tempo_estimado, filamento_mm, mesa_temp;
    int camadas;
} hi_arquivo_t;

typedef struct {
    char nome[160], estado[16];
    double inicio, duracao, filamento_mm;
} hi_historico_t;

typedef struct {
    bool conectado;                 /* Moonraker respondeu */
    char klippy[16];                /* ready / shutdown / error / startup */
    char klippy_msg[256];
    /* temperaturas */
    double bico, bico_alvo, bico_pot, mesa, mesa_alvo, mesa_pot, mcu;
    /* impressão */
    char impr_estado[16];           /* standby / printing / paused / complete / cancelled / error */
    char impr_arquivo[160], impr_msg[160];
    double progresso, impr_duracao, total_duracao, filamento_usado;
    int camada, camadas, power_loss;
    /* movimento */
    double pos[4], vel_fator, fluxo_fator, z_offset, pa, vel_atual;
    char homed[8];
    char ocioso[12];                /* idle_timeout: Printing (executando G-code) / Ready / Idle */
    double max_accel, max_vel;
    /* saídas */
    double led, ventoinha;
    int filamento_no_bico;
    int config_pendente;            /* configfile.save_config_pending: calibração calculada e ainda não gravada */          /* sensor de filamento do cabeçote (filament_sensor) */
    /* malha */
    int malha_lin, malha_col; double malha[HI_MAX_MALHA][HI_MAX_MALHA], malha_min, malha_max;
    char malha_perfil[32];
    /* sistema */
    double carga, mem_livre_kb;
    char rede[64], ip[32], ssid[64];
    int sinal_dbm, freq_mhz;
    unsigned versao;                /* incrementa a cada atualização */
} hi_estado_t;

void hi_dados_iniciar(void);
void hi_estado_copiar(hi_estado_t *dst);

/* comandos (assíncronos, em fila) */
void hi_gcode(const char *fmt, ...);                 /* POST /printer/gcode/script */
void hi_req(const char *metodo, const char *caminho); /* outra chamada da API, ex.: POST /printer/print/pause */

/* listas sob demanda: hi_pedir_*(); depois hi_*_pronto() > 0 quando atualizado */
void hi_pedir_arquivos(void);
int  hi_arquivos(hi_arquivo_t *dst, int max, unsigned *versao);
void hi_pedir_historico(void);
int  hi_historico(hi_historico_t *dst, int max, unsigned *versao);

/* respostas do console (para ler resultados de calibração): últimas linhas desde 'desde' (tempo unix) */
int  hi_console(char *dst, int tam, double desde);
/* última mensagem de erro de comando (vazio se nenhuma) e contador */
unsigned hi_erro_cmd(char *dst, int tam);
double hi_agora(void);
/* chamada direta ao Klipper pelo socket /tmp/klippy_uds (endpoints que o Moonraker não expõe). Devolve o JSON de resposta. */
int hi_klippy(const char *metodo, char *resp, int tam);
/* horas de impressão registradas pelo Moonraker (histórico) */
double hi_horas_impressas(void);
#endif
