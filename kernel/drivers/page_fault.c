#include "page_fault.h"
#include "vmm.h"
#include "klib.h"
#include "serial.h"
#include "vga.h"

static size_t pf_count = 0;
static size_t pf_fixup_count = 0;
static uint32_t last_fault_addr = 0;
static uint32_t last_err_code = 0;

/* Pila reentrante de contextos de recuperación de copia de usuario (Auditoría v19) */
static struct user_copy_fixup_ctx copy_ctx_stack[USER_COPY_CTX_MAX_DEPTH];
static size_t copy_ctx_depth = 0;
static bool last_popped_faulted = false;

#if defined(__KERNEL__)
extern uintptr_t arch_get_user_copy_fault_start(void);
extern uintptr_t arch_get_user_copy_fault_end(void);
extern uintptr_t arch_get_user_copy_fault_fixup(void);

uintptr_t page_fault_get_copy_eip_start(void) {
    return arch_get_user_copy_fault_start();
}

uintptr_t page_fault_get_copy_eip_end(void) {
    return arch_get_user_copy_fault_end();
}

uintptr_t page_fault_get_copy_fixup_eip(void) {
    return arch_get_user_copy_fault_fixup();
}
#else
/* Marcadores deterministas para pruebas de redirección de EIP en host */
#define HOST_MOCK_COPY_EIP_START  ((uintptr_t)0x0010A000UL)
#define HOST_MOCK_COPY_EIP_END    ((uintptr_t)0x0010A010UL)
#define HOST_MOCK_COPY_FIXUP_EIP  ((uintptr_t)0x0010A020UL)

uintptr_t page_fault_get_copy_eip_start(void) {
    return HOST_MOCK_COPY_EIP_START;
}

uintptr_t page_fault_get_copy_eip_end(void) {
    return HOST_MOCK_COPY_EIP_END;
}

uintptr_t page_fault_get_copy_fixup_eip(void) {
    return HOST_MOCK_COPY_FIXUP_EIP;
}

int arch_user_copy_bytes(void* dst, const void* src, size_t size) {
    if (!dst || !src) return -1;
    k_memcpy(dst, src, size);
    return page_fault_did_user_copy_fault() ? -1 : 0;
}
#endif

bool page_fault_push_user_copy_ctx(uintptr_t user_start, size_t size,
                                   uintptr_t eip_start, uintptr_t eip_end, uintptr_t fixup_eip) {
    if (copy_ctx_depth >= USER_COPY_CTX_MAX_DEPTH) {
        return false; // Rechazar desbordamiento de anidamiento de contextos
    }
    if (fixup_eip == 0 || eip_end <= eip_start) {
        return false;
    }
    uintptr_t user_end = user_start;
    if (size > 0) {
        if (user_start > (UINTPTR_MAX - size)) {
            return false;
        }
        user_end = user_start + size;
    }

    struct user_copy_fixup_ctx* ctx = &copy_ctx_stack[copy_ctx_depth++];
    ctx->user_start = user_start;
    ctx->user_end   = user_end;
    ctx->eip_start  = eip_start;
    ctx->eip_end    = eip_end;
    ctx->fixup_eip  = fixup_eip;
    ctx->faulted    = false;
    return true;
}

bool page_fault_pop_user_copy_ctx(void) {
    if (copy_ctx_depth == 0) {
        last_popped_faulted = false;
        return false;
    }
    copy_ctx_depth--;
    last_popped_faulted = copy_ctx_stack[copy_ctx_depth].faulted;
    return last_popped_faulted;
}

size_t page_fault_get_user_copy_depth(void) {
    return copy_ctx_depth;
}

void page_fault_begin_user_copy_range(uintptr_t user_addr, size_t size) {
    (void)page_fault_push_user_copy_ctx(
        user_addr,
        size,
        page_fault_get_copy_eip_start(),
        page_fault_get_copy_eip_end(),
        page_fault_get_copy_fixup_eip()
    );
}

void page_fault_begin_user_copy(void) {
    page_fault_begin_user_copy_range(0x1000, 0xBFFFF000UL);
}

void page_fault_end_user_copy(void) {
    (void)page_fault_pop_user_copy_ctx();
}

bool page_fault_is_user_copy_active(void) {
    return copy_ctx_depth > 0;
}

bool page_fault_did_user_copy_fault(void) {
    if (copy_ctx_depth > 0) {
        return copy_ctx_stack[copy_ctx_depth - 1].faulted;
    }
    return last_popped_faulted;
}

void page_fault_init(void) {
    pf_count = 0;
    pf_fixup_count = 0;
    copy_ctx_depth = 0;
    last_popped_faulted = false;
    register_interrupt_handler(14, page_fault_handler);
    serial_write_string("[VMM] Manejador de Page Fault (Vector 14 / ISR 14) registrado en IDT.\r\n");
}

void page_fault_handler(struct registers* regs) {
    if (!regs) return;

    pf_count++;
    last_fault_addr = vmm_get_fault_addr();
    last_err_code = regs->err_code;

    // Verificar si existe un contexto activo de copia de usuario y si TANTO el EIP de la
    // instrucción como la dirección causante en CR2 pertenecen exactamente a la ventana registrada.
    // Si un #PF ocurre en otra instrucción de Ring 0 o fuera del rango de usuario esperado,
    // NO se intercepta como fixup y cae al camino de diagnóstico/Kernel Panic.
    if (copy_ctx_depth > 0) {
        struct user_copy_fixup_ctx* ctx = &copy_ctx_stack[copy_ctx_depth - 1];
        uintptr_t fault_eip  = (uintptr_t)regs->eip;
        uintptr_t fault_addr = (uintptr_t)last_fault_addr;

        bool eip_in_copy_range  = (fault_eip >= ctx->eip_start && fault_eip < ctx->eip_end);
        bool addr_in_user_range = (fault_addr >= ctx->user_start && fault_addr < ctx->user_end);

        if (eip_in_copy_range && addr_in_user_range) {
            ctx->faulted = true;
            pf_fixup_count++;
            // REDIRECCIÓN REAL DE EIP (Auditoría v19):
            // Modificar el EIP guardado en el marco de interrupción para que IRET salte a
            // ctx->fixup_eip (__user_copy_fault_fixup) y retorne -1 (-EFAULT) sin reejecutar
            // la instrucción que provocó el Page Fault.
            regs->eip = ctx->fixup_eip;
            serial_write_string("[VMM FIXUP] #PF en copia userland recuperado: EIP redirigido a fixup (-EFAULT).\r\n");
            return;
        }
    }

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

    // Si el fallo ocurrió dentro de código de kernel y no corresponde a una instrucción de copia registrada, detener la máquina por seguridad
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

size_t page_fault_get_fixup_count(void) {
    return pf_fixup_count;
}

uint32_t page_fault_get_last_address(void) {
    return last_fault_addr;
}

uint32_t page_fault_get_last_error_code(void) {
    return last_err_code;
}
