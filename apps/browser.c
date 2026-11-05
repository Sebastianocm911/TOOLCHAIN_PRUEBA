#include "host_api.h"

/* ToolChain Browser: navegador textual HTTPS.
 * El kernel envia URL por TC/1 en COM2; el puente del anfitrion hace TLS
 * verificado y devuelve HTML convertido a texto. No ejecuta JavaScript/CSS. */

#define BG       0xf1f5fb
#define PANEL    0xffffff
#define INK      0x14213a
#define MUTED    0x5a6b85
#define BLUE     0x2470e8
#define BORDER   0xc9d5e6
#define AMBER    0xb45309
#define URL_CAP  320
#define PAGE_CAP 1025
#define LINE_CAP 112

static host_api_t *os;
static char address[URL_CAP];
static char page[PAGE_CAP];
static char status_line[96] = "Escribe una URL HTTPS o una busqueda y pulsa Enter";
static int address_len, page_scroll, dirty = 1;

static int length(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}
static void copy_text(char *dst, int cap, const char *src) {
    int i = 0;
    while (src && src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}
static void set_status(const char *s) {
    copy_text(status_line, (int)sizeof(status_line), s);
    dirty = 1;
}
static int starts(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i] && s[i] == prefix[i]) i++;
    return prefix[i] == 0;
}
static int safe_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
static int append(char *dst, int cap, const char *src) {
    int used = length(dst), i = 0;
    while (src && src[i] && used + 1 < cap) dst[used++] = src[i++];
    dst[used] = 0;
    return src == 0 || src[i] == 0;
}
static int normalize_url(char *out, int cap) {
    out[0] = 0;
    if (!address_len) return 0;
    int query = 0, looks_like_host = 0;
    for (int i = 0; i < address_len; i++) {
        if (address[i] == ' ' || address[i] == '\t') query = 1;
        if (address[i] == '.' || address[i] == '/' || address[i] == ':') looks_like_host = 1;
    }
    if (!query && !looks_like_host && !starts(address, "http://") && !starts(address, "https://")) query = 1;
    if (query) {
        if (!append(out, cap, "https://www.google.com/search?q=")) return 0;
        for (int i = 0; i < address_len; i++) {
            unsigned char c = (unsigned char)address[i];
            if (safe_char(c)) {
                char one[2] = {(char)c, 0};
                if (!append(out, cap, one)) return 0;
            } else if (c == ' ') {
                if (!append(out, cap, "%20")) return 0;
            } else {
                static const char hex[] = "0123456789ABCDEF";
                char encoded[4] = {'%', hex[c >> 4], hex[c & 15], 0};
                if (!append(out, cap, encoded)) return 0;
            }
        }
        return 1;
    }
    if (starts(address, "https://")) return append(out, cap, address);
    if (starts(address, "http://")) {
        if (!append(out, cap, "https://")) return 0;
        return append(out, cap, address + 7);
    }
    if (!append(out, cap, "https://")) return 0;
    return append(out, cap, address);
}
static void browse(void) {
    char url[1024];
    if (!normalize_url(url, (int)sizeof(url))) {
        set_status("URL vacia o demasiado larga");
        return;
    }
    if (!os->web_fetch) {
        set_status("ABI sin servicio web: reconstruye la ISO actualizada");
        return;
    }
    page[0] = 0;
    page_scroll = 0;
    set_status("Conectando por HTTPS mediante el puente COM2...");
    os->present();
    int ok = os->web_fetch(url, page, (int)sizeof(page));
    if (ok) {
        copy_text(address, (int)sizeof(address), url);
        address_len = length(address);
        set_status("Pagina recibida por HTTPS | ↑/↓ desplazar | barra superior editable");
        os->write("browser.elf: pagina HTTPS recibida\n");
    } else {
        if (!page[0]) copy_text(page, (int)sizeof(page), "No se pudo obtener la pagina. Revisa el puente anfitrion.");
        set_status("No se pudo cargar la pagina; consulta el mensaje mostrado");
        os->write("browser.elf: error de descarga HTTPS\n");
    }
    dirty = 1;
}
static void draw_page(void) {
    int x = 28, y = 126, line_height = 18;
    int max_lines = (os->height() - 174) / line_height;
    int logical = 0, offset = 0;
    while (page[offset] && logical < page_scroll + max_lines) {
        char line[LINE_CAP];
        int used = 0;
        while (page[offset] && page[offset] != '\n' && used + 1 < (int)sizeof(line)) {
            unsigned char c = (unsigned char)page[offset];
            int bytes = (c < 0x80) ? 1 : ((c & 0xe0) == 0xc0 ? 2 : ((c & 0xf0) == 0xe0 ? 3 : 4));
            if (used + bytes >= (int)sizeof(line)) break;
            for (int b = 0; b < bytes && page[offset]; b++) line[used++] = page[offset++];
        }
        if (page[offset] == '\n') offset++;
        line[used] = 0;
        if (logical >= page_scroll) {
            os->text(x, y, line, logical == page_scroll ? BLUE : INK, 0);
            y += line_height;
        }
        logical++;
    }
    if (page_scroll > 0) os->text(os->width() - 90, 108, "Mas arriba", MUTED, 0);
    if (page[offset]) os->text(os->width() - 90, os->height() - 45, "Mas abajo", MUTED, 0);
}
static void draw(void) {
    os->clear(BG);
    os->text(20, 12, "ToolChain Browser", INK, 1);
    os->text(20, 40, "Navegacion web HTTPS | HTML a texto | sin JavaScript ni extensiones", MUTED, 0);
    os->rect(18, 66, os->width() - 112, 38, PANEL);
    os->frame(18, 66, os->width() - 112, 38, BORDER);
    os->text(28, 78, address[0] ? address : "https://www.google.com", INK, 0);
    os->rect(os->width() - 84, 66, 66, 38, BLUE);
    os->text(os->width() - 65, 78, "IR", PANEL, 0);
    os->rect(18, 112, os->width() - 36, os->height() - 164, PANEL);
    os->frame(18, 112, os->width() - 36, os->height() - 164, BORDER);
    draw_page();
    os->text(20, os->height() - 42, status_line, AMBER, 0);
    os->text(20, os->height() - 22, "Escribe URL/busqueda | Enter: ir | Backspace: editar | ↑/↓: desplazarse | ESC: salir", MUTED, 0);
    os->present();
}
int main(host_api_t *api) {
    if (!api || api->abi_version != HOST_ABI_VERSION) {
        if (api) api->write("browser.elf: ABI incompatible\n");
        return 1;
    }
    os = api;
    if (!api->window("ToolChain Browser - HTTPS", 820, 540)) return 2;
    address[0] = 0; page[0] = 0; address_len = 0;
    host_event_t event;
    while (api->pump(&event)) {
        if (event.type == HOST_EV_CLOSE) break;
        if (event.type == HOST_EV_DOWN && event.y >= 66 && event.y <= 106) {
            if (event.x >= api->width() - 100) browse();
            dirty = 1;
        }
        if (event.type == HOST_EV_KEY) {
            int key = (int)event.key;
            if (key == 27) break;
            if (key == HOST_KEY_UP) { if (page_scroll > 0) page_scroll--; dirty = 1; }
            else if (key == HOST_KEY_DOWN) { page_scroll++; dirty = 1; }
            else if (key == 19) { browse(); }
            else if (key == '\r' || key == '\n') browse();
            else if ((key == '\b' || key == 127) && address_len > 0) {
                address[--address_len] = 0; dirty = 1;
            } else if (key >= 32 && key <= 126 && address_len + 1 < URL_CAP) {
                address[address_len++] = (char)key; address[address_len] = 0; dirty = 1;
            }
        }
        if (dirty) { draw(); dirty = 0; }
    }
    api->write("browser.elf: cierre correcto\n");
    return api->finish(0);
}
