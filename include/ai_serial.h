#ifndef AI_SERIAL_H
#define AI_SERIAL_H
#include <stddef.h>
void ai_init(void);
int ai_poll(void);
void ai_command(const char *arg);
/* Obtiene una pagina HTTPS via el puente anfitrion COM2; texto acotado. */
int ai_web_fetch(const char *url,char *out,size_t capacity);
/* Agente de bajo nivel: la IA ejecuta ordenes del shell por COM2 y recibe
 * la salida real de cada una. Usa TC/1 tipos 4/5 y 0x84/0x85. */
void ai_agent_start(const char *goal);
int ai_agent_active(void);
/* Hay una peticion IA o una sesion de agente en curso. */
int ai_busy(void);
#endif
