/* Hi Painel: dados guardados no próprio aparelho (banco de filamentos, histórico de calibrações,
 * preferências) e edição segura do bloco SAVE_CONFIG do printer.cfg. */
#ifndef HI_ARQUIVOS_H
#define HI_ARQUIVOS_H
#include <stdbool.h>

#define HI_DIR       "/mnt/UDISK/printer_data/hi"
#define HI_PRINTERCFG "/mnt/UDISK/printer_data/config/printer.cfg"
#define HI_MAX_FIL   80

typedef struct {
    char nome[48], tipo[12], origem[12];  /* origem: "Hi" (nosso) ou "Creality" (só leitura) */
    int bico, bico_1a, mesa, vent_min, vent_max;
    double pa, fluxo, vol_max, retracao, zhop;
} hi_fil_t;

int  hi_fil_carregar(hi_fil_t *dst, int max, int *ativo);   /* nossos + os da Creality */
bool hi_fil_salvar(const hi_fil_t *f, int indice_nosso);    /* -1 = novo */
bool hi_fil_apagar(const char *nome);
bool hi_fil_ativar(const char *nome);
void hi_fil_padrao(const char *tipo, hi_fil_t *f);          /* valores padrão por tipo */
extern const char *HI_TIPOS[];

void hi_calib_registrar(const char *tipo, const char *resumo); /* histórico com data */
int  hi_calib_listar(char linhas[][96], int max);
bool hi_calib_ultima(const char *tipo, char *dst, int tam);

int  hi_pref(const char *chave, int padrao);
void hi_pref_set(const char *chave, int valor);

/* grava valores no bloco SAVE_CONFIG (com backup) e comenta a mesma opção no corpo da seção */
bool hi_cfg_autosave(const char *secao, const char *chaves[], const char *valores[], int n, char *erro, int tam_erro);
bool hi_cfg_ler_pid(const char *secao, double pid[3]);
/* troca o teto de aceleração fixo das macros (variable_default_acc, M204 e SET_VELOCITY_LIMIT) com backup */
bool hi_cfg_macros_acel(int nova, char *erro, int tam_erro);
bool hi_cfg_valor(const char *secao, const char *chave, char *dst, int tam);   /* autosave tem prioridade */
#endif
