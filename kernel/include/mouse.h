#ifndef MOUSE_H
#define MOUSE_H

#include "ktypes.h"
#include "idt.h"

struct mouse_state {
    int32_t x;
    int32_t y;
    bool left_button;
    bool right_button;
    bool middle_button;
};

/* Inicializar el controlador de ratón PS/2 (8042) e interrupción IRQ 12 */
void mouse_init(uint32_t screen_width, uint32_t screen_height);

/* Procesar un byte recibido del puerto 0x60 (utilizable en pruebas y en IRQ) */
void mouse_process_byte(uint8_t byte);

/* Manejador de interrupción IRQ 12 (Vector 44) */
void mouse_handler(struct registers* regs);

/* Obtener estado actual del puntero y botones */
struct mouse_state* mouse_get_state(void);

/* Actualizar límites de pantalla para el cursor */
void mouse_set_bounds(uint32_t width, uint32_t height);

/* Dibujar el cursor gráfico de flecha sobre el backbuffer */
void mouse_draw_cursor(void);

/* Consultar si el hardware de ratón auxiliar PS/2 fue detectado y está activo */
bool mouse_is_present(void);

#endif
