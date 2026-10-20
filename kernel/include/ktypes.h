#ifndef KTYPES_H
#define KTYPES_H

#ifdef __KERNEL__
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint32_t           size_t;
typedef int32_t            ssize_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
typedef uint8_t bool;

#else
/* Entorno de pruebas / Host */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif

#endif
