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

/*
 * Mecanismo de protección y recuperación (fixup) durante copias entre userland y kernel (Auditoría v18):
 * Permite registrar una ventana activa de copia (copy_from_user / copy_to_user / strnlen_user)
 * y detectar si se produjo un Page Fault inesperado durante la transferencia para abortar con -EFAULT.
 */
void page_fault_begin_user_copy(void);
void page_fault_end_user_copy(void);
bool page_fault_is_user_copy_active(void);
bool page_fault_did_user_copy_fault(void);
void page_fault_simulate_user_copy_fault(uint32_t fault_addr);

#endif
