#ifndef TASK_H
#define TASK_H

#include "ktypes.h"
#include "shell.h"

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

#define TASK_DEFAULT_QUANTUM 5

/* Bloque de Control de Proceso (PCB) */
struct task {
    uint32_t id;
    char name[24];
    enum task_state state;
    uint8_t* stack;
    size_t stack_size;
    uint32_t esp;               // ESP guardado de contexto (switch_context)
    uint32_t kernel_stack_top;  // Tope real superior de la pila de kernel (TSS.ESP0)
    enum nova_profile profile;  // Credencial / perfil de seguridad propio de este proceso
    uint32_t sleep_ticks;       // Ticks restantes para despertar si está en TASK_SLEEPING
    uint32_t time_slice;        // Quantum restante para preempción por timer PIT
    uint32_t runtime_ticks;
};

struct task_info {
    uint32_t id;
    char name[24];
    enum task_state state;
    uint32_t runtime_ticks;
};

void scheduler_init(void);
int task_create(const char* name, void (*entry_point)(void));
int task_create_with_profile(const char* name, void (*entry_point)(void), enum nova_profile profile);
void task_set_current_profile(enum nova_profile profile);
void task_yield(void);
void task_sleep(uint32_t ticks);
void task_schedule_tick(void);
void scheduler_lock(void);
void scheduler_unlock(void);
void scheduler_set_preemptive(bool enable);
bool scheduler_is_preemptive(void);
void task_exit(void);
void task_list(void);
struct task* task_get_current(void);
bool task_get_info_at(int idx, struct task_info* out_info);

#endif
