#ifndef KLIB_H
#define KLIB_H

#include "ktypes.h"

size_t k_strlen(const char* str);
void* k_memset(void* dest, int val, size_t count);
void* k_memcpy(void* dest, const void* src, size_t count);
int k_strcmp(const char* s1, const char* s2);
int k_strncmp(const char* s1, const char* s2, size_t n);
void itoa(int val, char* buf, int base);
void uitoa(uint32_t val, char* buf, int base);

/* kprintf con soporte para %s, %c, %d, %u, %x, %p */
void kprintf(const char* fmt, ...);

#endif
