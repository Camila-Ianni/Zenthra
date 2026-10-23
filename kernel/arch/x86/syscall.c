#include "syscall.h"
#include "vga.h"
#include "klib.h"
#include "nova_iot.h"
#include "acl_security.h"
#include "task.h"
#include "vmm.h"
#include "page_fault.h"
#include "keyboard.h"
#include "errno.h"

/*
 * En el binario real del kernel (__KERNEL__), se exige obligatoriamente que la paginación
 * por hardware (CR0.PG) esté activa para permitir cualquier acceso a memoria de usuario.
 * En pruebas de host sin VMM inicializado, puede configurarse explícitamente para probar
 * tanto el camino de kernel estricto como los mocks básicos de punteros.
 */
#if defined(__KERNEL__)
static bool strict_require_paging = true;
#else
static bool strict_require_paging = false;
#endif

void syscall_set_require_paging(bool require_paging) {
    strict_require_paging = require_paging;
}

bool syscall_is_paging_required(void) {
    return strict_require_paging;
}

/*
 * Validación aritmética de punteros de usuario:
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
 * Validación estricta de páginas mapeadas para punteros de usuario (Auditoría v15..v18):
 * Comprueba que todas las páginas del rango [ptr, ptr + size) estén presentes en el
 * directorio de páginas activo con permisos de usuario (PTE_USER) y, si corresponde,
 * de escritura (PTE_WRITABLE).
 *
 * POLÍTICA FAIL-CLOSED (Auditoría v18):
 * 1. En modo kernel real (o cuando strict_require_paging == true), si la paginación
 *    NO está activa, se RECHAZA inmediatamente retornando false (nunca se permite acceso
 *    de usuario sin protección de paginación activa).
 * 2. Si no existe un directorio de páginas de proceso válido o es inaccesible, se
 *    RECHAZA de inmediato retornando false.
 */
bool is_user_pages_accessible_failclosed(const void* ptr, size_t size, bool write_intent) {
    if (!is_user_pointer(ptr, size)) return false;
    if (size == 0) return true;

    if (!vmm_is_paging_enabled()) {
        if (strict_require_paging) {
            return false; // Camino real de kernel: rechazar si paginación está desactivada
        }
        return true; // Solo permitido en tests unitarios tempranos de host antes de vmm_init()
    }

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
    return true;
}

/*
 * Copia segura con tabla de excepción real (Exception Table / Fixup) desde userland hacia kernel (Auditoría v19):
 * 1. Bloquea el planificador (scheduler_lock) para estabilizar el espacio de direcciones durante la validación y copia.
 * 2. Valida todas las páginas del rango en el directorio activo (is_user_pages_accessible_failclosed).
 * 3. Apila un contexto reentrante (page_fault_push_user_copy_ctx) que vincula estrictamente el rango de direcciones
 *    de usuario [user_src, user_src + size) y el rango de instrucciones [__user_copy_fault_start, __user_copy_fault_end).
 * 4. Ejecuta la copia mediante arch_user_copy_bytes: si ocurre un #PF de hardware dentro de la instrucción,
 *    page_fault_handler redirige regs->eip = __user_copy_fault_fixup y arch_user_copy_bytes retorna -1 (-EFAULT)
 *    de forma inmediata sin bucle de excepciones ni pánico.
 */
int copy_from_user(void* kernel_dest, const void* user_src, size_t size) {
    if (!kernel_dest || !user_src) return -1;
    if (size == 0) return 0;

    scheduler_lock();
    if (!is_user_pages_accessible_failclosed(user_src, size, false)) {
        scheduler_unlock();
        return -1;
    }

    if (!page_fault_push_user_copy_ctx((uintptr_t)user_src, size,
                                       page_fault_get_copy_eip_start(),
                                       page_fault_get_copy_eip_end(),
                                       page_fault_get_copy_fixup_eip())) {
        scheduler_unlock();
        return -1;
    }

    int rc = arch_user_copy_bytes(kernel_dest, user_src, size);
    bool faulted = page_fault_pop_user_copy_ctx();
    scheduler_unlock();

    return (rc != 0 || faulted) ? -1 : 0;
}

/*
 * Copia segura con tabla de excepción real (Exception Table / Fixup) desde kernel hacia userland (Auditoría v19):
 * Retorna 0 si fue exitosa, o -1 si el destino de usuario o páginas son inválidos/de solo lectura o hubo #PF con fixup.
 */
int copy_to_user(void* user_dest, const void* kernel_src, size_t size) {
    if (!user_dest || !kernel_src) return -1;
    if (size == 0) return 0;

    scheduler_lock();
    if (!is_user_pages_accessible_failclosed(user_dest, size, true)) {
        scheduler_unlock();
        return -1;
    }

    if (!page_fault_push_user_copy_ctx((uintptr_t)user_dest, size,
                                       page_fault_get_copy_eip_start(),
                                       page_fault_get_copy_eip_end(),
                                       page_fault_get_copy_fixup_eip())) {
        scheduler_unlock();
        return -1;
    }

    int rc = arch_user_copy_bytes(user_dest, kernel_src, size);
    bool faulted = page_fault_pop_user_copy_ctx();
    scheduler_unlock();

    return (rc != 0 || faulted) ? -1 : 0;
}

/*
 * Primitiva de verificación de hardware (Auditoría v19):
 * Ejecuta arch_user_copy_bytes directamente sobre un rango de usuario registrado en la
 * tabla de excepción SIN el filtro previo de páginas por software, permitiendo provocar un
 * Page Fault (#PF Vector 14) real en la CPU en QEMU (página no mapeada o página de solo lectura
 * con CR0.WP=1) y demostrar que page_fault_handler redirige regs->eip a __user_copy_fault_fixup
 * retornando -1 (-EFAULT) sin bucle infinito ni Kernel Panic.
 */
int copy_from_user_fixup_hw_test(void* kernel_dest, const void* user_src, size_t size) {
    if (!kernel_dest || !user_src || size == 0) return -1;
    if (!is_user_pointer(user_src, size)) return -1;

    scheduler_lock();
    if (!page_fault_push_user_copy_ctx((uintptr_t)user_src, size,
                                       page_fault_get_copy_eip_start(),
                                       page_fault_get_copy_eip_end(),
                                       page_fault_get_copy_fixup_eip())) {
        scheduler_unlock();
        return -1;
    }

    int rc = arch_user_copy_bytes(kernel_dest, user_src, size);
    bool faulted = page_fault_pop_user_copy_ctx();
    scheduler_unlock();

    return (rc != 0 || faulted) ? -1 : 0;
}

int copy_to_user_fixup_hw_test(void* user_dest, const void* kernel_src, size_t size) {
    if (!user_dest || !kernel_src || size == 0) return -1;
    if (!is_user_pointer(user_dest, size)) return -1;

    scheduler_lock();
    if (!page_fault_push_user_copy_ctx((uintptr_t)user_dest, size,
                                       page_fault_get_copy_eip_start(),
                                       page_fault_get_copy_eip_end(),
                                       page_fault_get_copy_fixup_eip())) {
        scheduler_unlock();
        return -1;
    }

    int rc = arch_user_copy_bytes(user_dest, kernel_src, size);
    bool faulted = page_fault_pop_user_copy_ctx();
    scheduler_unlock();

    return (rc != 0 || faulted) ? -1 : 0;
}

/*
 * Calcula la longitud de una cadena de usuario de forma segura hasta max_len (Auditoría v14..v19).
 *
 * Contrato de seguridad y prevención de fallos (#PF en Ring 0):
 * Mantiene estable el espacio de direcciones con scheduler_lock(), valida bloque por bloque de página
 * en el directorio activo y copia cada bloque validado mediante arch_user_copy_bytes protegido por
 * page_fault_push_user_copy_ctx() con redirección real de EIP ante cualquier #PF inesperado.
 */
ssize_t strnlen_user(const char* user_str, size_t max_len) {
    if (!user_str || max_len == 0) return -1;
    if (!is_user_pointer(user_str, 1)) return -1;

    scheduler_lock();
    size_t inspected = 0;
    char kpage_chunk[64];

    while (inspected < max_len) {
        const char* cur = user_str + inspected;
        if (!is_user_pointer(cur, 1)) {
            scheduler_unlock();
            return -1;
        }

        // Determinar cuántos bytes restan en la página actual antes del siguiente límite de 4 KiB
        uintptr_t cur_addr = (uintptr_t)cur;
        size_t bytes_left_in_page = PAGE_SIZE - (cur_addr & (PAGE_SIZE - 1));
        size_t chunk_len = max_len - inspected;
        if (chunk_len > bytes_left_in_page) {
            chunk_len = bytes_left_in_page;
        }
        if (chunk_len > sizeof(kpage_chunk)) {
            chunk_len = sizeof(kpage_chunk);
        }

        // Validar que el bloque dentro de la página sea accesible con permisos de usuario
        if (!is_user_pages_accessible_failclosed(cur, chunk_len, false)) {
            scheduler_unlock();
            return -1; // Página inaccesible: abortar de forma segura sin desreferenciar
        }

        if (!page_fault_push_user_copy_ctx((uintptr_t)cur, chunk_len,
                                           page_fault_get_copy_eip_start(),
                                           page_fault_get_copy_eip_end(),
                                           page_fault_get_copy_fixup_eip())) {
            scheduler_unlock();
            return -1;
        }

        int rc = arch_user_copy_bytes(kpage_chunk, cur, chunk_len);
        bool faulted = page_fault_pop_user_copy_ctx();
        if (rc != 0 || faulted) {
            scheduler_unlock();
            return -1;
        }

        // Inspeccionar bytes en el buffer local del kernel
        for (size_t j = 0; j < chunk_len; j++) {
            if (kpage_chunk[j] == '\0') {
                scheduler_unlock();
                return (ssize_t)(inspected + j);
            }
        }

        inspected += chunk_len;
    }

    scheduler_unlock();
    return -1; // Terminador NUL no encontrado dentro de max_len
}

/* Tamaño de chunk para transferencias acotadas en la pila de kernel */
#define KERNEL_SYSCALL_BUF_SIZE 256
#define MAX_SYSCALL_TRANSFER_SIZE 65536

/* Handler de Syscalls invocado por la interrupción 0x80 (Ring 3 -> Ring 0) */
void syscall_dispatcher(struct registers* regs) {
    uint32_t syscall_num = regs->eax;

    switch (syscall_num) {
        case SYS_WRITE: {
            // ABI de Syscall Zenthra OS (Auditoría v14..v19):
            //   EBX: const void* user_buf (puntero a búfer de usuario)
            //   ECX: size_t count (número de bytes solicitados a escribir)
            // Contrato de transferencia y escritura parcial (Auditoría v18/v19):
            //   - count == 0 retorna 0 (éxito nulo estándar POSIX).
            //   - Cada llamada acota count a un máximo de MAX_SYSCALL_TRANSFER_SIZE (65536 bytes).
            //   - No se realiza doble validación global previa: cada bloque de 256 bytes ('chunk')
            //     es validado y copiado una única vez por copy_from_user().
            //   - Si el primer chunk falla, retorna -EFAULT (-14).
            //   - Si algún chunk posterior falla tras haber transferido N > 0 bytes válidos,
            //     retorna exactamente N (escritura parcial estándar POSIX).
            const char* user_buf = (const char*)(uintptr_t)regs->ebx;
            size_t count = (size_t)regs->ecx;

            if (count == 0) {
                regs->eax = 0;
                break;
            }

            if (!user_buf) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            if (count > MAX_SYSCALL_TRANSFER_SIZE) {
                count = MAX_SYSCALL_TRANSFER_SIZE;
            }

            char kbuf_write[KERNEL_SYSCALL_BUF_SIZE];
            size_t total_written = 0;
            while (total_written < count) {
                size_t chunk = count - total_written;
                if (chunk > sizeof(kbuf_write)) {
                    chunk = sizeof(kbuf_write);
                }

                // Validación única por chunk dentro de copy_from_user (aritmética + páginas + fixup)
                if (copy_from_user(kbuf_write, user_buf + total_written, chunk) != 0) {
                    regs->eax = (total_written > 0) ? (uint32_t)total_written : (uint32_t)-EFAULT;
                    break;
                }

                for (size_t i = 0; i < chunk; i++) {
                    vga_putc(kbuf_write[i]);
                }
                total_written += chunk;
            }

            if (total_written > 0) {
                regs->eax = (uint32_t)total_written;
            }
            break;
        }

        case SYS_READ: {
            // ABI de Syscall Zenthra OS (Auditoría v15..v19):
            //   EBX: void* user_buf (puntero a búfer de destino en usuario)
            //   ECX: size_t count (número máximo de bytes solicitados)
            //   EDX: uint32_t flags (SYS_READ_NONBLOCK o SYS_READ_BLOCKING)
            //
            // Lectura Transaccional y Validación Exacta (Auditoría v17..v19):
            // 1. Se comprueba que user_buf no sea NULL y que su primer byte sea escribible antes de
            //    devolver -EAGAIN cuando la cola está vacía (preservando la prioridad POSIX -EFAULT sobre -EAGAIN).
            // 2. Cuando existen 'peeked' bytes disponibles en la cola, copy_to_user(user_buf, kbuf_read, peeked)
            //    es la validación autoritativa que verifica el rango exacto [user_buf, user_buf + peeked)
            //    y realiza la copia bajo protección de fixup antes de invocar keyboard_discard_chars(peeked).
            void* user_buf = (void*)(uintptr_t)regs->ebx;
            size_t count = (size_t)regs->ecx;
            uint32_t flags = (uint32_t)regs->edx;

            if (flags & ~(SYS_READ_NONBLOCK | SYS_READ_BLOCKING)) {
                regs->eax = (uint32_t)-EINVAL;
                break;
            }

            if (flags & SYS_READ_BLOCKING) {
                regs->eax = (uint32_t)-ENOSYS;
                break;
            }

            if (count == 0) {
                regs->eax = 0;
                break;
            }

            if (!user_buf) {
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            scheduler_lock();
            keyboard_poll_hardware();

            if (keyboard_queue_is_empty()) {
                // Si la cola está vacía, verificar accesibilidad de la base de user_buf para retornar -EFAULT si el puntero es inválido
                bool base_ok = is_user_pages_accessible_failclosed(user_buf, 1, true);
                scheduler_unlock();
                regs->eax = base_ok ? (uint32_t)-EAGAIN : (uint32_t)-EFAULT;
                break;
            }

            size_t q_len = keyboard_queue_length();
            size_t to_read = count;
            if (to_read > q_len) {
                to_read = q_len;
            }
            if (to_read > KERNEL_SYSCALL_BUF_SIZE) {
                to_read = KERNEL_SYSCALL_BUF_SIZE;
            }

            char kbuf_read[KERNEL_SYSCALL_BUF_SIZE];
            size_t peeked = keyboard_peek_queue(kbuf_read, to_read);
            if (peeked == 0) {
                scheduler_unlock();
                regs->eax = (uint32_t)-EAGAIN;
                break;
            }

            // copy_to_user es la validación autoritativa para los 'peeked' bytes exactos y ejecuta bajo fixup
            if (copy_to_user(user_buf, kbuf_read, peeked) != 0) {
                // Si la copia falla, la cola permanece 100% intacta
                scheduler_unlock();
                regs->eax = (uint32_t)-EFAULT;
                break;
            }

            // Confirmación transaccional dentro de la misma sección crítica sincronizada
            keyboard_discard_chars(peeked);
            scheduler_unlock();

            regs->eax = (uint32_t)peeked;
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
            // Auditoría v16..v18: Aislamiento de perfiles y prevención de escalamiento de privilegios.
            // 1. Validar que el perfil destino exista (< PROFILE_COUNT); de lo contrario retornar -EINVAL.
            // 2. Comprobar que la tarea actual posea OP_SYSTEM_ADMIN y que la transición cumpla la política
            //    de monotonicidad de capacidades (acl_can_transition_profile): ninguna tarea puede autoelevarse
            //    a un perfil que otorgue capacidades adicionales (ej. OP_ARDUINO_WRITE o OP_ARDUINO_READ).
            //    Así se impide que perfiles de software con OP_SYSTEM_ADMIN (Backend, Frontend, Fullstack,
            //    Ciberseguridad) escalen a PROFILE_IOT_HARDWARE y tomen control de actuadores físicos.
            uint32_t new_prof = regs->ebx;
            if (new_prof >= PROFILE_COUNT) {
                regs->eax = (uint32_t)-EINVAL; // -22
                break;
            }

            struct task* cur = task_get_current();
            enum nova_profile curr_prof = cur ? cur->profile : acl_get_active_profile();

            if (!acl_check_permission(curr_prof, OP_SYSTEM_ADMIN)) {
                acl_log_violation(curr_prof, OP_SYSTEM_ADMIN, "SYS_SET_PROFILE");
                regs->eax = (uint32_t)-EACCES; // -13
                break;
            }

            if (!acl_can_transition_profile(curr_prof, (enum nova_profile)new_prof)) {
                // Registrar intento de escalamiento de privilegios hacia perfil con capacidades adicionales
                uint8_t added_caps = (uint8_t)(acl_get_profile_mask((enum nova_profile)new_prof) & ~acl_get_profile_mask(curr_prof));
                enum sec_operation violated_op = (added_caps & OP_ARDUINO_WRITE) ? OP_ARDUINO_WRITE :
                                                 ((added_caps & OP_ARDUINO_READ) ? OP_ARDUINO_READ : OP_SYSTEM_ADMIN);
                acl_log_violation(curr_prof, violated_op, "SYS_SET_PROFILE_ESCALATION_BLOCKED");
                regs->eax = (uint32_t)-EACCES; // -13
                break;
            }

            if (cur) {
                cur->profile = (enum nova_profile)new_prof;
            }
            regs->eax = 0;
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
#if defined(__KERNEL__)
    idt_set_gate(0x80, (uint32_t)(uintptr_t)isr128, 0x08, 0xEE);
#else
    idt_set_gate(0x80, (uint32_t)(uintptr_t)syscall_dispatcher, 0x08, 0xEE);
#endif
    register_interrupt_handler(0x80, syscall_dispatcher);
}
