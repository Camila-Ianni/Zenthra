#include "idt.h"
#include "klib.h"
#include "vga.h"
#include "serial.h"
#include "io.h"

#define IDT_ENTRIES 256
#define KERNEL_CS   0x08

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;
static isr_t interrupt_handlers[IDT_ENTRIES];

/* Stubs externos definidos en assembly (interrupts.s) */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static void lidt_flush(uint32_t idt_ptr_addr) {
#if defined(__KERNEL__)
    __asm__ volatile ("lidt (%0)" : : "r"(idt_ptr_addr) : "memory");
#else
    (void)idt_ptr_addr;
#endif
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
void isr_handler(struct registers* regs) {
    if (!regs) return;

    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
        kprintf("\n================================================================================\n");
        kprintf(" [PANIC DE CPU] Excepcion no controlada Vector: %u (0x%x) | ErrorCode: 0x%x\n", 
                regs->int_no, regs->int_no, regs->err_code);
        kprintf(" EIP: 0x%x | CS: 0x%x | EFLAGS: 0x%x\n", regs->eip, regs->cs, regs->eflags);
        kprintf(" Sistema detenido de emergencia para salvaguardar la memoria.\n");
        kprintf("================================================================================\n");

        serial_write_string("\r\n[CPU PANIC] Excepcion no controlada Int: ");
        char buf[16];
        uitoa(regs->int_no, buf, 10);
        serial_write_string(buf);
        serial_write_string(" ErrCode: 0x");
        uitoa(regs->err_code, buf, 16);
        serial_write_string(buf);
        serial_write_string(" EIP: 0x");
        uitoa(regs->eip, buf, 16);
        serial_write_string(buf);
        serial_write_string("\r\n");

#if defined(__KERNEL__)
        __asm__ volatile ("cli; hlt");
#endif
    }
}

/* Handler en C para las interrupciones de hardware (IRQs gestionadas por PIC) */
void irq_handler(struct registers* regs) {
    if (!regs) return;

#if defined(__KERNEL__)
    // Si la interrupción vino del PIC esclavo (IRQ 8 a 15, vector >= 40), enviar EOI al esclavo
    if (regs->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    // Enviar EOI al PIC maestro
    outb(0x20, 0x20);
#endif

    if (interrupt_handlers[regs->int_no] != 0) {
        isr_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
}

/* Remapeo del PIC 8259 para mover IRQ 0..15 al rango 32..47 (evitar conflicto con excepciones x86) */
static void pic_remap(void) {
#if defined(__KERNEL__)
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
#endif
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base = (uint32_t)&idt;

    k_memset(&idt, 0, sizeof(struct idt_entry) * IDT_ENTRIES);
    k_memset(&interrupt_handlers, 0, sizeof(isr_t) * IDT_ENTRIES);

    pic_remap();

    /* Instalar puertas para todas las excepciones de CPU (0..31) */
    idt_set_gate(0,  (uint32_t)isr0,  KERNEL_CS, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  KERNEL_CS, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  KERNEL_CS, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  KERNEL_CS, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  KERNEL_CS, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  KERNEL_CS, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  KERNEL_CS, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  KERNEL_CS, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  KERNEL_CS, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  KERNEL_CS, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, KERNEL_CS, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, KERNEL_CS, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, KERNEL_CS, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, KERNEL_CS, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, KERNEL_CS, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, KERNEL_CS, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, KERNEL_CS, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, KERNEL_CS, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, KERNEL_CS, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, KERNEL_CS, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, KERNEL_CS, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, KERNEL_CS, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, KERNEL_CS, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, KERNEL_CS, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, KERNEL_CS, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, KERNEL_CS, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, KERNEL_CS, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, KERNEL_CS, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, KERNEL_CS, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, KERNEL_CS, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, KERNEL_CS, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, KERNEL_CS, 0x8E);

    /* Instalar puertas para las interrupciones de hardware PIC (IRQ 0..15 -> Vectores 32..47) */
    idt_set_gate(32, (uint32_t)irq0,  KERNEL_CS, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  KERNEL_CS, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  KERNEL_CS, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  KERNEL_CS, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  KERNEL_CS, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  KERNEL_CS, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  KERNEL_CS, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  KERNEL_CS, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  KERNEL_CS, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  KERNEL_CS, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, KERNEL_CS, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, KERNEL_CS, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, KERNEL_CS, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, KERNEL_CS, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, KERNEL_CS, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, KERNEL_CS, 0x8E);

    lidt_flush((uint32_t)&idtp);
}
