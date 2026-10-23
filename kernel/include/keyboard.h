#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "ktypes.h"

#define KEYBOARD_QUEUE_CAPACITY 128

void keyboard_init(void);
char keyboard_getchar_nonblocking(void);
char keyboard_getchar(void);

/* Gestión de cola circular de teclado y terminal (Auditoría v15) */
bool keyboard_enqueue_char(char c);
char keyboard_dequeue_char(void);
bool keyboard_queue_is_empty(void);
size_t keyboard_queue_length(void);
void keyboard_queue_clear(void);
ssize_t keyboard_read_queue(char* dest, size_t count);
void keyboard_poll_hardware(void);

#endif

