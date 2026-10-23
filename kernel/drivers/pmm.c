#include "pmm.h"
#include "multiboot.h"
#include "klib.h"
#include "serial.h"
#include "vga.h"

/* Límite máximo soportado: 1 GiB de memoria física (262,144 marcos de 4 KiB) */
#define PMM_MAX_FRAMES 262144
#define PMM_BITMAP_SIZE (PMM_MAX_FRAMES / 32)

static uint32_t pmm_bitmap[PMM_BITMAP_SIZE];
static size_t total_memory_bytes = 0;
static size_t total_frames = 0;
static size_t used_frames = 0;

/* Símbolo definido por el linker script para marcar el fin del código/datos del kernel */
#if defined(__KERNEL__)
extern uint8_t _kernel_end;
#define GET_KERNEL_END() ((uintptr_t)&_kernel_end)
#else
#define GET_KERNEL_END() (0x00200000UL)
#endif

static inline void set_bit(size_t frame) {
    if (frame < total_frames) {
        if (!(pmm_bitmap[frame / 32] & (1U << (frame % 32)))) {
            pmm_bitmap[frame / 32] |= (1U << (frame % 32));
            used_frames++;
        }
    }
}

static inline void clear_bit(size_t frame) {
    if (frame < total_frames) {
        if (pmm_bitmap[frame / 32] & (1U << (frame % 32))) {
            pmm_bitmap[frame / 32] &= ~(1U << (frame % 32));
            if (used_frames > 0) {
                used_frames--;
            }
        }
    }
}

static inline bool test_bit(size_t frame) {
    if (frame >= total_frames) return true;
    return (pmm_bitmap[frame / 32] & (1U << (frame % 32))) != 0;
}

void pmm_mark_region_used(uintptr_t base, size_t length) {
    if (length == 0) return;
    size_t start_frame = base / PAGE_SIZE;
    size_t end_frame = (base + length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t frame = start_frame; frame < end_frame && frame < total_frames; frame++) {
        set_bit(frame);
    }
}

void pmm_mark_region_free(uintptr_t base, size_t length) {
    if (length == 0) return;
    size_t start_frame = base / PAGE_SIZE;
    size_t end_frame = (base + length) / PAGE_SIZE;

    for (size_t frame = start_frame; frame < end_frame && frame < total_frames; frame++) {
        clear_bit(frame);
    }
}

void pmm_init(uint32_t magic, uintptr_t mbi_addr) {
    // Por defecto soportamos hasta 128 MiB (32,768 marcos) salvo detección superior
    total_memory_bytes = 128 * 1024 * 1024;
    total_frames = total_memory_bytes / PAGE_SIZE;
    used_frames = total_frames;

    // Inicialmente marcar todos los marcos como ocupados/reservados
    k_memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));

    bool mmap_found = false;

    if (magic == MULTIBOOT1_BOOTLOADER_MAGIC && mbi_addr != 0) {
        struct multiboot1_info* mbi = (struct multiboot1_info*)mbi_addr;

        // Comprobar si GRUB entregó mapa de memoria completo
        if ((mbi->flags & (1 << 6)) && mbi->mmap_length > 0 && mbi->mmap_addr != 0) {
            uintptr_t mmap_curr = mbi->mmap_addr;
            uintptr_t mmap_end = mbi->mmap_addr + mbi->mmap_length;

            while (mmap_curr + sizeof(uint32_t) <= mmap_end) {
                struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)mmap_curr;
                uint32_t entry_total_size = entry->size + sizeof(uint32_t);

                // Validar que el tamaño de la entrada sea consistente y no desborde mmap_end
                if (entry->size < (sizeof(struct multiboot_mmap_entry) - sizeof(uint32_t)) ||
                    mmap_curr + entry_total_size > mmap_end ||
                    mmap_curr + entry_total_size < mmap_curr) {
                    break;
                }

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    uint64_t base64 = ((uint64_t)entry->addr_high << 32) | (uint64_t)entry->addr_low;
                    uint64_t len64  = ((uint64_t)entry->len_high << 32) | (uint64_t)entry->len_low;
                    uint64_t max_limit = (uint64_t)PMM_MAX_FRAMES * PAGE_SIZE;

                    // Ignorar rangos corruptos o con overflow
                    if (base64 + len64 < base64 || len64 == 0) {
                        mmap_curr += entry_total_size;
                        continue;
                    }

                    // Ignorar memoria física ubicada íntegramente por encima de 1 GiB
                    if (base64 >= max_limit) {
                        mmap_curr += entry_total_size;
                        continue;
                    }

                    // Recortar longitud si se extiende más allá de 1 GiB
                    if (base64 + len64 > max_limit) {
                        len64 = max_limit - base64;
                    }

                    uintptr_t base = (uintptr_t)base64;
                    size_t len = (size_t)len64;

                    if (base + len > total_memory_bytes) {
                        total_memory_bytes = base + len;
                        if (total_memory_bytes > (size_t)PMM_MAX_FRAMES * PAGE_SIZE) {
                            total_memory_bytes = (size_t)PMM_MAX_FRAMES * PAGE_SIZE;
                        }
                        total_frames = total_memory_bytes / PAGE_SIZE;
                    }

                    pmm_mark_region_free(base, len);
                    mmap_found = true;
                }

                mmap_curr += entry_total_size;
            }
        } else if (mbi->flags & (1 << 0)) {
            // Fallback: usar mem_lower y mem_upper de Multiboot 1
            total_memory_bytes = 1024 * 1024 + (size_t)mbi->mem_upper * 1024;
            if (total_memory_bytes > (size_t)PMM_MAX_FRAMES * PAGE_SIZE) {
                total_memory_bytes = (size_t)PMM_MAX_FRAMES * PAGE_SIZE;
            }
            total_frames = total_memory_bytes / PAGE_SIZE;
            pmm_mark_region_free(0x00100000, (size_t)mbi->mem_upper * 1024);
            mmap_found = true;
        }
    }

    if (!mmap_found) {
        // Modo fallback para pruebas en host o emulación sin mapa de memoria
        total_memory_bytes = 64 * 1024 * 1024; // 64 MiB
        total_frames = total_memory_bytes / PAGE_SIZE;
        pmm_mark_region_free(0x00100000, 63 * 1024 * 1024);
    }

    // 1. Reservar el primer megabyte (0x00000000 - 0x000FFFFF: IVT, BDA, VGA B8000, ROM BIOS)
    pmm_mark_region_used(0x00000000, 0x00100000);

    // 2. Reservar la imagen del Kernel ELF (_kernel_end)
    uintptr_t kernel_end_addr = GET_KERNEL_END();
    if (kernel_end_addr <= 0x00100000) {
        kernel_end_addr = 0x00200000; // Reserva mínima de 1 MiB para kernel
    }
    pmm_mark_region_used(0x00100000, kernel_end_addr - 0x00100000);

    // 3. Reservar la región asignada para el Kernel Heap (0x00400000 - 0x00600000: 2 MiB)
    pmm_mark_region_used(0x00400000, 2 * 1024 * 1024);

    // 4. Reservar las estructuras de Multiboot y módulos
    if (mbi_addr != 0) {
        pmm_mark_region_used(mbi_addr, sizeof(struct multiboot1_info));
        struct multiboot1_info* mbi = (struct multiboot1_info*)mbi_addr;
        if ((mbi->flags & (1 << 6)) && mbi->mmap_addr != 0 && mbi->mmap_length > 0) {
            pmm_mark_region_used(mbi->mmap_addr, mbi->mmap_length);
        }
        if ((mbi->flags & (1 << 3)) && mbi->mods_count > 0 && mbi->mods_addr != 0) {
            pmm_mark_region_used((uintptr_t)mbi->mods_addr, mbi->mods_count * sizeof(struct multiboot_mod_list));
            struct multiboot_mod_list* mods = (struct multiboot_mod_list*)(uintptr_t)mbi->mods_addr;
            for (uint32_t m = 0; m < mbi->mods_count; m++) {
                if (mods[m].mod_end > mods[m].mod_start) {
                    pmm_mark_region_used(mods[m].mod_start, mods[m].mod_end - mods[m].mod_start);
                }
            }
        }
        if ((mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) && mbi->framebuffer_addr != 0) {
            uint64_t fb_addr = mbi->framebuffer_addr;
            uint64_t fb_size = (uint64_t)mbi->framebuffer_pitch * (uint64_t)mbi->framebuffer_height;
            if (fb_addr < (uint64_t)total_memory_bytes && fb_size > 0) {
                pmm_mark_region_used((uintptr_t)fb_addr, (size_t)fb_size);
            }
        }
    }

    // Recontar con precisión exacta los marcos ocupados
    used_frames = 0;
    for (size_t i = 0; i < total_frames; i++) {
        if (test_bit(i)) {
            used_frames++;
        }
    }

    char buf[32];
    serial_write_string("[PMM] Memoria fisica detectada: ");
    uitoa((uint32_t)(total_memory_bytes / (1024 * 1024)), buf, 10);
    serial_write_string(buf);
    serial_write_string(" MiB (Marcos libres: ");
    uitoa((uint32_t)pmm_get_free_frames(), buf, 10);
    serial_write_string(buf);
    serial_write_string(")\r\n");
}

uintptr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < total_frames / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (size_t b = 0; b < 32; b++) {
                if (!(pmm_bitmap[i] & (1U << b))) {
                    size_t frame = i * 32 + b;
                    if (frame < total_frames) {
                        set_bit(frame);
                        return (uintptr_t)(frame * PAGE_SIZE);
                    }
                }
            }
        }
    }
    return 0; // Sin memoria física disponible
}

uintptr_t pmm_alloc_frame_below(uintptr_t max_limit) {
    size_t max_frames = max_limit / PAGE_SIZE;
    if (max_frames > total_frames) max_frames = total_frames;

    for (size_t i = 0; i < (max_frames + 31) / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (size_t b = 0; b < 32; b++) {
                if (!(pmm_bitmap[i] & (1U << b))) {
                    size_t frame = i * 32 + b;
                    if (frame < max_frames) {
                        set_bit(frame);
                        return (uintptr_t)(frame * PAGE_SIZE);
                    }
                }
            }
        }
    }
    return 0; // Sin memoria física disponible bajo el límite
}

bool pmm_is_region_reserved(uintptr_t base, size_t length) {
    if (length == 0) return true;
    size_t start_frame = base / PAGE_SIZE;
    size_t end_frame = (base + length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t frame = start_frame; frame < end_frame && frame < total_frames; frame++) {
        if (!test_bit(frame)) {
            return false;
        }
    }
    return true;
}

void pmm_free_frame(uintptr_t frame_addr) {
    if ((frame_addr % PAGE_SIZE) != 0) return;
    size_t frame = frame_addr / PAGE_SIZE;
    clear_bit(frame);
}

bool pmm_is_frame_allocated(uintptr_t frame_addr) {
    size_t frame = frame_addr / PAGE_SIZE;
    return test_bit(frame);
}

size_t pmm_get_total_frames(void) {
    return total_frames;
}

size_t pmm_get_free_frames(void) {
    if (total_frames < used_frames) return 0;
    return total_frames - used_frames;
}

size_t pmm_get_used_frames(void) {
    return used_frames;
}

size_t pmm_get_total_memory_kib(void) {
    return (total_frames * PAGE_SIZE) / 1024;
}

size_t pmm_get_free_memory_kib(void) {
    return (pmm_get_free_frames() * PAGE_SIZE) / 1024;
}
