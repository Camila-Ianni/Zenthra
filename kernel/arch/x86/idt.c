#include "idt.h"
#include "klib.h"
#include "vga.h"
#include "io.h"

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;
static isr_t interrupt_handlers[IDT_ENTRIES];

static void lidt_flush(uint32_t idt_ptr_addr) {
    __asm__ volatile ("lidt (%0)" : : "r"(idt_ptr_addr) : "memory");
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

/* Handler en C para excepciones y fallos de la CPU */
void isr_handler(struct registers regs) {
    if (interrupt_handlers[regs.int_no] != 0) {
        isr_t handler = interrupt_handlers[regs.int_no];
        handler(&regs);
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
        kprintf("\n[PANIC] EXCEPCION DE CPU NO CONTROLADA: Int %u, ErrCode: 0x%x\n", regs.int_no, regs.err_code);
        kprintf("EIP: 0x%x, CS: 0x%x, EFLAGS: 0x%x\n", regs.eip, regs.cs, regs.eflags);
        kprintf("Sistema detenido por proteccion de integridad.\n");
        __asm__ volatile ("cli; hlt");
    }
}

/* Handler en C para las interrupciones de hardware (IRQs gestionadas por PIC) */
void irq_handler(struct registers regs) {
    // Si la interrupción vino del PIC esclavo (IRQ 8 a 15), enviar EOI al esclavo
    if (regs.int_no >= 40) {
        outb(0xA0, 0x20);
    }
    // Enviar EOI al PIC maestro
    outb(0x20, 0x20);

    if (interrupt_handlers[regs.int_no] != 0) {
        isr_t handler = interrupt_handlers[regs.int_no];
        handler(&regs);
    }
}

/* Remapeo del PIC 8259 para mover IRQ 0..15 al rango 32..47 (evitar conflicto con excepciones x86) */
static void pic_remap(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    outb(0x21, 0x20); // Master PIC vector offset -> 32 (0x20)
    io_wait();
    outb(0xA1, 0x28); // Slave PIC vector offset -> 40 (0x28)
    io_wait();
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();
    outb(0x21, 0x00); // Máscara: habilitar todas las IRQs
    outb(0xA1, 0x00);
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base = (uint32_t)&idt;

    k_memset(&idt, 0, sizeof(struct idt_entry) * IDT_ENTRIES);
    k_memset(&interrupt_handlers, 0, sizeof(isr_t) * IDT_ENTRIES);

    pic_remap();
    lidt_flush((uint32_t)&idtp);
}
