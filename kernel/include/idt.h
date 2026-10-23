#ifndef IDT_H
#define IDT_H

#include "ktypes.h"

/* Estructura de una entrada (puerta) en la IDT (8 bytes) */
struct idt_entry {
    uint16_t base_low;      // 16 bits inferiores de la dirección del handler
    uint16_t sel;           // Selector de segmento de código del kernel
    uint8_t  always0;       // Siempre 0
    uint8_t  flags;         // Flags: Presente, DPL (Ring 0-3), Gate type (32-bit Interrupt Gate 0x8E)
    uint16_t base_high;     // 16 bits superiores de la dirección del handler
} __attribute__((packed));

/* Puntero que la CPU carga con la instrucción 'lidt' */
struct idt_ptr {
    uint16_t limit;         // Tamaño de la IDT - 1
    uint32_t base;          // Dirección base de la IDT
} __attribute__((packed));

/* Estructura del estado de registros guardados durante una interrupción */
struct registers {
    uint32_t ds;                                      // Selector del segmento de datos
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Pushed por pusha
    uint32_t int_no, err_code;                        // Número de interrupción y código de error
    uint32_t eip, cs, eflags, useresp, ss;            // Pushed por la CPU automáticamente
};

typedef void (*isr_t)(struct registers*);

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void register_interrupt_handler(uint8_t n, isr_t handler);
void isr_handler(struct registers* regs);
void irq_handler(struct registers* regs);
void pic_clear_mask(uint8_t irq);
void pic_set_mask(uint8_t irq);

#endif
