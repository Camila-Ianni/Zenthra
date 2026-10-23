#include "syscall.h"
#include "vga.h"
#include "klib.h"
#include "nova_iot.h"
#include "acl_security.h"
#include "task.h"
#include "vmm.h"

/*
 * Validación de punteros de usuario:
 * Comprueba que el rango [ptr, ptr + size) esté dentro del espacio de usuario válido,
 * no sea NULL y no desborde el límite de memoria del kernel.
 */
bool is_user_pointer(const void* ptr, size_t size) {
    if (ptr == NULL) return false;
    uintptr_t addr = (uintptr_t)ptr;

    // Comprobar que la dirección base no exceda el límite de usuario
    if (addr >= USER_MEMORY_LIMIT) return false;

    // Comprobar desbordamiento aritmético y límite superior sin wrap-around
    if (size > (USER_MEMORY_LIMIT - addr)) return false;

    return true;
}

/*
 * Validación estricta de páginas mapeadas para punteros de usuario:
 * Comprueba no solo el límite lógico plano (USER_MEMORY_LIMIT), sino también
 * que todas las páginas del rango [ptr, ptr + size) estén presentes en el
 * directorio de páginas activo con permisos de usuario (PTE_USER) y, si corresponde,
 * de escritura (PTE_WRITABLE). Previene fallos de página (#PF) en Ring 0.
 */
static bool is_user_pages_accessible(const void* ptr, size_t size, bool write_intent) {
    if (!is_user_pointer(ptr, size)) return false;
    if (size == 0) return true;

    if (vmm_is_paging_enabled()) {
        struct page_directory* cur_pd = vmm_get_current_directory();
        if (!cur_pd) cur_pd = vmm_get_kernel_directory();

        uintptr_t start = (uintptr_t)ptr;
        uintptr_t end = start + size;
        uintptr_t start_page = start & PTE_FRAME_MASK;
        uintptr_t end_page = (end + PAGE_SIZE - 1) & PTE_FRAME_MASK;

        for (uintptr_t page = start_page; page < end_page; page += PAGE_SIZE) {
            uint32_t flags = vmm_get_page_flags(cur_pd, page);
            if (!(flags & PTE_PRESENT) || !(flags & PTE_USER)) {
                return false;
            }
            if (write_intent && !(flags & PTE_WRITABLE)) {
                return false;
            }
        }
    }
    return true;
}

/*
 * Copia segura desde userland hacia buffer acotado en kernel:
 * Retorna 0 si fue exitosa, o -1 si el puntero, rango o páginas son inválidos.
 */
int copy_from_user(void* kernel_dest, const void* user_src, size_t size) {
    if (!kernel_dest || !user_src) return -1;
    if (!is_user_pages_accessible(user_src, size, false)) return -1;

    k_memcpy(kernel_dest, user_src, size);
    return 0;
}

/*
 * Copia segura desde kernel hacia buffer en userland:
 * Retorna 0 si fue exitosa, o -1 si el destino de usuario o páginas son inválidos/de solo lectura.
 */
int copy_to_user(void* user_dest, const void* kernel_src, size_t size) {
    if (!user_dest || !kernel_src) return -1;
    if (!is_user_pages_accessible(user_dest, size, true)) return -1;

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
void syscall_dispatcher(struct registers* regs) {
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
            struct task* cur_task = task_get_current();
            enum nova_profile prof = cur_task ? cur_task->profile : acl_get_active_profile();

            if (cmd == 1) { // PING
                if (!acl_check_permission(prof, OP_ARDUINO_READ)) {
                    acl_log_violation(prof, OP_ARDUINO_READ, "SYS_ARDUINO_CMD_PING");
                    regs->eax = (uint32_t)-13; // -EACCES
                    break;
                }
                regs->eax = nova_iot_ping() ? 0 : 1;
            } else if (cmd == 2) { // LED ON
                if (!acl_check_permission(prof, OP_ARDUINO_WRITE)) {
                    acl_log_violation(prof, OP_ARDUINO_WRITE, "SYS_ARDUINO_CMD_LED_ON");
                    regs->eax = (uint32_t)-13; // -EACCES
                    break;
                }
                regs->eax = nova_iot_set_led(true) ? 0 : 1;
            } else if (cmd == 3) { // LED OFF
                if (!acl_check_permission(prof, OP_ARDUINO_WRITE)) {
                    acl_log_violation(prof, OP_ARDUINO_WRITE, "SYS_ARDUINO_CMD_LED_OFF");
                    regs->eax = (uint32_t)-13; // -EACCES
                    break;
                }
                regs->eax = nova_iot_set_led(false) ? 0 : 1;
            } else if (cmd == 4) { // GET TEMP
                if (!acl_check_permission(prof, OP_ARDUINO_READ)) {
                    acl_log_violation(prof, OP_ARDUINO_READ, "SYS_ARDUINO_CMD_GET_TEMP");
                    regs->eax = (uint32_t)-13; // -EACCES
                    break;
                }
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

        case SYS_GET_PROFILE: {
            struct task* cur = task_get_current();
            regs->eax = (uint32_t)(cur ? cur->profile : acl_get_active_profile());
            break;
        }

        case SYS_SET_PROFILE: {
            uint32_t new_prof = regs->ebx;
            struct task* cur = task_get_current();
            enum nova_profile curr_prof = cur ? cur->profile : acl_get_active_profile();
            if (!acl_check_permission(curr_prof, OP_SYSTEM_ADMIN)) {
                acl_log_violation(curr_prof, OP_SYSTEM_ADMIN, "SYS_SET_PROFILE");
                regs->eax = (uint32_t)-13; // -EACCES
                break;
            }
            if (new_prof < PROFILE_COUNT) {
                if (cur) {
                    cur->profile = (enum nova_profile)new_prof;
                }
                acl_set_active_profile((enum nova_profile)new_prof);
                regs->eax = 0;
            } else {
                regs->eax = (uint32_t)-1;
            }
            break;
        }

        default: {
            regs->eax = (uint32_t)-1;
            break;
        }
    }
}

#if defined(__KERNEL__)
extern void isr128(void);
#endif

void syscall_init(void) {
    // Interrupción de software 0x80 (128 decimal) accesible desde Ring 3 (DPL=3 / 0xEE)
    // Se utiliza el stub isr128 que construye el marco completo struct registers y retorna con IRET
#if defined(__KERNEL__)
    idt_set_gate(0x80, (uint32_t)(uintptr_t)isr128, 0x08, 0xEE);
#else
    idt_set_gate(0x80, (uint32_t)(uintptr_t)syscall_dispatcher, 0x08, 0xEE);
#endif
    register_interrupt_handler(0x80, syscall_dispatcher);
}
