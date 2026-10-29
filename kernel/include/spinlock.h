#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "ktypes.h"

/*
 * Spinlock multiprocesador (SMP-ready) para Zenthra OS
 * Basado en primitivas atómicas de compilador (__sync_lock_test_and_set)
 * y la instrucción x86 'pause' para reducir contención de bus en bucles de espera.
 */

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spinlock_init(spinlock_t* sl) {
    if (sl) {
        sl->lock = 0;
    }
}

static inline void spin_lock(spinlock_t* sl) {
    if (!sl) return;
#if defined(__i386__) || defined(__x86_64__)
    while (__sync_lock_test_and_set(&sl->lock, 1)) {
        __asm__ volatile("pause" ::: "memory");
    }
#else
    while (__sync_lock_test_and_set(&sl->lock, 1)) {}
#endif
}

static inline bool spin_trylock(spinlock_t* sl) {
    if (!sl) return false;
    return (__sync_lock_test_and_set(&sl->lock, 1) == 0);
}

static inline void spin_unlock(spinlock_t* sl) {
    if (!sl) return;
    __sync_lock_release(&sl->lock);
}

static inline uint32_t spin_lock_irqsave(spinlock_t* sl) {
    uint32_t flags = 0;
#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
#endif
    spin_lock(sl);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t* sl, uint32_t flags) {
    spin_unlock(sl);
#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile("pushl %0; popfl" : : "r"(flags) : "memory");
#else
    (void)flags;
#endif
}

#endif
