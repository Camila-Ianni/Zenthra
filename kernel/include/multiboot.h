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
};

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
