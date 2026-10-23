#ifndef TASK_H
#define TASK_H

#include "ktypes.h"

#define MAX_TASKS 8

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY  = 1,
    TASK_RUNNING = 2,
    TASK_SLEEPING = 3,
    TASK_TERMINATED = 4
};

/* Contexto de ejecución de una tarea x86 */
struct task_context {
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eflags;
    uint32_t eip;
};

/* Bloque de Control de Proceso (PCB) */
struct task {
    uint32_t id;
    char name[24];
    enum task_state state;
    uint8_t* stack;
    size_t stack_size;
    uint32_t esp;
    uint32_t sleep_ticks;
    uint32_t runtime_ticks;
};

void scheduler_init(void);
int task_create(const char* name, void (*entry_point)(void));
void task_yield(void);
void task_exit(void);
void task_list(void);
struct task* task_get_current(void);

#endif
