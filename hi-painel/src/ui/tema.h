/* Hi Painel: tema visual (escuro, moderno) e componentes reutilizáveis.
 * A tela da Hi é DEITADA: 320x240 (pixels quadrados). O painel GC9307 é 240x320 em pé e foi montado
 * girado 90 graus; o envio ao /dev/fb0 faz a rotação (ver main_fb.c). Teste com foto em 13/11/2026. */
#ifndef HI_TEMA_H
#define HI_TEMA_H
#include "lvgl.h"

#define HI_W 320
#define HI_H 240

#define COR_FUNDO      lv_color_hex(0x0E1116)
#define COR_CARTAO     lv_color_hex(0x181C23)
#define COR_CARTAO2    lv_color_hex(0x222833)
#define COR_BORDA      lv_color_hex(0x2C3340)
#define COR_TEXTO      lv_color_hex(0xE8ECF2)
#define COR_TEXTO2     lv_color_hex(0x8B95A5)
#define COR_DESTAQUE   lv_color_hex(0x3B82F6)
#define COR_OK         lv_color_hex(0x22C55E)
#define COR_AVISO      lv_color_hex(0xF59E0B)
#define COR_ERRO       lv_color_hex(0xEF4444)
#define COR_BICO       lv_color_hex(0xFB923C)
#define COR_MESA       lv_color_hex(0x38BDF8)

LV_FONT_DECLARE(hi_font_12)
LV_FONT_DECLARE(hi_font_14)
LV_FONT_DECLARE(hi_font_16)
LV_FONT_DECLARE(hi_font_20)
LV_FONT_DECLARE(hi_font_28)
LV_FONT_DECLARE(hi_font_40)

typedef lv_obj_t *(*hi_tela_fn)(void);

lv_obj_t *hi_tela_nova(void);
lv_obj_t *hi_cabecalho(lv_obj_t *tela, const char *titulo, bool voltar);
lv_obj_t *hi_cartao(lv_obj_t *pai, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
lv_obj_t *hi_rotulo(lv_obj_t *pai, const char *txt, const lv_font_t *f, lv_color_t cor);
lv_obj_t *hi_botao(lv_obj_t *pai, const char *txt, lv_color_t cor, lv_coord_t w, lv_coord_t h);
lv_obj_t *hi_lista(lv_obj_t *tela, lv_coord_t y, lv_coord_t h);  /* área rolável em coluna */
lv_obj_t *hi_item(lv_obj_t *lista, const char *icone, lv_color_t cor_icone, const char *titulo, const char *detalhe);
lv_obj_t *hi_item_grande(lv_obj_t *lista, const char *icone, lv_color_t cor_icone, const char *titulo, const char *detalhe);
lv_obj_t *hi_campo_num(lv_obj_t *pai, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *nome, const char *valor, const char *dica);

/* navegação */
void hi_ir(hi_tela_fn f);            /* abre a tela (empilha) */
void hi_voltar(void);                /* volta para a anterior */
void hi_ir_raiz(hi_tela_fn f);       /* zera a pilha */
void hi_definir_raiz(hi_tela_fn f);  /* zera a pilha sem carregar (use antes de hi_ir) */
void hi_trocar(hi_tela_fn f);        /* substitui a tela atual (sem empilhar) */
void hi_ao_clicar(lv_obj_t *o, hi_tela_fn destino);
extern void (*hi_sair_cb)(void);
extern uint32_t hi_bloqueio_ate;
void hi_bloquear_toque(uint32_t ms);     /* "voltar para a tela Creality" */
#endif
