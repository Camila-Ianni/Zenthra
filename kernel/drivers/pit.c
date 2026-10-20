#include "pit.h"
#include "io.h"
#include "idt.h"

static volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency = 100; // 100 Hz por defecto (1 tick = 10ms)

/* Callback ejecutado en cada interrupción IRQ0 (Vector 32) */
static void timer_callback(struct registers* regs) {
    (void)regs;
    timer_ticks++;
}

void pit_init(uint32_t frequency) {
    timer_frequency = frequency;
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    // Comando: Canal 0, Modo 3 (Square Wave Generator), Acceso LSB luego MSB
    outb(PIT_COMMAND_PORT, 0x36);

    // Enviar divisor al canal 0
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    // Registrar handler de IRQ0 (vector 32 en IDT tras remapeo PIC)
    register_interrupt_handler(32, timer_callback);
}

uint32_t pit_get_ticks(void) {
    return timer_ticks;
}

void pit_sleep(uint32_t milliseconds) {
    // Calcular ticks requeridos según la frecuencia configurada
    uint32_t ticks_to_wait = (milliseconds * timer_frequency) / 1000;
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    uint32_t target_tick = timer_ticks + ticks_to_wait;
    while (timer_ticks < target_tick) {
        __asm__ volatile ("hlt"); // Pausa eficiente de la CPU hasta la siguiente interrupción
    }
}
