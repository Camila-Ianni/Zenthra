#include "ktypes.h"
#include "vga.h"
#include "serial.h"

/* Buffers simulados en memoria para verificar comportamiento en pruebas */
uint16_t mock_vga_buffer[80 * 25];
char mock_serial_tx_buffer[1024];
size_t mock_serial_tx_len = 0;

void mock_hw_reset(void) {
    for (size_t i = 0; i < 80 * 25; i++) {
        mock_vga_buffer[i] = 0;
    }
    mock_serial_tx_len = 0;
    mock_serial_tx_buffer[0] = '\0';
}

/* Stubs para pruebas de syscalls en host */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    (void)num; (void)base; (void)sel; (void)flags;
}

void register_interrupt_handler(uint8_t n, void (*handler)(void*)) {
    (void)n; (void)handler;
}

bool nova_iot_ping(void) {
    return true;
}

bool nova_iot_get_temp(uint16_t* out_temp) {
    if (out_temp) *out_temp = 250;
    return true;
}
