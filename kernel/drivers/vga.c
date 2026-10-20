#include "vga.h"
#include "io.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

static size_t vga_row;
static size_t vga_column;
static uint8_t vga_current_color;

void vga_update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_BUFFER[index] = vga_entry(' ', vga_current_color);
        }
    }
    vga_row = 0;
    vga_column = 0;
    vga_update_cursor(0, 0);
}

void vga_init(void) {
    vga_row = 0;
    vga_column = 0;
    vga_current_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
}

void vga_set_color(uint8_t color) {
    vga_current_color = color;
}

static void vga_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_current_color);
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_column = 0;
        if (++vga_row >= VGA_HEIGHT) {
            vga_scroll();
        }
    } else if (c == '\r') {
        vga_column = 0;
    } else if (c == '\t') {
        vga_column = (vga_column + 4) & ~3;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
    } else if (c == '\b') {
        if (vga_column > 0) {
            vga_column--;
            VGA_BUFFER[vga_row * VGA_WIDTH + vga_column] = vga_entry(' ', vga_current_color);
        }
    } else {
        const size_t index = vga_row * VGA_WIDTH + vga_column;
        VGA_BUFFER[index] = vga_entry(c, vga_current_color);
        if (++vga_column >= VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row >= VGA_HEIGHT) {
                vga_scroll();
            }
        }
    }
    vga_update_cursor(vga_column, vga_row);
}

void vga_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        vga_putc(data[i]);
    }
}

void vga_writestring(const char* data) {
    size_t i = 0;
    while (data[i] != '\0') {
        vga_putc(data[i]);
        i++;
    }
}
