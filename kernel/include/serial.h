#ifndef SERIAL_H
#define SERIAL_H

#include "ktypes.h"

/* Puertos estándar UART */
#define COM1_PORT 0x3F8

int serial_init(void);
int serial_received(void);
char serial_read(void);
int serial_is_transmit_empty(void);
void serial_write_char(char a);
void serial_write_string(const char* str);
void serial_printf(const char* fmt, ...);

#endif
