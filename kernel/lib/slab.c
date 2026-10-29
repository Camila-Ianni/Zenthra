#include "slab.h"
#include "kheap.h"
#include "klib.h"

static kmem_cache_t cache_pool[SLAB_MAX_CACHES];
static bool slab_subsystem_initialized = false;

void slab_init(void) {
    if (slab_subsystem_initialized) return;
    for (size_t i = 0; i < SLAB_MAX_CACHES; i++) {
        cache_pool[i].is_initialized = false;
        cache_pool[i].raw_buffer = NULL;
        cache_pool[i].free_list = NULL;
        cache_pool[i].active_allocs = 0;
        cache_pool[i].total_allocs = 0;
        cache_pool[i].capacity = 0;
        cache_pool[i].obj_size = 0;
        cache_pool[i].name[0] = '\0';
    }
    slab_subsystem_initialized = true;
}

kmem_cache_t* kmem_cache_create(const char* name, size_t size) {
    if (!slab_subsystem_initialized) {
        slab_init();
    }
    if (size == 0) return NULL;

    // Alinear al tamaño mínimo de un puntero (4 bytes en x86-32)
    size_t aligned_size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    if (aligned_size < sizeof(void*)) {
        aligned_size = sizeof(void*);
    }

    // Buscar una ranura libre en el pool de caches
    kmem_cache_t* cache = NULL;
    for (size_t i = 0; i < SLAB_MAX_CACHES; i++) {
        if (!cache_pool[i].is_initialized) {
            cache = &cache_pool[i];
            break;
        }
    }
    if (!cache) return NULL;

    size_t capacity = SLAB_DEFAULT_CAPACITY;
    size_t total_bytes = capacity * aligned_size;
    uint8_t* buf = (uint8_t*)kmalloc(total_bytes);
    if (!buf) return NULL;

    k_memset(buf, 0, total_bytes);

    if (name) {
        size_t nlen = k_strlen(name);
        if (nlen >= SLAB_NAME_MAX) nlen = SLAB_NAME_MAX - 1;
        k_memcpy(cache->name, name, nlen);
        cache->name[nlen] = '\0';
    } else {
        k_memcpy(cache->name, "unnamed_cache", 13);
        cache->name[13] = '\0';
    }

    cache->obj_size = aligned_size;
    cache->capacity = capacity;
    cache->active_allocs = 0;
    cache->total_allocs = 0;
    cache->raw_buffer = buf;

    // Enlazar bloques contiguos en la lista libre inicial O(1)
    for (size_t i = 0; i < capacity; i++) {
        uint8_t* current = buf + (i * aligned_size);
        uint8_t* next = (i + 1 < capacity) ? (buf + ((i + 1) * aligned_size)) : NULL;
        *(void**)current = (void*)next;
    }
    cache->free_list = (void*)buf;
    cache->is_initialized = true;

    return cache;
}

void* kmem_cache_alloc(kmem_cache_t* cache) {
    if (!cache || !cache->is_initialized || !cache->free_list) {
        return NULL;
    }

    void* obj = cache->free_list;
    cache->free_list = *(void**)obj;
    cache->active_allocs++;
    cache->total_allocs++;

    k_memset(obj, 0, cache->obj_size);
    return obj;
}

void kmem_cache_free(kmem_cache_t* cache, void* obj) {
    if (!cache || !cache->is_initialized || !obj) {
        return;
    }

    uintptr_t addr = (uintptr_t)obj;
    uintptr_t start = (uintptr_t)cache->raw_buffer;
    uintptr_t end = start + (cache->capacity * cache->obj_size);

    // Validación estricta de límites de puntero
    if (addr < start || addr >= end) {
        return;
    }
    // Validación de alineación al tamaño del objeto
    if ((addr - start) % cache->obj_size != 0) {
        return;
    }

    // Insertar al inicio de la lista de libres O(1)
    *(void**)obj = cache->free_list;
    cache->free_list = obj;

    if (cache->active_allocs > 0) {
        cache->active_allocs--;
    }
}

void kmem_cache_destroy(kmem_cache_t* cache) {
    if (!cache || !cache->is_initialized) return;

    if (cache->raw_buffer) {
        kfree(cache->raw_buffer);
        cache->raw_buffer = NULL;
    }
    cache->free_list = NULL;
    cache->active_allocs = 0;
    cache->total_allocs = 0;
    cache->capacity = 0;
    cache->obj_size = 0;
    cache->name[0] = '\0';
    cache->is_initialized = false;
}

size_t kmem_cache_get_active(kmem_cache_t* cache) {
    return cache ? cache->active_allocs : 0;
}

size_t kmem_cache_get_total(kmem_cache_t* cache) {
    return cache ? cache->total_allocs : 0;
}
