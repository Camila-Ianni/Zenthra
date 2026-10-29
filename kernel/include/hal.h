#ifndef HAL_H
#define HAL_H

#include "ktypes.h"

/*
 * Hardware Abstraction Layer (HAL) para Zenthra OS
 * Basado en las directrices de 'operating-system-kernel-jserv-mini-arm-os'
 * Aísla las operaciones de CPU y puertos I/O para portabilidad de arquitecturas.
 */

/* Códigos canónicos de diagnóstico POST en puerto I/O 0x80 */
#define POST_BSS_CLEARED    0x01
#define POST_GDT_OK         0x02
#define POST_IDT_OK         0x03
#define POST_PIT_OK         0x04
#define POST_PMM_OK         0x05
#define POST_VMM_OK         0x06
#define POST_KHEAP_OK       0x07
#define POST_SCHEDULER_OK   0x08
#define POST_VFS_MOUNT_OK   0x09
#define POST_VESA_GUI_OK    0x10

static inline void hal_outb(uint16_t port, uint8_t val) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

static inline uint8_t hal_inb(uint16_t port) {
#if defined(__i386__) || defined(__x86_64__)
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#else
    (void)port;
    return 0;
#endif
}

static inline void hal_outw(uint16_t port, uint16_t val) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

static inline uint16_t hal_inw(uint16_t port) {
#if defined(__i386__) || defined(__x86_64__)
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#else
    (void)port;
    return 0;
#endif
}

static inline void hal_post_code(uint8_t code) {
    hal_outb(0x80, code);
}

static inline void hal_disable_interrupts(void) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("cli" ::: "memory");
#endif
}

static inline void hal_enable_interrupts(void) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("sti" ::: "memory");
#endif
}

static inline void hal_cpu_halt(void) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("hlt");
#endif
}

/* Verificación de CPUID x86 mediante conmuta del bit 21 en EFLAGS */
static inline bool hal_has_cpuid(void) {
#if defined(__i386__) || defined(__x86_64__)
    uint32_t original_flags, modified_flags;
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        "movl %0, %1\n\t"
        "xorl $0x00200000, %1\n\t"
        "pushl %1\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %1\n\t"
        "pushl %0\n\t"
        "popfl\n\t"
        : "=r"(original_flags), "=r"(modified_flags)
        :
        : "cc"
    );
    return ((original_flags ^ modified_flags) & 0x00200000) != 0;
#else
    return true;
#endif
}

#endif
