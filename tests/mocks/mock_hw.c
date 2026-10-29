#include "ktypes.h"
#include "vga.h"
#include "serial.h"

/* Buffers simulados en memoria para verificar comportamiento en pruebas */
uint16_t mock_vga_buffer[80 * 25];
char mock_serial_tx_buffer[1024];
size_t mock_serial_tx_len = 0;

#include "idt.h"

static isr_t mock_interrupt_handlers[256];
uint32_t mock_cr2_addr = 0;

void mock_hw_reset(void) {
    for (size_t i = 0; i < 80 * 25; i++) {
        mock_vga_buffer[i] = 0;
    }
    mock_serial_tx_len = 0;
    mock_serial_tx_buffer[0] = '\0';
    mock_cr2_addr = 0;
    for (int i = 0; i < 256; i++) {
        mock_interrupt_handlers[i] = NULL;
    }
}

/* Stubs para pruebas de interrupciones y syscalls en host */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    (void)num; (void)base; (void)sel; (void)flags;
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    mock_interrupt_handlers[n] = handler;
}

isr_t mock_get_interrupt_handler(uint8_t n) {
    return mock_interrupt_handlers[n];
}

bool nova_iot_ping(void) {
    return true;
}

bool nova_iot_get_temp(uint16_t* out_temp) {
    if (out_temp) *out_temp = 250;
    return true;
}

void tss_set_kernel_stack(uint32_t esp0) {
    (void)esp0;
}

void pic_clear_mask(uint8_t irq) {
    (void)irq;
}

void pic_set_mask(uint8_t irq) {
    (void)irq;
}

void serial_write_char(char c) {
    if (mock_serial_tx_len < sizeof(mock_serial_tx_buffer) - 1) {
        mock_serial_tx_buffer[mock_serial_tx_len++] = c;
        mock_serial_tx_buffer[mock_serial_tx_len] = '\0';
    }
}

void serial_write_string(const char* s) {
    if (!s) return;
    while (*s) {
        serial_write_char(*s++);
    }
}

int serial_received(void) {
    return 0;
}

char serial_read(void) {
    return 0;
}
