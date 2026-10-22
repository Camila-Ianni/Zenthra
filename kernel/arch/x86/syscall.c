#include "syscall.h"
#include "vga.h"
#include "klib.h"
#include "nova_iot.h"

/*
 * Validación de punteros de usuario:
 * Comprueba que el rango [ptr, ptr + size) esté dentro del espacio de usuario válido,
 * no sea NULL y no desborde el límite de 32 bits ni invada el espacio reservado del kernel.
 */
bool is_user_pointer(const void* ptr, size_t size) {
    if (ptr == NULL) return false;
    uintptr_t addr = (uintptr_t)ptr;

    // Comprobar desbordamiento aritmético
    if (addr + size < addr) return false;

    // Comprobar que no acceda al espacio de memoria protegido del kernel
    if (addr + size > USER_MEMORY_LIMIT) return false;

    return true;
}

/*
 * Copia segura desde userland hacia buffer acotado en kernel:
 * Retorna 0 si fue exitosa, o -1 si el puntero o rango es inválido.
 */
int copy_from_user(void* kernel_dest, const void* user_src, size_t size) {
    if (!kernel_dest || !user_src) return -1;
    if (!is_user_pointer(user_src, size)) return -1;

    k_memcpy(kernel_dest, user_src, size);
    return 0;
}

/*
 * Copia segura desde kernel hacia buffer en userland:
 * Retorna 0 si fue exitosa, o -1 si la dirección de destino de usuario es inválida.
 */
int copy_to_user(void* user_dest, const void* kernel_src, size_t size) {
    if (!user_dest || !kernel_src) return -1;
    if (!is_user_pointer(user_dest, size)) return -1;

    k_memcpy(user_dest, kernel_src, size);
    return 0;
}

/*
 * Calcula la longitud de una cadena de usuario de forma segura hasta max_len.
 * Evita desbordamiento si la cadena no tiene terminador NUL dentro del rango.
 * Retorna la longitud en bytes (sin incluir el NUL), o -1 si no se encontró NUL dentro de max_len.
 */
ssize_t strnlen_user(const char* user_str, size_t max_len) {
    if (!user_str) return -1;
    if (!is_user_pointer(user_str, 1)) return -1;

    for (size_t i = 0; i < max_len; i++) {
        if (!is_user_pointer(user_str + i, 1)) return -1;
        if (user_str[i] == '\0') {
            return (ssize_t)i;
        }
    }
    return -1; // Cadena sin terminador NUL dentro de max_len
}

/* Buffer acotado seguro en el kernel para transferencias de syscalls */
#define KERNEL_SYSCALL_BUF_SIZE 256
static char kernel_safe_buf[KERNEL_SYSCALL_BUF_SIZE];

/* Handler de Syscalls invocado por la interrupción 0x80 (Ring 3 -> Ring 0) */
static void syscall_dispatcher(struct registers* regs) {
    uint32_t syscall_num = regs->eax;

    switch (syscall_num) {
        case SYS_WRITE: {
            // ebx: puntero userland a cadena, ecx: longitud solicitada
            const char* user_str = (const char*)(uintptr_t)regs->ebx;
            size_t requested_len = (size_t)regs->ecx;

            if (requested_len == 0 || requested_len >= KERNEL_SYSCALL_BUF_SIZE) {
                regs->eax = (uint32_t)-1;
                break;
            }

            // Validar de forma segura la cadena antes de acceder a ella en Ring 0
            ssize_t safe_len = strnlen_user(user_str, requested_len + 1);
            if (safe_len < 0) {
                regs->eax = (uint32_t)-1; // Cadena malformada, sin NUL o puntero fuera de límites
                break;
            }

            // Copia segura al buffer interno del kernel
            if (copy_from_user(kernel_safe_buf, user_str, (size_t)safe_len) != 0) {
                regs->eax = (uint32_t)-1;
                break;
            }

            kernel_safe_buf[safe_len] = '\0';
            for (size_t i = 0; i < (size_t)safe_len; i++) {
                vga_putc(kernel_safe_buf[i]);
            }
            regs->eax = (uint32_t)safe_len;
            break;
        }

        case SYS_ARDUINO_CMD: {
            // ebx: comando (1: ping, 2: led on, 3: led off, 4: get temp)
            // ecx: puntero en userland para almacenar el resultado
            uint32_t cmd = regs->ebx;
            if (cmd == 1) {
                regs->eax = nova_iot_ping() ? 0 : 1;
            } else if (cmd == 2) {
                regs->eax = nova_iot_set_led(true) ? 0 : 1;
            } else if (cmd == 3) {
                regs->eax = nova_iot_set_led(false) ? 0 : 1;
            } else if (cmd == 4) {
                void* user_out_ptr = (void*)(uintptr_t)regs->ecx;
                uint16_t kernel_temp = 0;
                bool ok = nova_iot_get_temp(&kernel_temp);
                if (ok) {
                    // Escribir el resultado a userland mediante copia validada
                    if (copy_to_user(user_out_ptr, &kernel_temp, sizeof(uint16_t)) == 0) {
                        regs->eax = 0;
                    } else {
                        regs->eax = (uint32_t)-1; // Dirección de memoria de usuario no válida
                    }
                } else {
                    regs->eax = 1; // Error físico de comunicación
                }
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
    idt_set_gate(0x80, (uint32_t)(uintptr_t)syscall_dispatcher, 0x08, 0xEE);
    register_interrupt_handler(0x80, syscall_dispatcher);
}
