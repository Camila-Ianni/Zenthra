#ifndef IO_H
#define IO_H

#include "ktypes.h"

/* Escribir un byte a un puerto I/O */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Leer un byte de un puerto I/O */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Esperar un ciclo I/O (para sincronización de hardware lento) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
