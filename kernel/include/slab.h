#ifndef SLAB_H
#define SLAB_H

#include "ktypes.h"

#define SLAB_NAME_MAX 32
#define SLAB_MAX_CACHES 16
#define SLAB_DEFAULT_CAPACITY 64

struct kmem_cache;
typedef struct kmem_cache kmem_cache_t;

struct kmem_cache {
    char name[SLAB_NAME_MAX];
    size_t obj_size;
    size_t capacity;
    size_t active_allocs;
    size_t total_allocs;
    void* free_list;
    uint8_t* raw_buffer;
    bool is_initialized;
};

/* Inicializar el subsistema SLAB */
void slab_init(void);

/* Crear una caché de objetos de tamaño fijo */
kmem_cache_t* kmem_cache_create(const char* name, size_t size);

/* Asignar un objeto O(1) desde la caché */
void* kmem_cache_alloc(kmem_cache_t* cache);

/* Liberar un objeto O(1) devolviéndolo a la lista de bloques libres */
void kmem_cache_free(kmem_cache_t* cache, void* obj);

/* Destruir la caché y liberar su memoria */
void kmem_cache_destroy(kmem_cache_t* cache);

/* Métricas de uso */
size_t kmem_cache_get_active(kmem_cache_t* cache);
size_t kmem_cache_get_total(kmem_cache_t* cache);

#endif
