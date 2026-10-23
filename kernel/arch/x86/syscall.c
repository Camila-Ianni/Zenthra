#include "syscall.h"
#include "vga.h"
#include "klib.h"
#include "nova_iot.h"
#include "acl_security.h"
#include "task.h"
#include "vmm.h"
#include "keyboard.h"
#include "errno.h"

/*
 * Validación de punteros de usuario:
 * Comprueba que el rango [ptr, ptr + size) esté dentro del espacio de usuario válido,
 * no sea NULL y no desborde el límite de memoria del kernel ni wrap-around aritmético.
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
 * Validación estricta de páginas mapeadas para punteros de usuario (Auditoría v15):
 * Comprueba que todas las páginas del rango [ptr, ptr + size) estén presentes en el
 * directorio de páginas activo con permisos de usuario (PTE_USER) y, si corresponde,
 * de escritura (PTE_WRITABLE).
 *
 * POLÍTICA FAIL-CLOSED (Auditoría v15):
 * Si la paginación está activa y no existe un directorio de páginas de proceso válido
 * o es inaccesible, se RECHAZA de inmediato retornando false en lugar de validar contra
 * el espacio del kernel por defecto.
 */
bool is_user_pages_accessible_failclosed(const void* ptr, size_t size, bool write_intent) {
    if (!is_user_pointer(ptr, size)) return false;
    if (size == 0) return true;

    if (vmm_is_paging_enabled()) {
        struct page_directory* cur_pd = vmm_get_current_directory();
        // Fallar cerrado: si no hay un directorio de páginas activo o es inaccesible, rechazar
        if (!cur_pd || !vmm_is_directory_accessible(cur_pd)) {
            return false;
        }

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

/* Helper interno compatible */
static inline bool is_user_pages_accessible(const void* ptr, size_t size, bool write_intent) {
    return is_user_pages_accessible_failclosed(ptr, size, write_intent);
}

/*
 * Copia segura desde userland hacia buffer acotado en kernel:
 * Retorna 0 si fue exitosa, o -1 si el puntero, rango o páginas son inválidos.
 */
int copy_from_user(void* kernel_dest, const void* user_src, size_t size) {
    if (!kernel_dest || !user_src) return -1;
    if (!is_user_pages_accessible_failclosed(user_src, size, false)) return -1;

    k_memcpy(kernel_dest, user_src, size);
    return 0;
}

/*
 * Copia segura desde kernel hacia buffer en userland:
 * Retorna 0 si fue exitosa, o -1 si el destino de usuario o páginas son inválidos/de solo lectura.
 */
int copy_to_user(void* user_dest, const void* kernel_src, size_t size) {
    if (!user_dest || !kernel_src) return -1;
    if (!is_user_pages_accessible_failclosed(user_dest, size, true)) return -1;

    k_memcpy(user_dest, kernel_src, size);
    return 0;
}

/*
 * Calcula la longitud de una cadena de usuario de forma segura hasta max_len.
 *
 * Contrato de seguridad y prevención de fallos (#PF en Ring 0) - Auditoría v14/v15:
 * Valida bloque por bloque de página en el directorio de páginas activo que cada dirección
 * pertenezca a userland (USER_MEMORY_LIMIT), esté presente (PTE_PRESENT) y cuente con
 * permisos de usuario (PTE_USER). Si la cadena se extiende hacia una página no mapeada
 * o sin PTE_USER antes de encontrar un terminador NUL, aborta de forma segura retornando -1.
 */
ssize_t strnlen_user(const char* user_str, size_t max_len) {
    if (!user_str || max_len == 0) return -1;
    if (!is_user_pointer(user_str, 1)) return -1;

    size_t inspected = 0;
    while (inspected < max_len) {
        const char* cur = user_str + inspected;
        if (!is_user_pointer(cur, 1)) {
            return -1;
        }

        // Validar que la página que contiene 'cur' sea accesible con permisos de usuario
        if (!is_user_pages_accessible_failclosed(cur, 1, false)) {
            return -1; // Página inaccesible: abortar de forma segura sin desreferenciar
        }

        // Determinar cuántos bytes restan en la página actual antes del siguiente límite de 4 KiB
        uintptr_t cur_addr = (uintptr_t)cur;
        size_t bytes_left_in_page = PAGE_SIZE - (cur_addr & (PAGE_SIZE - 1));
        size_t chunk_len = max_len - inspected;
        if (chunk_len > bytes_left_in_page) {
            chunk_len = bytes_left_in_page;
        }

        // Inspeccionar bytes dentro del rango validado de la página
        for (size_t j = 0; j < chunk_len; j++) {
            if (cur[j] == '\0') {
                return (ssize_t)(inspected + j);
            }
        }

        inspected += chunk_len;
    }

    return -1; // Terminador NUL no encontrado dentro de max_len
}

/* Buffer acotado seguro en el kernel para transferencias de syscalls */
#define KERNEL_SYSCALL_BUF_SIZE 256
static char kernel_safe_buf[KERNEL_SYSCALL_BUF_SIZE];

/* Handler de Syscalls invocado por la interrupción 0x80 (Ring 3 -> Ring 0) */
void syscall_dispatcher(struct registers* regs) {
    uint32_t syscall_num = regs->eax;

    switch (syscall_num) {
        case SYS_WRITE: {
            // ABI de Syscall Zenthra OS (Auditoría v14/v15):
            //   EBX: const void* user_buf (puntero a búfer de usuario)
            //   ECX: size_t count (número exacto de bytes a transferir)
            // Retorno en EAX:
            //   count == 0  -> 0 (éxito nulo estándar POSIX)
            //   éxito       -> número de bytes transferidos/escritos (> 0)
            //   error       -> -EFAULT (-14 ante puntero nulo, páginas no mapeadas, sin PTE_USER o fuera de rango)
            const char* user_buf = (const char*)(uintptr_t)regs->ebx;
            size_t count = (size_t)regs->ecx;

            if (count == 0) {
                regs->eax = 0;
                break;
            }

            // Validar puntero y desbordamiento en espacio de usuario
            if (!user_buf || !is_user_pointer(user_buf, count)) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            // Validar accesibilidad fail-closed de todas las páginas en [user_buf, user_buf + count)
            if (!is_user_pages_accessible_failclosed(user_buf, count, false)) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            size_t total_written = 0;
            while (total_written < count) {
                size_t chunk = count - total_written;
                if (chunk > KERNEL_SYSCALL_BUF_SIZE) {
                    chunk = KERNEL_SYSCALL_BUF_SIZE;
                }

                if (copy_from_user(kernel_safe_buf, user_buf + total_written, chunk) != 0) {
                    regs->eax = (total_written > 0) ? (uint32_t)total_written : (uint32_t)-EFAULT;
                    break;
                }

                for (size_t i = 0; i < chunk; i++) {
                    vga_putc(kernel_safe_buf[i]);
                }
                total_written += chunk;
            }

            regs->eax = (uint32_t)total_written;
            break;
        }

        case SYS_READ: {
            // ABI de Syscall Zenthra OS (Auditoría v15):
            //   EBX: void* user_buf (puntero a búfer de destino en usuario)
            //   ECX: size_t count (número de bytes a leer)
            //   EDX: uint32_t flags (SYS_READ_NONBLOCK o SYS_READ_BLOCKING)
            // Retorno en EAX:
            //   count == 0  -> 0 (éxito nulo estándar POSIX)
            //   error       -> -EFAULT (-14) ante puntero nulo, fuera de rango o sin PTE_WRITABLE
            //               -> -EAGAIN (-11) si no hay datos en la cola de entrada (lectura interactiva no bloqueante)
            //   éxito       -> número de bytes leídos (> 0)
            void* user_buf = (void*)(uintptr_t)regs->ebx;
            size_t count = (size_t)regs->ecx;

            if (count == 0) {
                regs->eax = 0;
                break;
            }

            if (!user_buf || !is_user_pointer(user_buf, count)) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            // Validación crítica previa:
            // Exigir que todas las páginas de destino estén presentes, pertenezcan a usuario
            // y tengan permiso de escritura (write_intent = true) ANTES de extraer cualquier byte
            // de la cola de entrada, garantizando que no se pierdan pulsaciones de teclas si el búfer es inválido.
            if (!is_user_pages_accessible_failclosed(user_buf, count, true)) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            // Consultar hardware físico si corresponde
            keyboard_poll_hardware();

            // Comprobar si hay datos disponibles en la cola de teclado/terminal
            if (keyboard_queue_is_empty()) {
                // En semántica interactiva no bloqueante, retornar -EAGAIN (-11), no 0 (0 es EOF en POSIX)
                regs->eax = (uint32_t)-EAGAIN;
                break;
            }

            size_t to_read = count;
            if (to_read > KERNEL_SYSCALL_BUF_SIZE) {
                to_read = KERNEL_SYSCALL_BUF_SIZE;
            }
            size_t q_len = keyboard_queue_length();
            if (to_read > q_len) {
                to_read = q_len;
            }

            // Extraer de la cola al buffer intermedio seguro del kernel
            for (size_t i = 0; i < to_read; i++) {
                kernel_safe_buf[i] = keyboard_dequeue_char();
            }

            // Copiar los datos leídos hacia el espacio de usuario
            if (copy_to_user(user_buf, kernel_safe_buf, to_read) != 0) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            regs->eax = (uint32_t)to_read;
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
                    regs->eax = (uint32_t)-EACCES; // -13
                    break;
                }
                regs->eax = nova_iot_ping() ? 0 : 1;
            } else if (cmd == 2) { // LED ON
                if (!acl_check_permission(prof, OP_ARDUINO_WRITE)) {
                    acl_log_violation(prof, OP_ARDUINO_WRITE, "SYS_ARDUINO_CMD_LED_ON");
                    regs->eax = (uint32_t)-EACCES; // -13
                    break;
                }
                regs->eax = nova_iot_set_led(true) ? 0 : 1;
            } else if (cmd == 3) { // LED OFF
                if (!acl_check_permission(prof, OP_ARDUINO_WRITE)) {
                    acl_log_violation(prof, OP_ARDUINO_WRITE, "SYS_ARDUINO_CMD_LED_OFF");
                    regs->eax = (uint32_t)-EACCES; // -13
                    break;
                }
                regs->eax = nova_iot_set_led(false) ? 0 : 1;
            } else if (cmd == 4) { // GET TEMP
                if (!acl_check_permission(prof, OP_ARDUINO_READ)) {
                    acl_log_violation(prof, OP_ARDUINO_READ, "SYS_ARDUINO_CMD_GET_TEMP");
                    regs->eax = (uint32_t)-EACCES; // -13
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
                        regs->eax = (uint32_t)-EFAULT; // -14
                    }
                } else {
                    regs->eax = (uint32_t)-EIO; // Error físico de comunicación (-5)
                }
            } else {
                regs->eax = (uint32_t)-EINVAL; // -22 Comando no permitido
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
                regs->eax = (uint32_t)-EACCES; // -13
                break;
            }
            if (new_prof < PROFILE_COUNT) {
                if (cur) {
                    cur->profile = (enum nova_profile)new_prof;
                }
                acl_set_active_profile((enum nova_profile)new_prof);
                regs->eax = 0;
            } else {
                regs->eax = (uint32_t)-EINVAL; // -22
            }
            break;
        }

        default: {
            regs->eax = (uint32_t)-ENOSYS; // -38
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
