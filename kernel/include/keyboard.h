#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "ktypes.h"

#define KEYBOARD_QUEUE_CAPACITY 128

enum keyboard_layout {
    KEYBOARD_LAYOUT_US = 0,       /* US QWERTY estándar */
    KEYBOARD_LAYOUT_ES = 1        /* Español Básico (ASCII Set 1; sin dead-keys ni AltGr extendido) */
};

void keyboard_init(void);
char keyboard_getchar_nonblocking(void);
char keyboard_getchar(void);

/* Gestión de cola circular de teclado y terminal (Auditoría v15/v16) */
bool keyboard_enqueue_char(char c);
char keyboard_dequeue_char(void);
bool keyboard_queue_is_empty(void);
size_t keyboard_queue_length(void);
void keyboard_queue_clear(void);
ssize_t keyboard_read_queue(char* dest, size_t count);
size_t keyboard_peek_queue(char* dest, size_t count);
void keyboard_discard_chars(size_t count);
void keyboard_poll_hardware(void);

/* Modificadores, layouts y telemetría de cola (Auditoría v16) */
void keyboard_set_layout(enum keyboard_layout layout);
enum keyboard_layout keyboard_get_layout(void);
size_t keyboard_get_overflow_count(void);
void keyboard_reset_overflow_count(void);
bool keyboard_is_shift_active(void);
bool keyboard_is_caps_active(void);
void keyboard_process_scancode(uint8_t scancode);

#endif
