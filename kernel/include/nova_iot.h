#ifndef NOVA_IOT_H
#define NOVA_IOT_H

#include "ktypes.h"
#include "nova_link.h"

/* Estructura para almacenar el último estado recibido de Arduino */
struct arduino_state {
    bool connected;
    bool led_state;
    uint16_t last_temp_decicelsius; // Ej: 245 = 24.5 °C
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t errors_count;
};

void nova_iot_init(void);
bool nova_iot_ping(void);
bool nova_iot_set_led(bool state);
bool nova_iot_get_temp(uint16_t* out_temp);
const struct arduino_state* nova_iot_get_state(void);

#endif
