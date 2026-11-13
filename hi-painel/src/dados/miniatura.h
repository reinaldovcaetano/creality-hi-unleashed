/* Hi Painel: miniaturas embutidas nos G-codes ("; thumbnail begin WxH tamanho" ... base64 PNG ... "; thumbnail end"),
 * lidas direto do arquivo (o Moonraker desta versão não indexa as miniaturas do Creality Print/Orca). */
#ifndef HI_MINIATURA_H
#define HI_MINIATURA_H
#include "lvgl.h"
#define HI_GCODES "/mnt/UDISK/printer_data/gcodes"
/* devolve uma imagem (PNG) pronta para lv_img_set_src, ou NULL. grande=0: a menor; grande=1: a maior até 300 px.
 * As imagens ficam num cache (não liberar). */
const lv_img_dsc_t *hi_miniatura(const char *nome_relativo, int grande);
void hi_miniatura_limpar_cache(void);
#endif
