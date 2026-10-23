#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "ktypes.h"

#define MULTIBOOT1_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

/* Estructura Multiboot 1 provista por GRUB / bootloaders */
struct multiboot1_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
};

#define MULTIBOOT_INFO_MEMORY             (1 << 0)
#define MULTIBOOT_INFO_BOOTDEV            (1 << 1)
#define MULTIBOOT_INFO_CMDLINE            (1 << 2)
#define MULTIBOOT_INFO_MODS               (1 << 3)
#define MULTIBOOT_INFO_MEM_MAP            (1 << 6)
#define MULTIBOOT_INFO_VBE_INFO           (1 << 11)
#define MULTIBOOT_INFO_FRAMEBUFFER_INFO   (1 << 12)

/* Entrada de mapa de memoria según la especificación Multiboot 1 */
struct multiboot_mmap_entry {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} __attribute__((packed));

#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

/* Encabezado genérico de etiquetas Multiboot 2 */
struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
};

/* Etiqueta Multiboot 2 de memoria básica (type = 4) */
struct multiboot2_tag_basic_mem {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

/* Información consolidada y sanitizada de Multiboot */
struct multiboot_parsed_info {
    uint32_t version;       /* 1 para Multiboot 1, 2 para Multiboot 2, 0 si no coincide */
    uint32_t mem_lower_kib; /* Memoria baja convencional */
    uint32_t mem_upper_kib; /* Memoria extendida alta */
    uint32_t tags_count;    /* Total de etiquetas parseadas de forma segura */
    bool has_framebuffer;   /* true si hay framebuffer VESA/LFB disponible */
    uint64_t fb_addr;       /* Dirección física del Linear Framebuffer */
    uint32_t fb_width;      /* Ancho en píxeles */
    uint32_t fb_height;     /* Alto en píxeles */
    uint32_t fb_pitch;      /* Bytes por fila (scanline) */
    uint8_t  fb_bpp;        /* Bits por píxel (e.g. 32 bpp) */
    bool valid;             /* true si la firma y estructura fueron validadas */
};

/*
 * Parser robusto de Multiboot 1 y 2 con protección estricta contra
 * etiquetas malformadas, bucles infinitos (size < 8) y desbordamientos.
 */
static inline bool multiboot_parse(uint32_t magic, uintptr_t addr, struct multiboot_parsed_info* out) {
    if (!out) return false;
    out->version = 0;
    out->mem_lower_kib = 0;
    out->mem_upper_kib = 0;
    out->tags_count = 0;
    out->has_framebuffer = false;
    out->fb_addr = 0;
    out->fb_width = 0;
    out->fb_height = 0;
    out->fb_pitch = 0;
    out->fb_bpp = 0;
    out->valid = false;

    if (addr == 0) {
        return false;
    }

    // Caso 1: Multiboot 1 (0x2BADB002)
    if (magic == MULTIBOOT1_BOOTLOADER_MAGIC) {
        struct multiboot1_info* mbi = (struct multiboot1_info*)addr;
        out->version = 1;
        out->valid = true;
        if (mbi->flags & 0x01) {
            out->mem_lower_kib = mbi->mem_lower;
            out->mem_upper_kib = mbi->mem_upper;
        }
        if (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
            out->has_framebuffer = true;
            out->fb_addr = mbi->framebuffer_addr;
            out->fb_width = mbi->framebuffer_width;
            out->fb_height = mbi->framebuffer_height;
            out->fb_pitch = mbi->framebuffer_pitch;
            out->fb_bpp = mbi->framebuffer_bpp;
        }
        return true;
    }

    // Caso 2: Multiboot 2 (0x36D76289)
    if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        uint32_t total_size = *(uint32_t*)addr;
        // Validación de límites de tamaño de cabecera Multiboot 2
        if (total_size < 16 || total_size > 65536) {
            return false;
        }

        out->version = 2;
        out->valid = true;

        uint32_t offset = 8;
        while (offset + 8 <= total_size) {
            struct multiboot2_tag* tag = (struct multiboot2_tag*)(addr + offset);

            // Salvaguarda crítica: Si type == 0 (fin) o size < 8, abortar iteración
            // evitando bucle infinito por tag corrupto con size == 0
            if (tag->type == 0 || tag->size < 8) {
                break;
            }

            out->tags_count++;

            // Tag 4: Información básica de memoria
            if (tag->type == 4 && tag->size >= sizeof(struct multiboot2_tag_basic_mem)) {
                struct multiboot2_tag_basic_mem* mem = (struct multiboot2_tag_basic_mem*)tag;
                out->mem_lower_kib = mem->mem_lower;
                out->mem_upper_kib = mem->mem_upper;
            }

            // Alineación de cada tag a 8 bytes según la especificación Multiboot 2
            uint32_t aligned_size = (tag->size + 7) & ~7;
            if (aligned_size == 0 || offset + aligned_size < offset) {
                break; // Prevención de desbordamiento de entero
            }
            offset += aligned_size;
        }
        return true;
    }

    return false;
}

#endif
