#include "syscall.h"
#include "vga.h"
#include "klib.h"
#include "nova_iot.h"

/* Handler de Syscalls invocado por la interrupción 0x80 (Ring 3 -> Ring 0) */
static void syscall_dispatcher(struct registers* regs) {
    uint32_t syscall_num = regs->eax;

    switch (syscall_num) {
        case SYS_WRITE: {
            // ebx: puntero a buffer de texto, ecx: longitud
            const char* str = (const char*)regs->ebx;
            size_t len = (size_t)regs->ecx;
            if (str != NULL) {
                for (size_t i = 0; i < len; i++) {
                    vga_putc(str[i]);
                }
                regs->eax = len; // Retorna cantidad de caracteres escritos
            } else {
                regs->eax = (uint32_t)-1;
            }
            break;
        }

        case SYS_ARDUINO_CMD: {
            // ebx: comando (1: ping, 2: led on, 3: led off, 4: get temp)
            // ecx: puntero de salida (para temperatura o datos)
            uint32_t cmd = regs->ebx;
            if (cmd == 1) {
                regs->eax = nova_iot_ping() ? 0 : 1;
            } else if (cmd == 2) {
                regs->eax = nova_iot_set_led(true) ? 0 : 1;
            } else if (cmd == 3) {
                regs->eax = nova_iot_set_led(false) ? 0 : 1;
            } else if (cmd == 4) {
                uint16_t* out = (uint16_t*)regs->ecx;
                regs->eax = nova_iot_get_temp(out) ? 0 : 1;
            } else {
                regs->eax = (uint32_t)-1; // Comando no permitido
            }
            break;
        }

        default: {
            regs->eax = (uint32_t)-1;
            break;
        }
    }
}

void syscall_init(void) {
    // Interrupción de software 0x80 (128 decimal) accesible desde Ring 3 (DPL=3)
    idt_set_gate(0x80, (uint32_t)syscall_dispatcher, 0x08, 0xEE);
    register_interrupt_handler(0x80, syscall_dispatcher);
}
