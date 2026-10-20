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

void gdt_init(void);

#endif
