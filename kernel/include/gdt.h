#ifndef GDT_H
#define GDT_H

#include "ktypes.h"

/* Estructura de una entrada en la GDT (8 bytes) */
struct gdt_entry {
    uint16_t limit_low;     // Los 16 bits inferiores del límite
    uint16_t base_low;      // Los 16 bits inferiores de la dirección base
    uint8_t  base_middle;   // Los siguientes 8 bits de la dirección base
    uint8_t  access;        // Flags de acceso (Presente, Privilegio DPL, Tipo)
    uint8_t  granularity;   // Granularidad y 4 bits superiores del límite
    uint8_t  base_high;     // Los 8 bits superiores de la dirección base
} __attribute__((packed));

/* Puntero que la CPU carga con la instrucción 'lgdt' */
struct gdt_ptr {
    uint16_t limit;         // Tamaño de la GDT - 1
    uint32_t base;          // Dirección inicial de la tabla GDT
} __attribute__((packed));

/* Estructura del Task State Segment (TSS) de 32 bits x86 */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;       // Puntero de pila del kernel cargado al conmutar de Ring 3 a Ring 0
    uint32_t ss0;        // Selector del segmento de pila del kernel (0x10)
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

void gdt_init(void);
void tss_set_kernel_stack(uint32_t esp0);

#endif
