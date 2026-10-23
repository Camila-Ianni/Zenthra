#ifndef PMM_H
#define PMM_H

#include "ktypes.h"

#define PAGE_SIZE 4096
#define PMM_FRAME_SIZE PAGE_SIZE

/* Inicializar el Administrador de Memoria Física (PMM) desde Multiboot */
void pmm_init(uint32_t magic, uintptr_t mbi_addr);

/* Asignar un marco físico libre de 4 KiB */
uintptr_t pmm_alloc_frame(void);

/* Liberar un marco físico de 4 KiB previamente asignado */
void pmm_free_frame(uintptr_t frame_addr);

/* Comprobar si un marco físico está ocupado */
bool pmm_is_frame_allocated(uintptr_t frame_addr);

/* Marcar un rango de direcciones físicas como ocupado */
void pmm_mark_region_used(uintptr_t base, size_t length);

/* Marcar un rango de direcciones físicas como disponible */
void pmm_mark_region_free(uintptr_t base, size_t length);

/* Asignar un marco físico libre por debajo de un límite dado (para tablas de páginas accesibles) */
uintptr_t pmm_alloc_frame_below(uintptr_t max_limit);

/* Comprobar si una región completa está marcada como reservada/ocupada */
bool pmm_is_region_reserved(uintptr_t base, size_t length);

/* Métricas del administrador de memoria física */
size_t pmm_get_total_frames(void);
size_t pmm_get_free_frames(void);
size_t pmm_get_used_frames(void);
size_t pmm_get_total_memory_kib(void);
size_t pmm_get_free_memory_kib(void);

#endif
