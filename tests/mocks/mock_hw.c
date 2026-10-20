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
