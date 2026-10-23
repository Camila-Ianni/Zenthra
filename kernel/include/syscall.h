#ifndef SYSCALL_H
#define SYSCALL_H

#include "ktypes.h"
#include "idt.h"

/* Números de llamadas al sistema (Syscalls) de NOVA OS */
#define SYS_EXIT         1
#define SYS_WRITE        2
#define SYS_READ         3
#define SYS_ARDUINO_CMD  4
#define SYS_GET_PROFILE  5
#define SYS_SET_PROFILE  6

/* Límite del espacio de memoria de usuario (Userland) */
#if UINTPTR_MAX > 0xFFFFFFFF
#define USER_MEMORY_LIMIT 0x00007FFFFFFFFFFFULL
#else
#define USER_MEMORY_LIMIT 0xC0000000UL
#endif

/* Funciones de copia segura entre espacio de usuario (Ring 3) y kernel (Ring 0) */
bool is_user_pointer(const void* ptr, size_t size);
int copy_from_user(void* kernel_dest, const void* user_src, size_t size);
int copy_to_user(void* user_dest, const void* kernel_src, size_t size);
ssize_t strnlen_user(const char* user_str, size_t max_len);

void syscall_init(void);
void syscall_dispatcher(struct registers* regs);

#endif
