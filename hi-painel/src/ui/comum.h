/* Hi Painel: componentes comuns (teclados, confirmação, avisos, atualização ao vivo, formatação) */
#ifndef HI_COMUM_H
#define HI_COMUM_H
#include "lvgl.h"
#include "../dados/moonraker.h"

typedef void (*hi_num_cb)(double valor, void *ud);
typedef void (*hi_txt_cb)(const char *valor, void *ud);
typedef void (*hi_ok_cb)(void *ud);
typedef void (*hi_atualizar_fn)(const hi_estado_t *e);

void hi_toast(const char *txt, lv_color_t cor);
void hi_confirmar(const char *titulo, const char *texto, const char *botao_ok, lv_color_t cor_ok, hi_ok_cb cb, void *ud);
void hi_teclado_num(const char *titulo, double inicial, double min, double max, int decimais, hi_num_cb cb, void *ud);
typedef void (*hi_menu_cb)(int opcao, void *ud);
/* menu com até 4 opções empilhadas; a última pode ser "Voltar" (cb recebe o índice) */
void hi_menu(const char *titulo, const char *texto, const char *opcoes[], const lv_color_t cores[], int n, hi_menu_cb cb, void *ud);
/* janela com deslizador da velocidade REAL da ventoinha da peça (20-100%) + liga/desliga */
void hi_ventoinha_popup(void);
void hi_teclado_texto(const char *titulo, const char *inicial, hi_txt_cb cb, void *ud);

/* a tela atual registra uma função chamada a cada 500 ms com o estado ao vivo (zerada ao trocar de tela) */
/* tela "Reiniciando o Klipper..." que só sai quando o Klipper volta a ficar pronto (chamar depois de enviar
 * FIRMWARE_RESTART / SAVE_CONFIG). 'depois' é a mensagem mostrada quando terminar (pode ser NULL). */
void hi_reiniciando(const char *motivo, const char *depois);
/* opcional: chamada uma vez quando o Klipper voltar pronto (em vez do aviso). Definir logo depois de hi_reiniciando. */
extern hi_ok_cb hi_reiniciando_depois; extern void *hi_reiniciando_depois_ud;
void hi_set_atualizar(hi_atualizar_fn f);
void hi_atualizar_agora(void);          /* chamado pelo timer principal */
extern hi_estado_t hi_e;                /* último estado copiado */

char *hi_br(char *b);                   /* vírgula decimal */
void hi_fmt_tempo(double s, char *dst, int tam);
const char *hi_estado_txt(const hi_estado_t *e, lv_color_t *cor);
bool hi_ocupada(const hi_estado_t *e);  /* imprimindo / pausada: bloqueia calibrações e movimentos */
#endif
