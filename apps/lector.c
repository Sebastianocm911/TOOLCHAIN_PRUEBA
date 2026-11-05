#include "host_api.h"
#include "fs.h"

/* Lector inicial de texto UTF-8. El FS V3.1 almacena texto de hasta 1023 bytes.
 * DOCX nativo requiere una futura API binaria + ZIP/DEFLATE + parser OOXML. */
#define BG       0xf4f7fc
#define PANEL    0xffffff
#define TEXT     0x14213a
#define DIM      0x5a6b85
#define ACCENT   0x2470e8
#define BORDER   0xc9d5e6
#define RED      0xd92b2b
#define CONTENT_MAX (FS_FILE_MAX_SIZE)
#define LINE_MAX 120
#define LINE_H   20

static host_api_t *os;
static char content[CONTENT_MAX];
static char current_path[FS_PATH_MAX];
static char message[128];
static uint32_t content_size;
static int top_line;
static int total_lines = 1;

static int text_len(const char *s) { int n = 0; while (s && s[n]) n++; return n; }
static void copy_text(char *to, int cap, const char *from) {
    int i = 0;
    while (from && from[i] && i + 1 < cap) { to[i] = from[i]; i++; }
    to[i] = 0;
}
static int has_docx_extension(const char *path) {
    int n = text_len(path);
    if (n < 5) return 0;
    const char *e = path + n - 5;
    const char *want = ".docx";
    while (*want) {
        char a = *e++, b = *want++;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (a != b) return 0;
    }
    return 1;
}
static void count_lines(void) {
    total_lines = 1;
    for (uint32_t i = 0; i < content_size; i++) if (content[i] == '\n') total_lines++;
}
static void load_path(const char *path) {
    content[0] = 0; content_size = 0; top_line = 0; current_path[0] = 0;
    copy_text(current_path, (int)sizeof(current_path), path);
    if (!path || !*path) {
        copy_text(message, (int)sizeof(message), "Usa: LECTOR /ruta/documento.txt o .md");
        return;
    }
    if (has_docx_extension(path)) {
        copy_text(message, (int)sizeof(message), "DOCX nativo pendiente: el FS actual no admite binarios/ZIP.");
        return;
    }
    uint32_t size = 0;
    const char *src = os->read_file ? os->read_file(path, &size) : 0;
    if (!src) {
        copy_text(message, (int)sizeof(message), "No se pudo leer. Verifica la ruta y que sea un archivo de texto.");
        return;
    }
    if (size >= CONTENT_MAX) size = CONTENT_MAX - 1;
    for (uint32_t i = 0; i < size; i++) content[i] = src[i];
    content[size] = 0;
    content_size = size;
    count_lines();
    copy_text(message, (int)sizeof(message), "Documento de texto cargado");
    os->write("lector.elf: DOCUMENTO DE TEXTO CARGADO\n");
}

static void draw_content(void) {
    int x = 20, y = 98, width = os->width() - 40, height = os->height() - 142;
    os->rect(x, y, width, height, PANEL);
    os->frame(x, y, width, height, BORDER);
    if (!content_size) {
        os->text(x + 14, y + 18, message, has_docx_extension(current_path) ? RED : DIM, 0);
        if (has_docx_extension(current_path)) {
            os->text(x + 14, y + 52, "DOCX es un paquete ZIP con XML comprimido (normalmente DEFLATE).", TEXT, 0);
            os->text(x + 14, y + 76, "Para leerlo dentro del kernel falta soporte de lectura binaria y descompresion.", TEXT, 0);
            os->text(x + 14, y + 100, "Por ahora abre TXT/MD guardados en el FS de ToolChainOS.", DIM, 0);
        } else {
            os->text(x + 14, y + 52, "Ejemplo: LECTOR /documentos/tarea.txt", TEXT, 0);
            os->text(x + 14, y + 76, "Formatos actuales: texto plano UTF-8 (.txt, .md).", DIM, 0);
        }
        return;
    }
    const char *p = content;
    int line = 0, yline = y + 14, shown = 0;
    while (*p && yline + LINE_H < y + height - 6) {
        char buffer[LINE_MAX];
        int n = 0;
        while (*p && *p != '\n' && n + 1 < (int)sizeof(buffer)) buffer[n++] = *p++;
        buffer[n] = 0;
        if (*p == '\n') p++;
        if (line >= top_line) {
            os->text(x + 14, yline, buffer, TEXT, 0);
            yline += LINE_H;
            shown++;
        }
        line++;
        if (shown >= 30) break;
    }
}
static void draw(void) {
    os->clear(BG);
    os->text(20, 10, "Lector de documentos", TEXT, 1);
    os->text(20, 38, current_path[0] ? current_path : "Sin documento seleccionado", ACCENT, 0);
    os->text(20, 62, message, DIM, 0);
    draw_content();
    os->text(20, os->height() - 26, "Flechas arriba/abajo desplazan  |  INICIO/FIN  |  ESC cerrar", DIM, 0);
    os->present();
}

int main(host_api_t *api) {
    if (!api || api->abi_version != HOST_ABI_VERSION) {
        if (api) api->write("lector.elf: ABI incompatible\n");
        return 1;
    }
    os = api;
    if (!api->window("Lector de documentos", 760, 470)) {
        api->write("lector.elf: no se pudo abrir la ventana\n");
        return 2;
    }
    load_path(api->launch_file ? api->launch_file() : "");
    draw();
    host_event_t event;
    while (api->pump(&event)) {
        if (event.type == HOST_EV_CLOSE) break;
        if (event.type == HOST_EV_KEY) {
            int key = (int)event.key;
            if (key == 27) break;
            if (key == HOST_KEY_UP && top_line > 0) top_line--;
            else if (key == HOST_KEY_DOWN && top_line + 1 < total_lines) top_line++;
            else if (key == HOST_KEY_HOME) top_line = 0;
            else if (key == HOST_KEY_END) top_line = total_lines > 1 ? total_lines - 1 : 0;
            draw();
        }
    }
    api->write("lector.elf: fin correcto\n");
    return api->finish(0);
}
