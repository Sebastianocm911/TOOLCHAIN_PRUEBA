#include "host_api.h"

/* Herramientas de red: interfaz grafica sobre los comandos RED del kernel.
 * No duplica el driver RTL8139 ni intenta implementar TCP dentro de la app. */
#define BG       0xf4f7fc
#define PANEL    0xffffff
#define TEXT     0x14213a
#define DIM      0x5a6b85
#define ACCENT   0x2470e8
#define BORDER   0xc9d5e6
#define OK       0x15803d
#define OUTPUT_MAX 2048

static host_api_t *os;
static char output[OUTPUT_MAX];
static char status_line[96] = "Selecciona una prueba de red";
static int dirty = 1;

static void run_command(const char *command, const char *label) {
    output[0] = 0;
    if (!os->exec || os->exec(command, output, (int)sizeof(output)) < 0) {
        output[0] = 0;
        os->write("net.elf: no se pudo ejecutar el comando del kernel\n");
    }
    int i = 0;
    while (label[i] && i + 1 < (int)sizeof(status_line)) { status_line[i] = label[i]; i++; }
    status_line[i] = 0;
    dirty = 1;
}

static void draw_output(void) {
    int x = 22, y = 116, line_height = 19;
    int width = os->width() - 44;
    os->rect(x, y, width, os->height() - 154, PANEL);
    os->frame(x, y, width, os->height() - 154, BORDER);
    os->text(x + 12, y + 10, "RESULTADO DEL KERNEL", DIM, 0);
    y += 34;
    int shown = 0;
    const char *p = output;
    while (*p && y + line_height < os->height() - 54 && shown < 24) {
        char line[112];
        int n = 0;
        while (*p && *p != '\n' && n + 1 < (int)sizeof(line)) line[n++] = *p++;
        line[n] = 0;
        if (*p == '\n') p++;
        os->text(x + 12, y, line, TEXT, 0);
        y += line_height;
        shown++;
    }
    if (!output[0]) os->text(x + 12, y, "Sin resultados. Ejecuta una prueba con las teclas 1-5.", DIM, 0);
}

static void draw(void) {
    os->clear(BG);
    os->text(22, 12, "Herramientas de red", TEXT, 1);
    os->text(22, 37, status_line, OK, 0);
    os->text(22, 68, "1 Estado   2 ARP   3 Ping puerta   4 DNS   5 Internet", ACCENT, 0);
    os->text(22, 90, "RED usa el driver RTL8139 del kernel. ESC cierra esta ventana.", DIM, 0);
    draw_output();
    os->text(22, os->height() - 25, "Nota: esta version prueba ARP/ICMP/DNS; aun no implementa HTTP/TCP.", DIM, 0);
    os->present();
}

int main(host_api_t *api) {
    if (!api || api->abi_version != HOST_ABI_VERSION) {
        if (api) api->write("net.elf: ABI incompatible\n");
        return 1;
    }
    os = api;
    if (!api->window("Red - ToolChainOS", 760, 470)) {
        api->write("net.elf: no se pudo abrir la ventana\n");
        return 2;
    }
    run_command("RED", "Estado de la interfaz RTL8139");
    host_event_t event;
    int closing = 0;
    while (!closing && api->pump(&event)) {
        if (event.type == HOST_EV_CLOSE) break;
        if (event.type == HOST_EV_KEY) {
            int key = (int)event.key;
            if (key == 27) break;
            if (key == '1') run_command("RED", "Estado de la interfaz RTL8139");
            else if (key == '2') run_command("RED ARP", "Resolucion ARP hacia la puerta");
            else if (key == '3') run_command("RED PING 10.0.2.2", "Ping ICMP a 10.0.2.2");
            else if (key == '4') run_command("RED DNS example.com", "Consulta DNS de example.com");
            else if (key == '5') run_command("RED INTERNET", "Prueba de conectividad QEMU");
            dirty = 1;
        }
        if (dirty) { draw(); dirty = 0; }
    }
    api->write("net.elf: fin correcto\n");
    return api->finish(0);
}
