#include "keyboard.h"
#include "io.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Scancodes de teclas especiales (Set 1 x86) */
#define SCANCODE_LSHIFT_MAKE   0x2A
#define SCANCODE_RSHIFT_MAKE   0x36
#define SCANCODE_LSHIFT_BREAK  0xAA
#define SCANCODE_RSHIFT_BREAK  0xB6
#define SCANCODE_CAPSLOCK_MAKE 0x3A

/* Tablas US Estándar */
static const char scancode_us_normal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   /* Left Shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,   /* Right Shift */
    '*', 0,   /* Alt */
    ' ', /* Space */
    0,   /* Caps lock */
};

static const char scancode_us_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   /* Ctrl */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    0,   /* Left Shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,   /* Right Shift */
    '*', 0,   /* Alt */
    ' ', /* Space */
    0,   /* Caps lock */
};

/* Tablas Distribución Español / Latinoamérica (Auditoría v16) */
static const char scancode_es_normal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', '?', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '`', '+', '\n',
    0,   /* Ctrl */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0,   '{', '}',
    0,   /* Left Shift */
    '<', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-',
    0,   /* Right Shift */
    '*', 0,   /* Alt */
    ' ', /* Space */
    0,   /* Caps lock */
};

static const char scancode_es_shift[128] = {
    0,   27,  '!', '\"', 0,   '$', '%', '&', '/', '(', ')', '=', '?', 0,   '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '^', '*', '\n',
    0,   /* Ctrl */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0,   '[', ']',
    0,   /* Left Shift */
    '>', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_',
    0,   /* Right Shift */
    '*', 0,   /* Alt */
    ' ', /* Space */
    0,   /* Caps lock */
};

/* Estado de modificadores, layout y telemetría de overflow (Auditoría v16) */
static bool shift_pressed = false;
static bool caps_lock_active = false;
static enum keyboard_layout active_layout = KEYBOARD_LAYOUT_US;
static size_t overflow_counter = 0;

/* Buffer circular para cola de entrada de teclado y terminal */
static char keyboard_queue[KEYBOARD_QUEUE_CAPACITY];
static size_t q_head = 0;
static size_t q_tail = 0;
static size_t q_count = 0;

void keyboard_set_layout(enum keyboard_layout layout) {
    active_layout = layout;
}

enum keyboard_layout keyboard_get_layout(void) {
    return active_layout;
}

size_t keyboard_get_overflow_count(void) {
    return overflow_counter;
}

void keyboard_reset_overflow_count(void) {
    overflow_counter = 0;
}

bool keyboard_is_shift_active(void) {
    return shift_pressed;
}

bool keyboard_is_caps_active(void) {
    return caps_lock_active;
}

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
        overflow_counter++; // Registrar evento de saturación para auditoría/telemetría
        return false;
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

/*
 * Inspección no destructiva de la cola de teclado (Lectura Transaccional - Auditoría v17).
 * Copia hasta 'count' caracteres disponibles hacia 'dest' sin avanzar la cabeza de la cola.
 */
size_t keyboard_peek_queue(char* dest, size_t count) {
    if (!dest || count == 0 || q_count == 0) return 0;
    size_t peeked = 0;
    size_t idx = q_head;
    while (peeked < count && peeked < q_count) {
        dest[peeked++] = keyboard_queue[idx];
        idx = (idx + 1) % KEYBOARD_QUEUE_CAPACITY;
    }
    return peeked;
}

/*
 * Descarta 'count' caracteres de la cola tras confirmación de copia exitosa a userland.
 */
void keyboard_discard_chars(size_t count) {
    if (count > q_count) {
        count = q_count;
    }
    q_head = (q_head + count) % KEYBOARD_QUEUE_CAPACITY;
    q_count -= count;
}

/* Procesamiento integral de scan codes con modificadores Shift y Caps Lock */
void keyboard_process_scancode(uint8_t scancode) {
    // 1. Detección de teclas modificadoras Make/Break
    if (scancode == SCANCODE_LSHIFT_MAKE || scancode == SCANCODE_RSHIFT_MAKE) {
        shift_pressed = true;
        return;
    }
    if (scancode == SCANCODE_LSHIFT_BREAK || scancode == SCANCODE_RSHIFT_BREAK) {
        shift_pressed = false;
        return;
    }
    if (scancode == SCANCODE_CAPSLOCK_MAKE) {
        caps_lock_active = !caps_lock_active;
        return;
    }

    // 2. Si el bit 7 está activo, es código de liberación (Break) de una tecla común: ignorar
    if (scancode & 0x80) {
        return;
    }

    if (scancode >= 128) {
        return;
    }

    // 3. Resolución de carácter según layout activo y modificadores
    char c = 0;
    const char* normal_tbl = (active_layout == KEYBOARD_LAYOUT_ES) ? scancode_es_normal : scancode_us_normal;
    const char* shift_tbl  = (active_layout == KEYBOARD_LAYOUT_ES) ? scancode_es_shift  : scancode_us_shift;

    char base_char = normal_tbl[scancode];
    if (base_char >= 'a' && base_char <= 'z') {
        // En letras, CapsLock y Shift actúan con XOR
        bool uppercase = (shift_pressed ^ caps_lock_active);
        c = uppercase ? (base_char - 'a' + 'A') : base_char;
    } else {
        // En números y símbolos, solo Shift altera el glifo
        c = shift_pressed ? shift_tbl[scancode] : normal_tbl[scancode];
    }

    if (c != 0) {
        keyboard_enqueue_char(c);
    }
}

void keyboard_poll_hardware(void) {
#if defined(__KERNEL__)
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        keyboard_process_scancode(scancode);
    }
#endif
}

void keyboard_init(void) {
    keyboard_queue_clear();
    shift_pressed = false;
    caps_lock_active = false;
    overflow_counter = 0;
    active_layout = KEYBOARD_LAYOUT_US;
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
