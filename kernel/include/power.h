#ifndef POWER_H
#define POWER_H

#include "ktypes.h"
#include "io.h"

/* Apagado de máquina virtual mediante puertos ACPI (QEMU, VirtualBox, Bochs) */
static inline void qemu_poweroff(void) {
    // QEMU PIIX4 (i440fx) ACPI Poweroff Port
    outw(0x604, 0x2000);

    // QEMU Q35 ACPI Poweroff Port
    outw(0x0804, 0x3400);

    // Bochs / Old QEMU ACPI Poweroff Port
    outw(0xB004, 0x2000);

    // VirtualBox ACPI Poweroff Port
    outw(0x4004, 0x2000);

    // QEMU debug exit device (-device isa-debug-exit)
    outb(0x501, 0x00);
}

/* Reinicio de la máquina mediante el controlador de teclado 8042 y fallback */
static inline void cpu_reboot(void) {
    // Esperar a que el buffer del controlador 8042 esté libre
    uint8_t status;
    for (int i = 0; i < 1000; i++) {
        status = inb(0x64);
        if ((status & 2) == 0) break;
    }
    // Pulsar línea de reset de la CPU (pin CPU RESET conectado al pin 0 del puerto de salida del 8042)
    outb(0x64, 0xFE);

#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    // Fallback: Triple fallo deliberado si el controlador no responde
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) null_idt = { 0, 0 };
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idt));
#endif
}

#endif
