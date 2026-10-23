#include "task.h"
#include "kheap.h"
#include "klib.h"
#include "vga.h"
#include "gdt.h"
#include "acl_security.h"
#include "serial.h"

#if defined(__KERNEL__)
extern void switch_context(uint32_t* old_esp_ptr, uint32_t new_esp);
#else
static inline void switch_context(uint32_t* old_esp_ptr, uint32_t new_esp) {
    if (old_esp_ptr) *old_esp_ptr = 0x1234;
    (void)new_esp;
}
#endif

#define TASK_STACK_SIZE 4096

static struct task task_table[MAX_TASKS];
static int current_task_idx = 0;
static uint32_t next_task_id = 1;
static uint8_t main_task_stack[TASK_STACK_SIZE];
static bool preemption_enabled = true;
#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
static inline uint32_t irq_save(void) {
    uint32_t eflags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(eflags) : : "memory");
    return eflags;
}

static inline void irq_restore(uint32_t eflags) {
    __asm__ volatile ("pushl %0; popfl" : : "r"(eflags) : "memory");
}
#else
static inline uint32_t irq_save(void) { return 0; }
static inline void irq_restore(uint32_t eflags) { (void)eflags; }
#endif

/*
 * CONTRATO DE CONCURRENCIA UNIPROCESADOR (UP) — Auditoría Técnica v12:
 *
 * 1. Modelo de Ejecución:
 *    El kernel de Zenthra opera actualmente en modo Uniprocesador (UP, un solo núcleo x86).
 *    La exclusión mutua de secciones críticas del scheduler se garantiza deshabilitando
 *    interrupciones a nivel de CPU mediante 'cli' e 'irq_save()' (pushfl), previniendo que
 *    una interrupción de temporizador PIT (IRQ0) o periférico conmute el contexto en medio
 *    de una mutación de la tabla de tareas o colas de pilas.
 *
 * 2. Anidamiento Estricto:
 *    El contador reentrante 'scheduler_lock_count' permite que funciones internas del scheduler
 *    se invoquen entre sí sin perder el estado original de IF (Interrupt Flag). 'saved_eflags'
 *    se captura en el primer nivel (count == 0) y solo se restaura con 'popfl' al desbloquear el nivel 0.
 *
 * 3. Límite Arquitectural SMP:
 *    Este mecanismo NO es un spinlock multiprocesador SMP; en un entorno SMP de múltiples núcleos,
 *    'cli' solo afecta a la CPU local. La transición a SMP requerirá spinlocks atómicos basados en
 *    'lock cmpxchg' / 'pause' y colas por CPU (planificado para fases posteriores a la estabilización i686/BIOS).
 */
static volatile int scheduler_lock_count = 0;
static volatile uint32_t saved_eflags = 0;

void scheduler_lock(void) {
    uint32_t flags = irq_save();
    if (scheduler_lock_count == 0) {
        saved_eflags = flags;
    }
    scheduler_lock_count++;
}

void scheduler_unlock(void) {
    if (scheduler_lock_count > 0) {
        scheduler_lock_count--;
        if (scheduler_lock_count == 0) {
            irq_restore(saved_eflags);
        }
    }
}

void scheduler_set_preemptive(bool enable) {
    preemption_enabled = enable;
}

bool scheduler_is_preemptive(void) {
    return preemption_enabled;
}

void scheduler_init(void) {
    k_memset(task_table, 0, sizeof(task_table));
    next_task_id = 1;
    preemption_enabled = true;
    scheduler_lock_count = 0;

    // Tarea 0: Tarea Principal del Kernel / Shell
    task_table[0].id = next_task_id++;
    const char* k_name = "kernel_shell";
    size_t len = k_strlen(k_name);
    k_memcpy(task_table[0].name, k_name, len + 1);
    task_table[0].state = TASK_RUNNING;
    task_table[0].stack = main_task_stack;
    task_table[0].stack_size = TASK_STACK_SIZE;
    task_table[0].kernel_stack_top = (uint32_t)(uintptr_t)(main_task_stack + TASK_STACK_SIZE);
    task_table[0].esp = 0;
    task_table[0].profile = PROFILE_GENERAL;
    task_table[0].sleep_ticks = 0;
    task_table[0].time_slice = TASK_DEFAULT_QUANTUM;
    task_table[0].runtime_ticks = 0;

    current_task_idx = 0;
    tss_set_kernel_stack(task_table[0].kernel_stack_top);
}

static void drain_zombie_stacks(uint8_t* active_stack);

int task_create_with_profile(const char* name, void (*entry_point)(void), enum nova_profile profile) {
    if (!name || !entry_point) return -1;

    // Reciclar de inmediato cualquier pila de tareas previamente terminadas
    drain_zombie_stacks(task_table[current_task_idx].stack);

    for (int i = 1; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_UNUSED || task_table[i].state == TASK_TERMINATED) {
            uint8_t* stack = (uint8_t*)kmalloc(TASK_STACK_SIZE);
            if (!stack) return -2; // Sin memoria

            task_table[i].id = next_task_id++;
            size_t len = k_strlen(name);
            if (len >= 23) len = 23;
            k_memcpy(task_table[i].name, name, len);
            task_table[i].name[len] = '\0';

            task_table[i].stack = stack;
            task_table[i].stack_size = TASK_STACK_SIZE;
            task_table[i].kernel_stack_top = (uint32_t)(uintptr_t)(stack + TASK_STACK_SIZE);
            task_table[i].profile = profile;
            task_table[i].sleep_ticks = 0;
            task_table[i].time_slice = TASK_DEFAULT_QUANTUM;
            task_table[i].state = TASK_READY;
            task_table[i].runtime_ticks = 0;

            // Marco de pila exacto para switch_context:
            // Orden de restauración por CPU: popfl -> pop edi -> pop esi -> pop ebx -> pop ebp -> ret (entry_point) -> ret (task_exit)
            uint32_t* sp = (uint32_t*)(stack + TASK_STACK_SIZE);

            *(--sp) = (uintptr_t)task_exit;   // Retorno al terminar entry_point() [sp + 24]
            *(--sp) = (uintptr_t)entry_point; // EIP inicial desapilado por ret en switch_context [sp + 20]
            *(--sp) = 0;                      // EBP: desapilado por popl %ebp [sp + 16]
            *(--sp) = 0;                      // EBX: desapilado por popl %ebx [sp + 12]
            *(--sp) = 0;                      // ESI: desapilado por popl %esi [sp + 8]
            *(--sp) = 0;                      // EDI: desapilado por popl %edi [sp + 4]
            *(--sp) = 0x202;                  // EFLAGS: desapilado por popfl (IF=1) [sp + 0]

            task_table[i].esp = (uint32_t)(uintptr_t)sp;
            return (int)task_table[i].id;
        }
    }
    return -3; // Tabla de tareas llena
}

int task_create(const char* name, void (*entry_point)(void)) {
    enum nova_profile inherited = task_table[current_task_idx].profile;
    return task_create_with_profile(name, entry_point, inherited);
}

#define MAX_ZOMBIE_STACKS 32
static uint8_t* zombie_stacks[MAX_ZOMBIE_STACKS];
static size_t zombie_stack_count = 0;

static void drain_zombie_stacks(uint8_t* active_stack);

static void enqueue_zombie_stack(uint8_t* stack) {
    if (!stack) return;

    // Evitar duplicados en la cola
    for (size_t i = 0; i < zombie_stack_count; i++) {
        if (zombie_stacks[i] == stack) return;
    }

    if (zombie_stack_count >= MAX_ZOMBIE_STACKS) {
        // En un modelo UP, cualquier pila en la cola distinta a la de la tarea en ejecución es segura de liberar
        uint8_t* cur_active = task_table[current_task_idx].stack;
        drain_zombie_stacks(cur_active);
    }

    if (zombie_stack_count < MAX_ZOMBIE_STACKS) {
        zombie_stacks[zombie_stack_count++] = stack;
    } else {
        serial_write_string("[SCHEDULER] Advertencia: Cola zombie saturada tras purga.\r\n");
    }
}

static void drain_zombie_stacks(uint8_t* active_stack) {
    for (size_t i = 0; i < zombie_stack_count; i++) {
        uint8_t* s = zombie_stacks[i];
        if (s != NULL && s != active_stack) {
            kfree(s);
            zombie_stacks[i] = NULL;
        }
    }
    size_t new_count = 0;
    for (size_t i = 0; i < zombie_stack_count; i++) {
        if (zombie_stacks[i] != NULL) {
            zombie_stacks[new_count++] = zombie_stacks[i];
        }
    }
    zombie_stack_count = new_count;
}

struct task* task_get_current(void) {
    return &task_table[current_task_idx];
}

void task_set_current_profile(enum nova_profile profile) {
    if (current_task_idx >= 0 && current_task_idx < MAX_TASKS) {
        task_table[current_task_idx].profile = profile;
    }
}

void task_exit(void) {
    // Marcar la tarea como terminada. La pila sigue siendo utilizada por la CPU
    // hasta que switch_context transfiera la ejecución a otra tarea activa.
    task_table[current_task_idx].state = TASK_TERMINATED;

    task_yield();
#if defined(__KERNEL__)
    while (1);
#endif
}

void task_sleep(uint32_t ticks) {
    if (ticks == 0) return;
    scheduler_lock();
    task_table[current_task_idx].sleep_ticks = ticks;
    task_table[current_task_idx].state = TASK_SLEEPING;
    scheduler_unlock();
    task_yield();
}

void task_schedule_tick(void) {
    // 1. Decrementar temporizadores de sueño y despertar tareas listas
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state == TASK_SLEEPING) {
            if (task_table[i].sleep_ticks > 0) {
                task_table[i].sleep_ticks--;
            }
            if (task_table[i].sleep_ticks == 0) {
                task_table[i].state = TASK_READY;
            }
        }
    }

    // 2. Comprobar si la preempción está habilitada y no hay bloqueo de sección crítica
    if (!preemption_enabled || scheduler_lock_count > 0) {
        return;
    }

    // 3. Decrementar time_slice de la tarea activa
    if (task_table[current_task_idx].state == TASK_RUNNING) {
        if (task_table[current_task_idx].time_slice > 0) {
            task_table[current_task_idx].time_slice--;
        }

        // Si expiró el quantum de tiempo (50ms por defecto), forzar preempción
        if (task_table[current_task_idx].time_slice == 0) {
            task_table[current_task_idx].time_slice = TASK_DEFAULT_QUANTUM;
            task_yield();
        }
    }
}

void task_yield(void) {
    // Round-Robin: buscar la siguiente tarea en estado TASK_READY (ignora TASK_SLEEPING)
    int next_idx = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_task_idx + i) % MAX_TASKS;
        if (task_table[idx].state == TASK_READY) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx == -1) {
        // Si no hay otra tarea en READY, mantener la actual si sigue en RUNNING
        if (task_table[current_task_idx].state == TASK_RUNNING) {
            return;
        }
        // Si la actual está en SLEEPING o TERMINATED, intentar volver a kernel_shell (tarea 0)
        if (task_table[0].state == TASK_READY || task_table[0].state == TASK_RUNNING) {
            next_idx = 0;
        } else {
            return;
        }
    }

    if (next_idx == current_task_idx && task_table[current_task_idx].state == TASK_RUNNING) {
        return; // No hay necesidad de conmutar hacia la misma tarea
    }

    int prev_idx = current_task_idx;
    if (task_table[prev_idx].state == TASK_RUNNING) {
        task_table[prev_idx].state = TASK_READY;
    } else if (task_table[prev_idx].state == TASK_TERMINATED) {
        // Encolar pila de la tarea que termina para liberación diferida segura
        if (task_table[prev_idx].stack != NULL) {
            enqueue_zombie_stack(task_table[prev_idx].stack);
            task_table[prev_idx].stack = NULL;
        }
    }

    current_task_idx = next_idx;
    task_table[current_task_idx].state = TASK_RUNNING;
    task_table[current_task_idx].runtime_ticks++;
    task_table[current_task_idx].time_slice = TASK_DEFAULT_QUANTUM;
    tss_set_kernel_stack(task_table[current_task_idx].kernel_stack_top);

    switch_context(&task_table[prev_idx].esp, task_table[current_task_idx].esp);

    // Al retornar a este punto, la CPU está ejecutando en el contexto de la nueva tarea activa.
    // Drenar de forma segura la cola de pilas zombies pendientes de liberación.
    drain_zombie_stacks(task_table[current_task_idx].stack);
}

void task_list(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Tabla de Procesos y Tareas de Zenthra OS:\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i].state != TASK_UNUSED) {
            const char* state_str = "DESCONOCIDO";
            switch (task_table[i].state) {
                case TASK_READY:      state_str = "READY     "; break;
                case TASK_RUNNING:    state_str = "RUNNING   "; break;
                case TASK_SLEEPING:   state_str = "SLEEPING  "; break;
                case TASK_TERMINATED: state_str = "TERMINATED"; break;
                default: break;
            }
            kprintf("  PID: %2u | Nombre: %-16s | Estado: %s | Ticks: %u\n",
                    task_table[i].id, task_table[i].name, state_str, task_table[i].runtime_ticks);
        }
    }
}

bool task_get_info_at(int idx, struct task_info* out_info) {
    if (idx < 0 || idx >= MAX_TASKS || !out_info) return false;
    if (task_table[idx].state == TASK_UNUSED) return false;

    out_info->id = task_table[idx].id;
    k_memcpy(out_info->name, task_table[idx].name, sizeof(out_info->name));
    out_info->state = task_table[idx].state;
    out_info->runtime_ticks = task_table[idx].runtime_ticks;
    return true;
}

