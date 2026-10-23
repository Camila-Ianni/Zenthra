#include "vmm.h"
#include "page_fault.h"
#include "klib.h"
#include "serial.h"
#include "vga.h"

static struct page_directory kernel_pdir __attribute__((aligned(4096)));
static struct page_table kernel_pts[KERNEL_IDENTITY_TABLES] __attribute__((aligned(4096)));
static struct page_directory* current_pdir = &kernel_pdir;
static bool paging_enabled = false;

uintptr_t vmm_get_identity_limit(void) {
    return KERNEL_IDENTITY_LIMIT;
}

#if !defined(__KERNEL__)
#include <stdlib.h>
extern uint32_t mock_cr2_addr;
static void* host_user_tables[ENTRIES_PER_PAGE_DIR];
#endif

bool vmm_is_directory_accessible(struct page_directory* pd) {
    if (!pd) return false;
#if defined(__KERNEL__)
    // Durante paginación activa en Ring 0, solo las direcciones en el Identity Map (0 .. 64 MiB) son directamente desreferenciables
    uintptr_t addr = (uintptr_t)pd;
    if (addr >= KERNEL_IDENTITY_LIMIT) return false;
#else
    // En host tests, rechazar punteros nulos o claramente inválidos (< 4 KiB)
    if ((uintptr_t)pd < 0x1000) return false;
#endif
    return true;
}

static inline struct page_table* vmm_get_pt(struct page_directory* pd, size_t pde_idx) {
    if (!pd || !vmm_is_directory_accessible(pd) || pde_idx >= ENTRIES_PER_PAGE_DIR) return NULL;
#if defined(__KERNEL__)
    if (pde_idx < KERNEL_IDENTITY_TABLES) {
        return &kernel_pts[pde_idx];
    }
    if (!(pd->entries[pde_idx] & PDE_PRESENT)) return NULL;
    uintptr_t pt_phys = pd->entries[pde_idx] & PDE_FRAME_MASK;
    // Si la tabla de páginas está dentro del Identity Mapping (primeros 64 MiB), su puntero físico es directamente accesible
    if (pt_phys != 0 && pt_phys < KERNEL_IDENTITY_LIMIT) {
        return (struct page_table*)pt_phys;
    }
    return NULL; // Tabla fuera de la ventana directamente mapeada: acceso no seguro
#else
    if (pde_idx < KERNEL_IDENTITY_TABLES) {
        return &kernel_pts[pde_idx];
    }
    if (!(pd->entries[pde_idx] & PDE_PRESENT)) return NULL;
    return (struct page_table*)host_user_tables[pde_idx];
#endif
}

uint32_t vmm_get_fault_addr(void) {
#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    uint32_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
#else
    return mock_cr2_addr;
#endif
}

void vmm_invlpg(uintptr_t addr) {
#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
#else
    (void)addr;
#endif
}

struct page_directory* vmm_get_kernel_directory(void) {
    return &kernel_pdir;
}

bool vmm_is_paging_enabled(void) {
    return paging_enabled;
}

void vmm_switch_page_directory(struct page_directory* pd) {
    if (!pd) {
        pd = &kernel_pdir;
    }
    current_pdir = pd;

#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    uintptr_t phys_addr = (uintptr_t)pd;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(phys_addr) : "memory");
#endif
}

int vmm_map_page(struct page_directory* pd, uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags) {
    if (!pd || !vmm_is_directory_accessible(pd)) return -1;

    size_t pde_idx = (virt_addr >> 22) & 0x3FF;
    size_t pte_idx = (virt_addr >> 12) & 0x3FF;

    // Proteger estrictamente los primeros 64 MiB: denegar mapeos de usuario (PTE_USER) en el espacio del kernel
    if ((flags & PTE_USER) && pde_idx < KERNEL_IDENTITY_TABLES) {
        return -1; // Acceso denegado: el espacio del kernel no admite páginas de usuario
    }

    // Si la tabla de páginas no existe en el directorio, asignar un nuevo marco físico garantizado en memoria accesible (< KERNEL_IDENTITY_LIMIT)
    if (!(pd->entries[pde_idx] & PDE_PRESENT)) {
#if defined(__KERNEL__)
        uintptr_t pt_frame = pmm_alloc_frame_below(KERNEL_IDENTITY_LIMIT);
        if (pt_frame == 0) {
            return -1; // Fallo de memoria física (OOM en memoria baja)
        }
        struct page_table* new_pt = (struct page_table*)pt_frame;
#else
        extern void* malloc(size_t);
        struct page_table* new_pt = (struct page_table*)malloc(sizeof(struct page_table));
        if (!new_pt) {
            return -1;
        }
        host_user_tables[pde_idx] = new_pt;
        uintptr_t pt_frame = (uintptr_t)new_pt;
#endif
        k_memset(new_pt, 0, sizeof(struct page_table));

        uint32_t pde_flags = PDE_PRESENT | PDE_WRITABLE;
        if (flags & PTE_USER) {
            pde_flags |= PDE_USER;
        }

        pd->entries[pde_idx] = (pt_frame & PDE_FRAME_MASK) | pde_flags;
    } else {
        // Si la página a mapear es de usuario, propagar el bit USER a la entrada del directorio
        if (flags & PTE_USER) {
            pd->entries[pde_idx] |= PDE_USER;
        }
    }

    struct page_table* pt = vmm_get_pt(pd, pde_idx);
    if (!pt) return -1;

    // Política explícita de ownership (Auditoría v10/v11):
    // Si la página ya está presente, rechazar colisión para forzar uso explícito de vmm_set_page_flags o vmm_unmap_page
    if (pt->entries[pte_idx] & PTE_PRESENT) {
        return -1; // Colisión: dirección virtual ya mapeada
    }

    pt->entries[pte_idx] = (phys_addr & PTE_FRAME_MASK) | (flags & 0xFFF) | PTE_PRESENT;

    if (paging_enabled && pd == current_pdir) {
        vmm_invlpg(virt_addr);
    }

    return 0;
}

int vmm_set_page_flags(struct page_directory* pd, uintptr_t virt_addr, uint32_t flags) {
    if (!pd || !vmm_is_directory_accessible(pd)) return -1;

    size_t pde_idx = (virt_addr >> 22) & 0x3FF;
    size_t pte_idx = (virt_addr >> 12) & 0x3FF;

    if (!(pd->entries[pde_idx] & PDE_PRESENT)) return -1;

    struct page_table* pt = vmm_get_pt(pd, pde_idx);
    if (!pt || !(pt->entries[pte_idx] & PTE_PRESENT)) return -1;

    // Proteger espacio de kernel contra permisos PTE_USER
    if ((flags & PTE_USER) && pde_idx < KERNEL_IDENTITY_TABLES) {
        return -1;
    }

    uintptr_t phys_frame = pt->entries[pte_idx] & PTE_FRAME_MASK;
    pt->entries[pte_idx] = phys_frame | (flags & 0xFFF) | PTE_PRESENT;

    if (flags & PTE_USER) {
        pd->entries[pde_idx] |= PDE_USER;
    }

    if (paging_enabled && pd == current_pdir) {
        vmm_invlpg(virt_addr);
    }
    return 0;
}

uint32_t vmm_get_page_flags(struct page_directory* pd, uintptr_t virt_addr) {
    if (!pd || !vmm_is_directory_accessible(pd)) return 0;

    size_t pde_idx = (virt_addr >> 22) & 0x3FF;
    size_t pte_idx = (virt_addr >> 12) & 0x3FF;

    if (!(pd->entries[pde_idx] & PDE_PRESENT)) return 0;

    struct page_table* pt = vmm_get_pt(pd, pde_idx);
    if (!pt || !(pt->entries[pte_idx] & PTE_PRESENT)) return 0;

    return pt->entries[pte_idx] & 0xFFF;
}

struct page_directory* vmm_get_current_directory(void) {
    return current_pdir;
}

int vmm_map_range(struct page_directory* pd, uintptr_t virt_start, uintptr_t phys_start, size_t size, uint32_t flags) {
    if (!pd || !vmm_is_directory_accessible(pd) || size == 0) return -1;

    uint64_t v_end = (uint64_t)virt_start + (uint64_t)size;
    uint64_t p_end = (uint64_t)phys_start + (uint64_t)size;
    if (v_end < (uint64_t)virt_start || v_end > 0x100000000ULL) return -1;
    if (p_end < (uint64_t)phys_start || p_end > 0x100000000ULL) return -1;

    uint64_t v_page_start = (uint64_t)(virt_start & PTE_FRAME_MASK);
    uint64_t p_page_start = (uint64_t)(phys_start & PTE_FRAME_MASK);
    uint64_t v_page_end = (v_end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t p_page_end = (p_end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    if (v_page_end > 0x100000000ULL || p_page_end > 0x100000000ULL) return -1;

    size_t aligned_size = (size_t)(v_page_end - v_page_start);
    uintptr_t v = (uintptr_t)v_page_start;
    uintptr_t p = (uintptr_t)p_page_start;

    for (size_t offset = 0; offset < aligned_size; offset += PAGE_SIZE) {
        int res = vmm_map_page(pd, v + offset, p + offset, flags);
        if (res != 0) return res;
    }
    return 0;
}

int vmm_map_physical_range(struct page_directory* pd, uint64_t phys_addr, size_t size, uint32_t flags, uintptr_t* out_virt_addr) {
    if (!pd || !vmm_is_directory_accessible(pd) || size == 0 || !out_virt_addr) return -1;

    // Validación estricta en aritmética de 64 bits contra overflow y límites de arquitectura de 32 bits (i686)
    uint64_t phys_end = phys_addr + (uint64_t)size;
    if (phys_end < phys_addr || phys_end > 0x100000000ULL) {
        return -1; // Dirección física fuera del espacio direccionable de 32 bits
    }

    uint64_t phys_page_start = phys_addr & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t phys_page_end = (phys_end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    if (phys_page_end < phys_end || phys_page_end > 0x100000000ULL) {
        return -1;
    }

    uint64_t total_span_64 = phys_page_end - phys_page_start;
    if (total_span_64 > 0xFFFFFFFFULL || total_span_64 == 0) {
        return -1;
    }

    uintptr_t p_page_start = (uintptr_t)phys_page_start;
    uintptr_t page_offset = (uintptr_t)(phys_addr & (PAGE_SIZE - 1));
    size_t total_span = (size_t)total_span_64;

    // Mapear identity la región física en el espacio virtual del kernel
    int res = vmm_map_range(pd, p_page_start, p_page_start, total_span, flags);
    if (res != 0) return res;

    *out_virt_addr = p_page_start + page_offset;
    return 0;
}

void vmm_unmap_page(struct page_directory* pd, uintptr_t virt_addr) {
    if (!pd || !vmm_is_directory_accessible(pd)) return;

    size_t pde_idx = (virt_addr >> 22) & 0x3FF;
    size_t pte_idx = (virt_addr >> 12) & 0x3FF;

    if (pd->entries[pde_idx] & PDE_PRESENT) {
        struct page_table* pt = vmm_get_pt(pd, pde_idx);
        if (pt) {
            pt->entries[pte_idx] = 0;
        }

        if (paging_enabled && pd == current_pdir) {
            vmm_invlpg(virt_addr);
        }
    }
}

uintptr_t vmm_virt_to_phys(struct page_directory* pd, uintptr_t virt_addr) {
    if (!pd || !vmm_is_directory_accessible(pd)) return 0;

    size_t pde_idx = (virt_addr >> 22) & 0x3FF;
    size_t pte_idx = (virt_addr >> 12) & 0x3FF;

    if (!(pd->entries[pde_idx] & PDE_PRESENT)) {
        return 0;
    }

    struct page_table* pt = vmm_get_pt(pd, pde_idx);
    if (!pt || !(pt->entries[pte_idx] & PTE_PRESENT)) {
        return 0;
    }

    return (pt->entries[pte_idx] & PTE_FRAME_MASK) | (virt_addr & 0xFFF);
}

bool vmm_is_mapped(struct page_directory* pd, uintptr_t virt_addr) {
    return vmm_virt_to_phys(pd, virt_addr) != 0;
}

struct page_directory* vmm_create_process_directory(void) {
#if defined(__KERNEL__)
    uintptr_t pd_frame = pmm_alloc_frame_below(KERNEL_IDENTITY_LIMIT);
    if (pd_frame == 0) return NULL;
    struct page_directory* new_pd = (struct page_directory*)pd_frame;
#else
    extern void* malloc(size_t);
    struct page_directory* new_pd = (struct page_directory*)malloc(sizeof(struct page_directory));
    if (!new_pd) return NULL;
#endif
    k_memset(new_pd, 0, sizeof(struct page_directory));

    // Clonar las entradas maestras del kernel (primeros 64 MiB) para preservar handlers, IDT y kernel
    for (size_t i = 0; i < KERNEL_IDENTITY_TABLES; i++) {
        new_pd->entries[i] = kernel_pdir.entries[i];
    }

    return new_pd;
}

void vmm_destroy_process_directory(struct page_directory* pd) {
    if (!pd || !vmm_is_directory_accessible(pd) || pd == &kernel_pdir) return;

#if defined(__KERNEL__)
    size_t ram_bytes = pmm_get_total_memory_kib() * 1024;
#endif

    // Liberar marcos de usuario y tablas de páginas asignadas en el espacio privado (índices 16 a 1023)
    for (size_t i = KERNEL_IDENTITY_TABLES; i < ENTRIES_PER_PAGE_DIR; i++) {
        if (pd->entries[i] & PDE_PRESENT) {
            struct page_table* pt = vmm_get_pt(pd, i);
            if (pt) {
                for (size_t pte_idx = 0; pte_idx < ENTRIES_PER_PAGE_TABLE; pte_idx++) {
                    if ((pt->entries[pte_idx] & PTE_PRESENT) && (pt->entries[pte_idx] & PTE_USER)) {
#if defined(__KERNEL__)
                        uintptr_t page_frame = pt->entries[pte_idx] & PTE_FRAME_MASK;
                        // Solo liberar si pertenece a la RAM administrada por PMM y no es MMIO externo
                        if (page_frame < ram_bytes && pmm_is_frame_allocated(page_frame)) {
                            pmm_free_frame(page_frame);
                        }
#endif
                        pt->entries[pte_idx] = 0;
                    }
                }
            }

#if defined(__KERNEL__)
            uintptr_t pt_frame = pd->entries[i] & PDE_FRAME_MASK;
            if (pt_frame < ram_bytes && pmm_is_frame_allocated(pt_frame)) {
                pmm_free_frame(pt_frame);
            }
#else
            if (host_user_tables[i]) {
                free(host_user_tables[i]);
                host_user_tables[i] = NULL;
            }
#endif
        }
    }

#if defined(__KERNEL__)
    uintptr_t pd_frame = (uintptr_t)pd;
    if (pd_frame < ram_bytes && pmm_is_frame_allocated(pd_frame)) {
        pmm_free_frame(pd_frame);
    }
#else
    free(pd);
#endif
}

void vmm_init(void) {
    // 1. Limpiar directorio maestro del kernel
    k_memset(&kernel_pdir, 0, sizeof(struct page_directory));

    // 2. Mapear de forma idéntica los primeros 64 MiB (Ring 0 / Supervisor)
    // Cubre: IVT, BDA, VGA B8000, código del kernel, heap (4-6 MB), tablas de paginación y buffers iniciales
    for (size_t t = 0; t < KERNEL_IDENTITY_TABLES; t++) {
        for (size_t i = 0; i < ENTRIES_PER_PAGE_TABLE; i++) {
            uintptr_t phys_addr = (t * ENTRIES_PER_PAGE_TABLE + i) * PAGE_SIZE;
            kernel_pts[t].entries[i] = (phys_addr & PTE_FRAME_MASK) | PTE_PRESENT | PTE_WRITABLE;
        }
        kernel_pdir.entries[t] = ((uintptr_t)&kernel_pts[t] & PDE_FRAME_MASK) | PDE_PRESENT | PDE_WRITABLE;
    }

    // 3. Registrar el manejador de Page Fault en la IDT
    page_fault_init();

    // 4. Activar paginación por hardware (CR3 y CR0)
    current_pdir = &kernel_pdir;

#if defined(__KERNEL__) && (defined(__i386__) || defined(__x86_64__))
    uintptr_t pd_phys = (uintptr_t)&kernel_pdir;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys) : "memory");

    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010000; // Bit 31: PG (Paging), Bit 16: WP (Write Protect)
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
#endif

    paging_enabled = true;

    serial_write_string("[VMM] Paginacion x86 (CR0.PG y CR0.WP) activada exitosamente.\r\n");
    serial_write_string("[VMM] Directorio maestro cargado en CR3 con 64 MiB en Identity Mapping.\r\n");
}
