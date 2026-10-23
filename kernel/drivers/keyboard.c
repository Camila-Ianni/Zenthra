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

/* Buffer circular para cola de entrada de teclado y terminal (Auditoría v15) */
static char keyboard_queue[KEYBOARD_QUEUE_CAPACITY];
static size_t q_head = 0;
static size_t q_tail = 0;
static size_t q_count = 0;

void keyboard_queue_clear(void) {
    q_head = 0;
    q_tail = 0;
    q_count = 0;
}

bool keyboard_queue_is_empty(void) {
    return q_count == 0;
}

size_t keyboard_queue_length(void) {
    return q_count;
}

bool keyboard_enqueue_char(char c) {
    if (c == 0) return false;
    if (q_count >= KEYBOARD_QUEUE_CAPACITY) {
        return false; // Cola saturada: prevenir desbordamiento y descartar carácter
    }
    keyboard_queue[q_tail] = c;
    q_tail = (q_tail + 1) % KEYBOARD_QUEUE_CAPACITY;
    q_count++;
    return true;
}

char keyboard_dequeue_char(void) {
    if (q_count == 0) return 0;
    char c = keyboard_queue[q_head];
    q_head = (q_head + 1) % KEYBOARD_QUEUE_CAPACITY;
    q_count--;
    return c;
}

ssize_t keyboard_read_queue(char* dest, size_t count) {
    if (!dest || count == 0) return 0;
    size_t read_bytes = 0;
    while (read_bytes < count && q_count > 0) {
        dest[read_bytes++] = keyboard_dequeue_char();
    }
    return (ssize_t)read_bytes;
}

void keyboard_poll_hardware(void) {
#if defined(__KERNEL__)
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        // Si el bit más significativo es 0, es key press (pulsación de tecla)
        if (!(scancode & 0x80)) {
            if (scancode < sizeof(scancode_ascii)) {
                char c = scancode_ascii[scancode];
                if (c) {
                    keyboard_enqueue_char(c);
                }
            }
        }
    }
#endif
}

void keyboard_init(void) {
    keyboard_queue_clear();
#if defined(__KERNEL__)
    // Vaciar el buffer del controlador 8042 si hubiera datos pendientes
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
    }
#endif
}

char keyboard_getchar_nonblocking(void) {
    keyboard_poll_hardware();
    if (!keyboard_queue_is_empty()) {
        return keyboard_dequeue_char();
    }
    return 0;
}

char keyboard_getchar(void) {
    char c = 0;
    while ((c = keyboard_getchar_nonblocking()) == 0);
    return c;
}
