#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniatura.h"

#define CACHE 48
static struct { char nome[170]; int grande; lv_img_dsc_t dsc; int vazio; } cache[CACHE];
static int prox;

static int b64v(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A'; if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52; if (c == '+') return 62; if (c == '/') return 63; return -1;
}

void hi_miniatura_limpar_cache(void)
{
    for (int i = 0; i < CACHE; i++) { free((void *)cache[i].dsc.data); memset(&cache[i], 0, sizeof cache[i]); }
    lv_img_cache_invalidate_src(NULL);
}

const lv_img_dsc_t *hi_miniatura(const char *nome, int grande)
{
    for (int i = 0; i < CACHE; i++)
        if (cache[i].nome[0] && cache[i].grande == grande && !strcmp(cache[i].nome, nome)) return cache[i].vazio ? NULL : &cache[i].dsc;
    char p[400]; snprintf(p, sizeof p, "%s/%s", HI_GCODES, nome);
    FILE *f = fopen(p, "r");
    int slot = prox; prox = (prox + 1) % CACHE;
    if (cache[slot].dsc.data) { lv_img_cache_invalidate_src(&cache[slot].dsc); free((void *)cache[slot].dsc.data); }
    memset(&cache[slot], 0, sizeof cache[slot]);
    snprintf(cache[slot].nome, sizeof cache[slot].nome, "%s", nome); cache[slot].grande = grande; cache[slot].vazio = 1;
    if (!f) return NULL;
    /* procura os blocos nos primeiros ~400 KB; escolhe pelo tamanho */
    char l[256]; long lido = 0; int melhor_w = grande ? 0 : 100000, w, h, tam; long melhor_pos = -1; int mw = 0, mh = 0;
    while (lido < 400000 && fgets(l, sizeof l, f)) {
        lido += strlen(l);
        if (!strncmp(l, "; thumbnail begin ", 18) && sscanf(l + 18, "%dx%d %d", &w, &h, &tam) == 3) {
            int ok = grande ? (w > melhor_w && w <= 320) : (w < melhor_w && w >= 24);
            if (ok) { melhor_w = w; mw = w; mh = h; melhor_pos = ftell(f); }
        }
        if (strstr(l, "; EXECUTABLE_BLOCK_START") || (l[0] != ';' && l[0] != '\n' && l[0] != '\r')) break;
    }
    if (melhor_pos < 0) { fclose(f); return NULL; }
    fseek(f, melhor_pos, SEEK_SET);
    size_t cap = 65536, n = 0; unsigned char *png = malloc(cap); unsigned acc = 0; int bits = 0;
    while (fgets(l, sizeof l, f) && strncmp(l, "; thumbnail end", 15)) {
        for (char *c = l + 1; *c; c++) {
            int v = b64v(*c); if (v < 0) continue;
            acc = (acc << 6) | v; bits += 6;
            if (bits >= 8) { bits -= 8; if (n + 1 >= cap) { cap *= 2; png = realloc(png, cap); } png[n++] = (acc >> bits) & 0xFF; }
        }
    }
    fclose(f);
    if (n < 8 || png[0] != 0x89 || png[1] != 'P') { free(png); return NULL; }   /* só PNG */
    cache[slot].dsc.header.always_zero = 0; cache[slot].dsc.header.w = mw; cache[slot].dsc.header.h = mh;
    cache[slot].dsc.header.cf = LV_IMG_CF_RAW_ALPHA; cache[slot].dsc.data_size = n; cache[slot].dsc.data = png;
    cache[slot].vazio = 0;
    return &cache[slot].dsc;
}
