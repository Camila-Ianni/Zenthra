#ifndef PIT_H
#define PIT_H

#include "ktypes.h"

/* Frecuencia del oscilador base del PIT (1193182 Hz) */
#define PIT_BASE_FREQUENCY 1193182

/* Puertos del chip 8253/8254 */
#define PIT_CHANNEL0_PORT 0x40
#define PIT_COMMAND_PORT  0x43

void pit_init(uint32_t frequency);
uint32_t pit_get_ticks(void);
void pit_sleep(uint32_t milliseconds);

#endif
