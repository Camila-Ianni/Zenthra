#ifndef SYSCALL_H
#define SYSCALL_H

#include "ktypes.h"
#include "idt.h"
#include "errno.h"

/* Números de llamadas al sistema (Syscalls) de Zenthra OS (Auditoría v15) */
#define SYS_EXIT         1
#define SYS_WRITE        2
#define SYS_READ         3
#define SYS_ARDUINO_CMD  4
#define SYS_GET_PROFILE  5
#define SYS_SET_PROFILE  6

/* Flags para SYS_READ */
#define SYS_READ_NONBLOCK 0x0000
#define SYS_READ_BLOCKING 0x0001

/* Límite del espacio de memoria de usuario (Userland) */
#if UINTPTR_MAX > 0xFFFFFFFF
#define USER_MEMORY_LIMIT 0x00007FFFFFFFFFFFULL
#else
#define USER_MEMORY_LIMIT 0xC0000000UL
#endif

/*
 * ABI de Syscalls Zenthra OS:
 *   EAX: Número de syscall
 *   EBX: Argumento 1
 *   ECX: Argumento 2
 *   EDX: Argumento 3
 *   ESI: Argumento 4
 *   EDI: Argumento 5
 * Retorno en EAX:
 *   >= 0 : Éxito (e.g. número de bytes transferidos o valor de retorno)
 *   < 0  : Código de error negativo (-errno, e.g. -EFAULT, -EAGAIN, -EACCES, -EINVAL)
 */

/* Funciones de copia segura entre espacio de usuario (Ring 3) y kernel (Ring 0) */
bool is_user_pointer(const void* ptr, size_t size);
bool is_user_pages_accessible_failclosed(const void* ptr, size_t size, bool write_intent);
void syscall_set_require_paging(bool require_paging);
bool syscall_is_paging_required(void);
int copy_from_user(void* kernel_dest, const void* user_src, size_t size);
int copy_to_user(void* user_dest, const void* kernel_src, size_t size);
ssize_t strnlen_user(const char* user_str, size_t max_len);

void syscall_init(void);
void syscall_dispatcher(struct registers* regs);

#endif
