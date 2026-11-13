/* Hi Painel: telas (320x240 deitada), com dados ao vivo do Klipper/Moonraker. */
#ifndef HI_TELAS_H
#define HI_TELAS_H
#include "lvgl.h"

lv_obj_t *tela_inicio(void);
/* imprimir */
lv_obj_t *tela_imprimir(void);
lv_obj_t *tela_arquivo(void);
lv_obj_t *tela_historico_impr(void);
lv_obj_t *tela_imprimindo(void);
lv_obj_t *tela_ajustes_impr(void);
/* controle */
lv_obj_t *tela_controle(void);
/* filamento */
lv_obj_t *tela_filamento(void);        /* carregar / descarregar / trocar */
lv_obj_t *tela_filamentos(void);       /* banco */
lv_obj_t *tela_filamento_editar(void);
/* calibrações */
lv_obj_t *tela_calibracoes(void);
lv_obj_t *tela_pid(void);
lv_obj_t *tela_shaper(void);
lv_obj_t *tela_malha(void);
lv_obj_t *tela_ztilt(void);
lv_obj_t *tela_pa(void);
lv_obj_t *tela_velocidade(void);
lv_obj_t *tela_historico_calib(void);
lv_obj_t *tela_calibrar_tudo(void);
/* ajustes */
lv_obj_t *tela_ajustes(void);
lv_obj_t *tela_rede(void);
lv_obj_t *tela_tela(void);
lv_obj_t *tela_sobre(void);

/* manutenção e queda de energia */
lv_obj_t *tela_manutencao(void);
lv_obj_t *tela_queda_energia(void);
int hi_manutencao_pendentes(void);
extern char hi_ql_arquivo[200];

/* parâmetros passados entre telas */
extern char hi_arquivo_sel[160];
extern int  hi_pid_mesa;        /* 0 = bico, 1 = mesa */
extern int  hi_fil_editando;    /* índice no nosso banco, -1 = novo */
#endif
