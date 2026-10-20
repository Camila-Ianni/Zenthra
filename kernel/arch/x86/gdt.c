#include "gdt.h"

#define GDT_ENTRIES 5

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gp;

/* Función en ensamblador inline para cargar la GDT y recargar selectores */
static void gdt_flush(uint32_t gdt_ptr_addr) {
    __asm__ volatile (
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $.reload_cs\n"
        ".reload_cs:\n"
        :
        : "r"(gdt_ptr_addr)
        : "eax", "memory"
    );
}

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base = (uint32_t)&gdt;

    // 0: Descriptor nulo requerido por x86
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Segmento de Código Kernel (Ring 0): Base 0, Límite 4GB, Acceso 0x9A, Granularidad 0xCF (4KB, 32-bit)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Segmento de Datos Kernel (Ring 0): Base 0, Límite 4GB, Acceso 0x92, Granularidad 0xCF
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 3: Segmento de Código Usuario (Ring 3): Acceso 0xFA (DPL=3)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 4: Segmento de Datos Usuario (Ring 3): Acceso 0xF2 (DPL=3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // Cargar la nueva GDT en la CPU
    gdt_flush((uint32_t)&gp);
}
