#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include "ktypes.h"
#include "idt.h"

#define PF_ERR_PRESENT  (1 << 0)
#define PF_ERR_WRITE    (1 << 1)
#define PF_ERR_USER     (1 << 2)
#define PF_ERR_RESERVED (1 << 3)
#define PF_ERR_FETCH    (1 << 4)

/* Inicializar y registrar el manejador de Page Fault (Vector 14 / ISR 14) */
void page_fault_init(void);

/* Manejador de la excepción #PF de hardware de la CPU */
void page_fault_handler(struct registers* regs);

/* Métricas y telemetría de fallos de página */
size_t page_fault_get_count(void);
uint32_t page_fault_get_last_address(void);
uint32_t page_fault_get_last_error_code(void);
size_t page_fault_get_fixup_count(void);

/*
 * Mecanismo de recuperación real de excepciones #PF (Exception Table / Fixup) durante copias
 * entre userland y kernel (Auditoría v19):
 *
 * 1. Pila reentrante de contextos de copia (profundidad USER_COPY_CTX_MAX_DEPTH) para soportar
 *    o rechazar explícitamente el anidamiento sin sobrescribir ni perder el contexto exterior.
 * 2. Vinculación estricta del fallo a:
 *      (a) Rango de EIP registrado de la instrucción de copia ([eip_start, eip_end)), y
 *      (b) Rango de direcciones virtuales de usuario esperado ([user_start, user_end)).
 * 3. Redirección real del contexto de CPU guardado: si el #PF cumple ambas condiciones,
 *    page_fault_handler reescribe regs->eip = fixup_eip para que IRET salte a la etiqueta de
 *    salida (__user_copy_fault_fixup) y retorne -1 (-EFAULT) sin reintentar la instrucción fallida.
 * 4. Si el #PF ocurre en otra instrucción de Ring 0 o sobre una dirección de kernel fuera de la
 *    ventana de usuario registrada, NO se oculta: sigue el camino normal de diagnóstico y Kernel Panic.
 */
#define USER_COPY_CTX_MAX_DEPTH 4

struct user_copy_fixup_ctx {
    uintptr_t user_start;
    uintptr_t user_end;
    uintptr_t eip_start;
    uintptr_t eip_end;
    uintptr_t fixup_eip;
    bool      faulted;
};

bool page_fault_push_user_copy_ctx(uintptr_t user_start, size_t size,
                                   uintptr_t eip_start, uintptr_t eip_end, uintptr_t fixup_eip);
bool page_fault_pop_user_copy_ctx(void);
size_t page_fault_get_user_copy_depth(void);

/* Wrappers de compatibilidad con rango de usuario y EIP de la primitiva arch_user_copy_bytes */
void page_fault_begin_user_copy_range(uintptr_t user_addr, size_t size);
void page_fault_begin_user_copy(void);
void page_fault_end_user_copy(void);
bool page_fault_is_user_copy_active(void);
bool page_fault_did_user_copy_fault(void);

/* Primitiva en assembly x86 (interrupts.s) con etiquetas de excepción exportadas */
int arch_user_copy_bytes(void* dst, const void* src, size_t size);
uintptr_t page_fault_get_copy_eip_start(void);
uintptr_t page_fault_get_copy_eip_end(void);
uintptr_t page_fault_get_copy_fixup_eip(void);

#endif
