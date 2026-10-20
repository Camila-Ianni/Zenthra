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

void syscall_init(void);

#endif
