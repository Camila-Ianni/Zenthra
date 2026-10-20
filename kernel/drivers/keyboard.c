#include "keyboard.h"
#include "io.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Tabla básica de scan codes a caracteres ASCII para teclado US estándar */
static const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, /* Right Shift */
    '*', 0, /* Alt */
    ' ', /* Space */
    0, /* Caps lock */
};

void keyboard_init(void) {
    // Vaciar el buffer del teclado si hubiera datos pendientes
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
    }
}

char keyboard_getchar_nonblocking(void) {
    if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        // Si el bit más significativo es 1, es key release (soltar tecla)
        if (!(scancode & 0x80)) {
            if (scancode < sizeof(scancode_ascii)) {
                return scancode_ascii[scancode];
            }
        }
    }
    return 0;
}

char keyboard_getchar(void) {
    char c = 0;
    while ((c = keyboard_getchar_nonblocking()) == 0);
    return c;
}
