#ifndef KHEAP_H
#define KHEAP_H

#include "ktypes.h"

/* Inicializa el montón del kernel con un puntero base y tamaño */
void kheap_init(void* start_addr, size_t size);

/* Asignación dinámica de memoria en el kernel (First-Fit) */
void* kmalloc(size_t size);

/* Liberación de un bloque previamente asignado */
void kfree(void* ptr);

/* Consulta de estadísticas del heap */
size_t kheap_get_used(void);
size_t kheap_get_free(void);

#endif
