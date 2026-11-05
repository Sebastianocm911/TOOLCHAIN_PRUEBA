#include "host_api.h"

/* Browser experimental: interfaz informativa. El kernel actual dispone de
 * Ethernet/ARP/ICMP/DNS, pero no ofrece sockets TCP ni cliente HTTP. Por eso
 * esta app no finge cargar sitios; muestra el estado real de la pila de red. */
#define BG 0xf4f7fc
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
static void draw(host_api_t *os){
    os->clear(BG);os->text(22,16,"ToolChain Browser",INK,1);
    os->rect(18,60,os->width()-36,42,0xffffff);os->frame(18,60,os->width()-36,42,0xc9d5e6);
    os->text(30,74,"http://  (cliente HTTP pendiente de sockets TCP)",MUTED,0);
    os->text(22,132,"Estado de compatibilidad",BLUE,1);
    os->text(22,174,"[OK] Adaptador RTL8139 y configuracion de red del kernel",INK,0);
    os->text(22,204,"[OK] Resolucion DNS y pruebas ICMP desde la herramienta NET",INK,0);
    os->text(22,234,"[PENDIENTE] Transporte TCP, sockets y cliente HTTP",0xb45309,0);
    os->text(22,278,"La ventana es una vista previa; no afirma que Internet/web ya funcione.",MUTED,0);
    os->text(22,os->height()-30,"ESC para cerrar",BLUE,0);os->present();
}
int main(host_api_t *api){
    if(!api||api->abi_version!=HOST_ABI_VERSION){if(api)api->write("browser.elf: ABI incompatible\n");return 1;}
    if(!api->window("Browser experimental - ToolChainOS",720,380))return 2;
    draw(api);host_event_t ev;while(api->pump(&ev)){if(ev.type==HOST_EV_CLOSE)break;if(ev.type==HOST_EV_KEY&&(int)ev.key==27)break;}
    return api->finish(0);
}
