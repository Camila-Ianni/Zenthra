#include "gdt.h"
#include "klib.h"

#define GDT_ENTRIES 6

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gp;
static struct tss_entry tss;

/* Función en ensamblador inline para cargar la GDT y recargar selectores */
static void gdt_flush(uint32_t gdt_ptr_addr) {
#if defined(__KERNEL__)
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
#else
    (void)gdt_ptr_addr;
#endif
}

/* Cargar el Task Register (TR) con el selector del TSS (GDT Entry 5: 0x28) */
static void tss_flush(void) {
#if defined(__KERNEL__)
    // 0x28 | 3 = 0x2B (RPL 3 para permitir acceso a TSS)
    __asm__ volatile ("ltr %%ax" : : "a"(0x2B));
#endif
}

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base = (uint32_t)&gdt;

    // 0: Descriptor nulo requerido por arquitectura x86
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Segmento de Código Kernel (Ring 0): Base 0, Límite 4GB, Acceso 0x9A, Granularidad 0xCF (4KB, 32-bit)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Segmento de Datos Kernel (Ring 0): Base 0, Límite 4GB, Acceso 0x92, Granularidad 0xCF
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 3: Segmento de Código Usuario (Ring 3): Acceso 0xFA (DPL=3)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 4: Segmento de Datos Usuario (Ring 3): Acceso 0xF2 (DPL=3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 5: Task State Segment (TSS) para conmutación de pila Ring 3 -> Ring 0 en Syscalls/IRQs
    k_memset(&tss, 0, sizeof(struct tss_entry));
    tss.ss0 = 0x10; // Kernel Data Segment
    tss.esp0 = 0;   // Actualizado por el scheduler con la pila de la tarea activa
    tss.iomap_base = sizeof(struct tss_entry);

    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_entry) - 1;
    // 0xE9: Presente=1, DPL=3, Type=9 (32-bit Available TSS), Granularidad=0 (bytes)
    gdt_set_gate(5, tss_base, tss_limit, 0xE9, 0x00);

    // Cargar la nueva GDT en la CPU y recargar el Task Register
    gdt_flush((uint32_t)&gp);
    tss_flush();
}
