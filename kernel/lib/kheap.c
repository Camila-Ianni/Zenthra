#include "kheap.h"
#include "klib.h"

/* Estructura de cabecera para cada bloque en la lista enlazada del heap */
struct block_header {
    size_t size;                // Tamaño utilizable del bloque (excluyendo la cabecera)
    bool is_free;               // true si está disponible, false si está ocupado
    struct block_header* next;  // Siguiente bloque en la lista
};

#define HEADER_SIZE sizeof(struct block_header)

static struct block_header* heap_head = NULL;
static size_t heap_total_size = 0;
static size_t heap_used_size = 0;

void kheap_init(void* start_addr, size_t size) {
    if (size <= HEADER_SIZE || start_addr == NULL) return;

    heap_head = (struct block_header*)start_addr;
    heap_head->size = size - HEADER_SIZE;
    heap_head->is_free = true;
    heap_head->next = NULL;

    heap_total_size = size;
    heap_used_size = 0;
}

void* kmalloc(size_t size) {
    if (size == 0 || heap_head == NULL) return NULL;

    // Alinear tamaño a múltiplos de 8 bytes (seguro en 32-bit y 64-bit)
    size = (size + 7) & ~7;

    struct block_header* curr = heap_head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            // Si el bloque sobrante es suficiente para albergar un nuevo bloque, dividirlo
            if (curr->size >= size + HEADER_SIZE + 8) {
                struct block_header* next_block = (struct block_header*)((uint8_t*)curr + HEADER_SIZE + size);
                next_block->size = curr->size - size - HEADER_SIZE;
                next_block->is_free = true;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = false;
            heap_used_size += (curr->size + HEADER_SIZE);
            return (void*)((uint8_t*)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }
    return NULL; // Sin memoria suficiente
}

void kfree(void* ptr) {
    if (ptr == NULL || heap_head == NULL) return;

    struct block_header* block = (struct block_header*)((uint8_t*)ptr - HEADER_SIZE);
    if (block->is_free) return; // Doble liberación evitada

    block->is_free = true;
    heap_used_size -= (block->size + HEADER_SIZE);

    // Coalescencia: fusionar bloques libres contiguos
    struct block_header* curr = heap_head;
    while (curr != NULL && curr->next != NULL) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += curr->next->size + HEADER_SIZE;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

size_t kheap_get_used(void) {
    return heap_used_size;
}

size_t kheap_get_free(void) {
    return (heap_total_size > heap_used_size) ? (heap_total_size - heap_used_size) : 0;
}
