#include "page_fault.h"
#include "vmm.h"
#include "klib.h"
#include "serial.h"
#include "vga.h"

static size_t pf_count = 0;
static uint32_t last_fault_addr = 0;
static uint32_t last_err_code = 0;

void page_fault_init(void) {
    register_interrupt_handler(14, page_fault_handler);
    serial_write_string("[VMM] Manejador de Page Fault (Vector 14 / ISR 14) registrado en IDT.\r\n");
}

void page_fault_handler(struct registers* regs) {
    if (!regs) return;

    pf_count++;
    last_fault_addr = vmm_get_fault_addr();
    last_err_code = regs->err_code;

    bool not_present = !(regs->err_code & PF_ERR_PRESENT);
    bool write_op    = (regs->err_code & PF_ERR_WRITE) != 0;
    bool user_mode   = (regs->err_code & PF_ERR_USER) != 0;
    bool reserved    = (regs->err_code & PF_ERR_RESERVED) != 0;
    bool fetch       = (regs->err_code & PF_ERR_FETCH) != 0;

    char buf[32];
    serial_write_string("\r\n======================================================\r\n");
    serial_write_string("[PAGE FAULT #PF] Violacion de memoria en direccion: 0x");
    uitoa(last_fault_addr, buf, 16);
    serial_write_string(buf);
    serial_write_string("\r\n  Causa: ");
    serial_write_string(not_present ? "Pagina no presente en memoria" : "Violacion de proteccion de permisos");
    serial_write_string("\r\n  Acceso: ");
    serial_write_string(write_op ? "Escritura" : "Lectura");
    serial_write_string(fetch ? " (Fetch de instruccion)" : "");
    serial_write_string("\r\n  Nivel CPU: ");
    serial_write_string(user_mode ? "Ring 3 (Espacio de Usuario)" : "Ring 0 (Supervisor / Kernel)");
    if (reserved) {
        serial_write_string("\r\n  Bits reservados alterados!");
    }
    serial_write_string("\r\n  EIP: 0x");
    uitoa(regs->eip, buf, 16);
    serial_write_string(buf);
    serial_write_string(" | ErrorCode: 0x");
    uitoa(regs->err_code, buf, 16);
    serial_write_string(buf);
    serial_write_string("\r\n======================================================\r\n");

    // Si el fallo ocurrió dentro de código de kernel, detener la máquina por seguridad
    if (!user_mode) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
        kprintf("\n[KERNEL PANIC] Page Fault en Ring 0 (Addr: 0x%x, EIP: 0x%x)\n", last_fault_addr, regs->eip);
#if defined(__KERNEL__)
        __asm__ volatile ("cli; hlt");
#endif
    }
}

size_t page_fault_get_count(void) {
    return pf_count;
}

uint32_t page_fault_get_last_address(void) {
    return last_fault_addr;
}

uint32_t page_fault_get_last_error_code(void) {
    return last_err_code;
}
