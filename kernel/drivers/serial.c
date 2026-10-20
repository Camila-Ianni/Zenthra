#include "serial.h"
#include "io.h"
#include "klib.h"

int serial_init(void) {
    outb(COM1_PORT + 1, 0x00);    // Deshabilitar interrupciones
    outb(COM1_PORT + 3, 0x80);    // Habilitar DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03);    // Divisor 3 -> 38400 baudios (o 1 -> 115200)
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);    // 8 bits, sin paridad, 1 stop bit (8N1)
    outb(COM1_PORT + 2, 0xC7);    // FIFO activado, clear con umbral de 14 bytes
    outb(COM1_PORT + 4, 0x0B);    // IRQs habilitadas, RTS/DSR activados
    outb(COM1_PORT + 4, 0x1E);    // Activar modo loopback para prueba
    outb(COM1_PORT + 0, 0xAE);    // Enviar byte de prueba

    // Comprobar si el chip serial funciona
    if (inb(COM1_PORT + 0) != 0xAE) {
        return 1; // Error en el puerto serie
    }

    // Modo normal de operación (desactivar loopback)
    outb(COM1_PORT + 4, 0x0F);
    return 0; // Exitoso
}

int serial_received(void) {
    return inb(COM1_PORT + 5) & 1;
}

char serial_read(void) {
    while (serial_received() == 0);
    return inb(COM1_PORT);
}

int serial_is_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_write_char(char a) {
    while (serial_is_transmit_empty() == 0);
    outb(COM1_PORT, a);
}

void serial_write_string(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(str[i]);
    }
}
