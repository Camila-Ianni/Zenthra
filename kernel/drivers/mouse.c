#include "mouse.h"
#include "io.h"
#include "gfx.h"
#include "serial.h"

static struct mouse_state current_mouse;
static uint32_t screen_w = 1024;
static uint32_t screen_h = 768;

static uint8_t mouse_bytes[3];
static uint8_t mouse_cycle = 0;

/* Máscara de mapa de bits del cursor de flecha (12x18 píxeles) */
/* 0 = Transparente, 1 = Borde Negro, 2 = Relleno Blanco */
static const uint8_t cursor_bitmap[18][12] = {
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0 },
    { 1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0 },
    { 1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 },
    { 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0 },
    { 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0 },
    { 1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0 },
    { 1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 1, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static bool mouse_present = false;

#if defined(__KERNEL__)
static bool mouse_wait_input(void) {
    uint32_t timeout = 50000;
    while (timeout--) {
        if (inb(0x64) & 1) return true;
    }
    return false;
}

static bool mouse_wait_output(void) {
    uint32_t timeout = 50000;
    while (timeout--) {
        if (!(inb(0x64) & 2)) return true;
    }
    return false;
}

static bool mouse_write_byte(uint8_t val) {
    if (!mouse_wait_output()) return false;
    outb(0x64, 0xD4);
    if (!mouse_wait_output()) return false;
    outb(0x60, val);
    return true;
}

static int mouse_read_byte(void) {
    if (!mouse_wait_input()) return -1;
    return (int)inb(0x60);
}
#endif

bool mouse_is_present(void) {
    return mouse_present;
}

void mouse_init(uint32_t width, uint32_t height) {
    current_mouse.x = width > 0 ? (int32_t)(width / 2) : 512;
    current_mouse.y = height > 0 ? (int32_t)(height / 2) : 384;
    current_mouse.left_button = false;
    current_mouse.right_button = false;
    current_mouse.middle_button = false;

    screen_w = width > 0 ? width : 1024;
    screen_h = height > 0 ? height : 768;

    mouse_cycle = 0;
    mouse_present = false;

    // Registrar manejador de interrupción en la IDT (Vector 44 = IRQ 12)
    register_interrupt_handler(44, mouse_handler);

#if defined(__KERNEL__)
    // Comprobar presencia del controlador 8042
    if (!mouse_wait_output()) {
        serial_write_string("[MOUSE] Controlador 8042 no responde (timeout). Modo sin raton.\r\n");
        return;
    }
    outb(0x64, 0xA8); // Habilitar puerto auxiliar

    if (!mouse_wait_output()) return;
    outb(0x64, 0x20); // Leer byte de configuración
    if (!mouse_wait_input()) return;
    uint8_t status = inb(0x60);
    status |= 0x02;   // Habilitar IRQ 12
    status &= ~0x20;  // Deshabilitar desactivación de reloj del ratón

    if (!mouse_wait_output()) return;
    outb(0x64, 0x60); // Escribir byte de configuración
    if (!mouse_wait_output()) return;
    outb(0x60, status);

    // Set defaults (0xF6) y verificar ACK (0xFA)
    if (!mouse_write_byte(0xF6) || mouse_read_byte() != 0xFA) {
        serial_write_string("[MOUSE] Ratón auxiliar no devolvió ACK (0xF6). Modo sin ratón.\r\n");
        return;
    }

    // Enable data reporting (0xF4) y verificar ACK (0xFA)
    if (!mouse_write_byte(0xF4) || mouse_read_byte() != 0xFA) {
        serial_write_string("[MOUSE] Ratón auxiliar no devolvió ACK (0xF4). Modo sin ratón.\r\n");
        return;
    }
#endif

    mouse_present = true;

    // Habilitar IRQ 12 e IRQ 2 (cascada PIC Slave) sólo si el dispositivo está confirmado
    pic_clear_mask(2);
    pic_clear_mask(12);

    serial_write_string("[MOUSE] Driver PS/2 (IRQ 12) inicializado en modo streaming.\r\n");
}

void mouse_set_bounds(uint32_t width, uint32_t height) {
    screen_w = width > 0 ? width : 1024;
    screen_h = height > 0 ? height : 768;

    if (current_mouse.x >= (int32_t)screen_w) current_mouse.x = screen_w - 1;
    if (current_mouse.y >= (int32_t)screen_h) current_mouse.y = screen_h - 1;
}

void mouse_process_byte(uint8_t byte) {
    if (mouse_cycle == 0) {
        // En el protocolo PS/2 estándar, el bit 3 del primer byte siempre debe ser 1
        if (!(byte & 0x08)) {
            return; // Descartar byte fuera de sincronía
        }
        mouse_bytes[0] = byte;
        mouse_cycle = 1;
    } else if (mouse_cycle == 1) {
        mouse_bytes[1] = byte;
        mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
        mouse_bytes[2] = byte;
        mouse_cycle = 0;

        // Decodificación de botones
        current_mouse.left_button   = (mouse_bytes[0] & 0x01) != 0;
        current_mouse.right_button  = (mouse_bytes[0] & 0x02) != 0;
        current_mouse.middle_button = (mouse_bytes[0] & 0x04) != 0;

        // Desplazamiento en X con extensión de signo
        int32_t dx = (int32_t)mouse_bytes[1];
        if (mouse_bytes[0] & 0x10) {
            dx |= 0xFFFFFF00;
        }

        // Desplazamiento en Y con extensión de signo
        int32_t dy = (int32_t)mouse_bytes[2];
        if (mouse_bytes[0] & 0x20) {
            dy |= 0xFFFFFF00;
        }

        // Actualizar coordenadas (Y se invierte para coincidir con el espacio de pantalla de arriba hacia abajo)
        current_mouse.x += dx;
        current_mouse.y -= dy;

        // Delimitación estricta a los bordes de la pantalla
        if (current_mouse.x < 0) current_mouse.x = 0;
        if ((uint32_t)current_mouse.x >= screen_w) current_mouse.x = screen_w - 1;

        if (current_mouse.y < 0) current_mouse.y = 0;
        if ((uint32_t)current_mouse.y >= screen_h) current_mouse.y = screen_h - 1;
    }
}

void mouse_handler(struct registers* regs) {
    (void)regs;
#if defined(__KERNEL__)
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t mouse_in = inb(0x60);
        mouse_process_byte(mouse_in);
    }
#endif
}

struct mouse_state* mouse_get_state(void) {
    return &current_mouse;
}

void mouse_draw_cursor(void) {
    if (!gfx_is_active() || !mouse_present) return;

    int32_t mx = current_mouse.x;
    int32_t my = current_mouse.y;

    for (int row = 0; row < 18; row++) {
        for (int col = 0; col < 12; col++) {
            uint8_t pixel_type = cursor_bitmap[row][col];
            if (pixel_type == 1) {
                gfx_putpixel((uint32_t)(mx + col), (uint32_t)(my + row), COLOR_BLACK);
            } else if (pixel_type == 2) {
                gfx_putpixel((uint32_t)(mx + col), (uint32_t)(my + row), COLOR_WHITE);
            }
        }
    }
}
