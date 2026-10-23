#ifndef IO_H
#define IO_H

#include "ktypes.h"

#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
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

/* Escribir una palabra (16 bits) a un puerto I/O */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Leer una palabra (16 bits) de un puerto I/O */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
#else
/* Stubs seguros para entornos host de pruebas (evita fallos de privilegio en userspace Linux CI) */
static inline void outb(uint16_t port, uint8_t val) { (void)port; (void)val; }
static inline uint8_t inb(uint16_t port) { (void)port; return 0; }
static inline void outw(uint16_t port, uint16_t val) { (void)port; (void)val; }
static inline uint16_t inw(uint16_t port) { (void)port; return 0; }
#endif

/* Esperar un ciclo I/O (para sincronización de hardware lento) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
