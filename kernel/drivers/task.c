#include "task.h"
#include "kheap.h"
#include "klib.h"
#include "vga.h"

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

void scheduler_init(void) {
    k_memset(task_table, 0, sizeof(task_table));

    // Tarea 0: Tarea Principal del Kernel / Shell
    task_table[0].id = next_task_id++;
    const char* k_name = "kernel_shell";
    size_t len = k_strlen(k_name);
    k_memcpy(task_table[0].name, k_name, len + 1);
    task_table[0].state = TASK_RUNNING;
    task_table[0].stack = NULL; // Utiliza el stack configurado en boot.s
    task_table[0].stack_size = 16384;
    task_table[0].esp = 0;
    task_table[0].runtime_ticks = 0;

    current_task_idx = 0;
}

int task_create(const char* name, void (*entry_point)(void)) {
    if (!name || !entry_point) return -1;

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
            task_table[i].state = TASK_READY;
            task_table[i].runtime_ticks = 0;

            // Marco de pila exacto para switch_context (retorno vía 'ret' y popfl/pop registers)
            uint32_t* sp = (uint32_t*)(stack + TASK_STACK_SIZE);

            *(--sp) = (uintptr_t)task_exit;   // Dirección si entry_point() termina
            *(--sp) = (uintptr_t)entry_point; // EIP inicial desapilado por 'ret' en switch_context
            *(--sp) = 0x202;                  // EFLAGS (IF=1 para interrupciones activas)
            *(--sp) = 0;                      // EDI
            *(--sp) = 0;                      // ESI
            *(--sp) = 0;                      // EBX
            *(--sp) = 0;                      // EBP

            task_table[i].esp = (uint32_t)(uintptr_t)sp;
            return (int)task_table[i].id;
        }
    }
    return -3; // Tabla de tareas llena
}

struct task* task_get_current(void) {
    return &task_table[current_task_idx];
}

void task_exit(void) {
    task_table[current_task_idx].state = TASK_TERMINATED;
    if (task_table[current_task_idx].stack != NULL) {
        kfree(task_table[current_task_idx].stack);
        task_table[current_task_idx].stack = NULL;
    }
    task_yield();
    while (1);
}

void task_yield(void) {
    // Round-Robin simple: buscar siguiente tarea lista
    int next_idx = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_task_idx + i) % MAX_TASKS;
        if (task_table[idx].state == TASK_READY || task_table[idx].state == TASK_RUNNING) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx == -1 || next_idx == current_task_idx) {
        return; // No hay otra tarea esperando
    }

    int prev_idx = current_task_idx;
    if (task_table[prev_idx].state == TASK_RUNNING) {
        task_table[prev_idx].state = TASK_READY;
    }

    current_task_idx = next_idx;
    task_table[current_task_idx].state = TASK_RUNNING;
    task_table[current_task_idx].runtime_ticks++;

    switch_context(&task_table[prev_idx].esp, task_table[current_task_idx].esp);
}

uint32_t scheduler_schedule(uint32_t current_esp) {
    task_table[current_task_idx].esp = current_esp;
    task_table[current_task_idx].runtime_ticks++;

    int next_idx = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_task_idx + i) % MAX_TASKS;
        if (task_table[idx].state == TASK_READY || task_table[idx].state == TASK_RUNNING) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx != -1) {
        if (task_table[current_task_idx].state == TASK_RUNNING) {
            task_table[current_task_idx].state = TASK_READY;
        }
        current_task_idx = next_idx;
        task_table[current_task_idx].state = TASK_RUNNING;
    }

    return task_table[current_task_idx].esp;
}

void task_list(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Tabla de Procesos y Tareas de NOVA OS:\n");
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
