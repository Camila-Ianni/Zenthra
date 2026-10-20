#include "klib.h"
#include "vga.h"

typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_end(v)     __builtin_va_end(v)

size_t k_strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void* k_memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    while (count--) {
        *ptr++ = (uint8_t)val;
    }
    return dest;
}

void* k_memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

int k_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int k_strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void uitoa(uint32_t val, char* buf, int base) {
    char tmp[32];
    int i = 0;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (val != 0) {
        int rem = val % base;
        tmp[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        val /= base;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

void itoa(int val, char* buf, int base) {
    if (base == 10 && val < 0) {
        buf[0] = '-';
        uitoa((uint32_t)(-val), buf + 1, 10);
    } else {
        uitoa((uint32_t)val, buf, base);
    }
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char num_buf[32];

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    vga_putc(c);
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (!s) s = "(null)";
                    vga_writestring(s);
                    break;
                }
                case 'd':
                case 'i': {
                    int d = va_arg(args, int);
                    itoa(d, num_buf, 10);
                    vga_writestring(num_buf);
                    break;
                }
                case 'u': {
                    uint32_t u = va_arg(args, uint32_t);
                    uitoa(u, num_buf, 10);
                    vga_writestring(num_buf);
                    break;
                }
                case 'x': {
                    uint32_t x = va_arg(args, uint32_t);
                    uitoa(x, num_buf, 16);
                    vga_writestring(num_buf);
                    break;
                }
                case 'p': {
                    uintptr_t p = (uintptr_t)va_arg(args, void*);
                    vga_writestring("0x");
                    uitoa((uint32_t)p, num_buf, 16);
                    vga_writestring(num_buf);
                    break;
                }
                case '%': {
                    vga_putc('%');
                    break;
                }
                default: {
                    vga_putc('%');
                    vga_putc(fmt[i]);
                    break;
                }
            }
        } else {
            vga_putc(fmt[i]);
        }
    }

    va_end(args);
}
