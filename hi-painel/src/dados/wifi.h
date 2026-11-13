/* Hi Painel: conexão Wi-Fi (wpa_supplicant) */
#ifndef HI_WIFI_H
#define HI_WIFI_H
void hi_wifi_conectar(const char *ssid, const char *senha);   /* assíncrono */
int  hi_wifi_status(char *dst, int tam);                       /* 0 parado, 1 conectando, 2 ok, 3 falhou */
void hi_wifi_limpar(void);
void hi_wifi_conectar_salva(const char *ssid);           /* usa a senha já guardada */
int  hi_wifi_salvas(char nomes[][64], int max);
#include <stdbool.h>
bool hi_wifi_esquecer(const char *ssid);
#endif
