#ifndef VMM_H
#define VMM_H

#include "ktypes.h"
#include "pmm.h"

/* Flags de entradas de tabla y directorio de páginas (x86 Paging) */
#define PTE_PRESENT       0x001
#define PTE_WRITABLE      0x002
#define PTE_USER          0x004
#define PTE_WRITETHROUGH  0x008
#define PTE_NOCACHE       0x010
#define PTE_ACCESSED      0x020
#define PTE_DIRTY         0x040
#define PTE_FRAME_MASK    (~(uintptr_t)0xFFF)

#define PDE_PRESENT       0x001
#define PDE_WRITABLE      0x002
#define PDE_USER          0x004
#define PDE_FRAME_MASK    (~(uintptr_t)0xFFF)

#define ENTRIES_PER_PAGE_TABLE 1024
#define ENTRIES_PER_PAGE_DIR   1024

/* Configuración de Identity Mapping supervisor de 32 bits (Ring 0) */
#define KERNEL_IDENTITY_TABLES 16 // 16 tablas x 1024 entradas x 4096 bytes = 64 MiB (0x04000000)
#define KERNEL_IDENTITY_MAP_SIZE ((uintptr_t)KERNEL_IDENTITY_TABLES * ENTRIES_PER_PAGE_TABLE * PAGE_SIZE)
#define KERNEL_IDENTITY_LIMIT    KERNEL_IDENTITY_MAP_SIZE

/* Estructura alineada a 4 KiB para tabla de páginas (1024 entradas x 4 bytes = 4 KiB) */
struct page_table {
    uint32_t entries[ENTRIES_PER_PAGE_TABLE];
} __attribute__((aligned(4096)));

/* Estructura alineada a 4 KiB para directorio de páginas (1024 entradas x 4 bytes = 4 KiB) */
struct page_directory {
    uint32_t entries[ENTRIES_PER_PAGE_DIR];
} __attribute__((aligned(4096)));

/* Inicializar paginación x86 de 32 bits, mapeo de identidad del kernel y activar CR0.PG */
void vmm_init(void);

/* Mapear una dirección virtual hacia una dirección física con flags especificados */
int vmm_map_page(struct page_directory* pd, uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags);

/* Mapear un rango contiguo de páginas virtuales hacia páginas físicas */
int vmm_map_range(struct page_directory* pd, uintptr_t virt_start, uintptr_t phys_start, size_t size, uint32_t flags);

/* Mapear una región física arbitraria (e.g. Framebuffer MMIO) alineando base y longitud a páginas */
int vmm_map_physical_range(struct page_directory* pd, uint64_t phys_addr, size_t size, uint32_t flags, uintptr_t* out_virt_addr);

/* Desasociar una página virtual */
void vmm_unmap_page(struct page_directory* pd, uintptr_t virt_addr);

/* Traducir una dirección virtual a su correspondiente física */
uintptr_t vmm_virt_to_phys(struct page_directory* pd, uintptr_t virt_addr);

/* Comprobar si una dirección virtual está mapeada */
bool vmm_is_mapped(struct page_directory* pd, uintptr_t virt_addr);

/* Modificar los flags/permisos de una página virtual ya mapeada e invalidar TLB */
int vmm_set_page_flags(struct page_directory* pd, uintptr_t virt_addr, uint32_t flags);

/* Obtener los flags de una página virtual mapeada (o 0 si no presente) */
uint32_t vmm_get_page_flags(struct page_directory* pd, uintptr_t virt_addr);

/* Comprobar si un directorio de páginas es accesible directamente en memoria baja (< 64 MiB) */
bool vmm_is_directory_accessible(struct page_directory* pd);

/* Obtener el límite superior en bytes del Identity Mapping supervisor (64 MiB) */
uintptr_t vmm_get_identity_limit(void);

/* Obtener el directorio de páginas maestro del kernel */
struct page_directory* vmm_get_kernel_directory(void);

/* Obtener el directorio de páginas actualmente activo */
struct page_directory* vmm_get_current_directory(void);

/* Crear un nuevo espacio de direcciones virtual para un proceso con kernel compartido */
struct page_directory* vmm_create_process_directory(void);

/* Destruir un espacio de direcciones de proceso liberando sus tablas de usuario */
void vmm_destroy_process_directory(struct page_directory* pd);

/* Conmutar el directorio activo cargando CR3 */
void vmm_switch_page_directory(struct page_directory* pd);

/* Obtener la dirección causante del último Page Fault (CR2) */
uint32_t vmm_get_fault_addr(void);

/* Invalidar una entrada específica del Translation Lookaside Buffer (TLB) */
void vmm_invlpg(uintptr_t addr);

/* Consultar si la paginación por hardware está activa */
bool vmm_is_paging_enabled(void);

#endif
