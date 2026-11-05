#include "host_api.h"
#include "fs.h"

/* ToolChain Writer: editor ligero UTF-8; documentos de texto plano. */
#define BG 0xf1f5fb
#define PANEL 0xffffff
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
#define BORDER 0xc9d5e6
#define LIMIT (FS_FILE_MAX_SIZE - 1)
#define PATH_CAP FS_PATH_MAX

static host_api_t *os;
static char document[LIMIT + 1];
static char path[PATH_CAP] = "/documento.txt";
static char status_line[72] = "Documento nuevo";
static int length, cursor, dirty = 1;

static void copy(char *dst, int cap, const char *src) {
    int i = 0;
    while (src && src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}
static int utf8_width(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xe0) == 0xc0) return 2;
    if ((c & 0xf0) == 0xe0) return 3;
    return 4;
}
static int previous_char(int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && ((unsigned char)document[pos] & 0xc0) == 0x80) pos--;
    return pos;
}
static int next_char(int pos) {
    int n;
    if (pos >= length) return length;
    n = utf8_width((unsigned char)document[pos]);
    pos += n;
    return pos > length ? length : pos;
}
static void load(const char *target) {
    uint32_t size = 0;
    const char *src = os->read_file(target, &size);
    copy(path, (int)sizeof(path), target);
    if (!src) {
        length = cursor = 0;
        document[0] = 0;
        copy(status_line, (int)sizeof(status_line), "Documento nuevo; escribe y guarda");
        return;
    }
    if (size > LIMIT) size = LIMIT;
    for (uint32_t i = 0; i < size; i++) document[i] = src[i];
    length = (int)size;
    document[length] = 0;
    cursor = length;
    copy(status_line, (int)sizeof(status_line), "Documento abierto");
    dirty = 1;
}
static void save(void) {
    document[length] = 0;
    if (os->write_file && os->write_file(path, document)) {
        copy(status_line, (int)sizeof(status_line), "Guardado correctamente");
        os->write("writer.elf: documento guardado\n");
    } else {
        copy(status_line, (int)sizeof(status_line), "Error al guardar");
        os->write("writer.elf: error al guardar\n");
    }
    dirty = 1;
}
static int line_start(int pos) {
    while (pos > 0 && document[pos - 1] != '\n') pos--;
    return pos;
}
static int line_end(int pos) {
    while (pos < length && document[pos] != '\n') pos++;
    return pos;
}
static void move_up(void) {
    int start = line_start(cursor);
    if (!start) { cursor = 0; return; }
    int column = cursor - start;
    int prev_end = start - 1;
    int prev_start = line_start(prev_end);
    cursor = prev_start + column;
    if (cursor > prev_end) cursor = prev_end;
}
static void move_down(void) {
    int start = line_start(cursor);
    int end = line_end(cursor);
    if (end >= length) return;
    int column = cursor - start;
    int next = end + 1;
    int next_end = line_end(next);
    cursor = next + column;
    if (cursor > next_end) cursor = next_end;
}
static void insert_char(int key) {
    if (length >= LIMIT) {
        copy(status_line, (int)sizeof(status_line), "Limite de 1023 bytes alcanzado");
        dirty = 1;
        return;
    }
    for (int i = length; i >= cursor; i--) document[i + 1] = document[i];
    document[cursor++] = (char)key;
    length++;
    document[length] = 0;
    dirty = 1;
}
static void backspace(void) {
    if (!cursor) return;
    int start = previous_char(cursor);
    int count = cursor - start;
    for (int i = start; i + count <= length; i++) document[i] = document[i + count];
    length -= count;
    cursor = start;
    document[length] = 0;
    dirty = 1;
}
static void delete_at(void) {
    if (cursor >= length) return;
    int end = next_char(cursor);
    int count = end - cursor;
    for (int i = cursor; i + count <= length; i++) document[i] = document[i + count];
    length -= count;
    document[length] = 0;
    dirty = 1;
}
static void draw(void) {
    os->clear(BG);
    os->text(18, 10, "ToolChain Writer", INK, 1);
    os->text(18, 38, "Editor de documentos de texto UTF-8", MUTED, 0);
    os->rect(16, 62, os->width() - 32, 32, PANEL);
    os->frame(16, 62, os->width() - 32, 32, BORDER);
    os->text(26, 72, path, BLUE, 0);
    int top = 104;
    int bottom = os->height() - 50;
    os->rect(16, top, os->width() - 32, bottom - top, PANEL);
    os->frame(16, top, os->width() - 32, bottom - top, BORDER);
    int x = 28;
    int y = top + 12;
    int cursor_x = x;
    int cursor_y = y;
    for (int i = 0; i < length;) {
        if (i == cursor) { cursor_x = x; cursor_y = y; }
        if (document[i] == '\n') { i++; x = 28; y += 19; continue; }
        int n = utf8_width((unsigned char)document[i]);
        if (n > length - i) n = 1;
        char glyph[5];
        for (int j = 0; j < n; j++) glyph[j] = document[i + j];
        glyph[n] = 0;
        if (x + 16 > os->width() - 25) { x = 28; y += 19; }
        os->text(x, y, glyph, INK, 0);
        x += os->text_width(glyph, 0);
        if (y > bottom - 18) break;
        i += n;
    }
    if (cursor == length) { cursor_x = x; cursor_y = y; }
    if (cursor_y >= top && cursor_y < bottom) os->rect(cursor_x, cursor_y + 15, 7, 2, BLUE);
    os->text(18, os->height() - 34, status_line, MUTED, 0);
    os->text(18, os->height() - 16, "Ctrl+S guardar | Flechas mover | Enter nueva linea | ESC guardar/salir", BLUE, 0);
    os->present();
}
int main(host_api_t *api) {
    if (!api || api->abi_version != HOST_ABI_VERSION) {
        if (api) api->write("writer.elf: ABI incompatible\n");
        return 1;
    }
    os = api;
    if (!api->window("Writer - ToolChainOS", 820, 540)) return 2;
    const char *launch = api->launch_file ? api->launch_file() : 0;
    load(launch && *launch ? launch : "/documento.txt");
    host_event_t event;
    while (api->pump(&event)) {
        if (event.type == HOST_EV_CLOSE) { save(); break; }
        if (event.type != HOST_EV_KEY) continue;
        int key = (int)event.key;
        if (key == 27) { save(); break; }
        if (key == 19) { save(); continue; }
        if (key == HOST_KEY_LEFT) { cursor = previous_char(cursor); dirty = 1; }
        else if (key == HOST_KEY_RIGHT) { cursor = next_char(cursor); dirty = 1; }
        else if (key == HOST_KEY_UP) { move_up(); dirty = 1; }
        else if (key == HOST_KEY_DOWN) { move_down(); dirty = 1; }
        else if (key == HOST_KEY_HOME) { cursor = line_start(cursor); dirty = 1; }
        else if (key == HOST_KEY_END) { cursor = line_end(cursor); dirty = 1; }
        else if (key == HOST_KEY_DELETE) delete_at();
        else if (key == '\b' || key == 127) backspace();
        else if (key == '\r' || key == '\n') insert_char('\n');
        else if (key == '\t') { insert_char(' '); insert_char(' '); insert_char(' '); insert_char(' '); }
        else if (key >= 32 && key <= 255) insert_char(key);
        if (dirty) { draw(); dirty = 0; }
    }
    return api->finish(0);
}
