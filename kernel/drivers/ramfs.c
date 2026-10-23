#include "ramfs.h"
#include "klib.h"
#include "vga.h"

static struct ramfs_file files[RAMFS_MAX_FILES];

void ramfs_init(void) {
    k_memset(files, 0, sizeof(files));

    // Archivo de bienvenida predeterminado del sistema
    ramfs_create("bienvenida.txt");
    const char* default_msg = "Bienvenido a Zenthra OS (2026).\nSistema Operativo Experimental con soporte IoT Arduino.\n";
    ramfs_write("bienvenida.txt", (const uint8_t*)default_msg, k_strlen(default_msg));
}

int ramfs_create(const char* name) {
    if (!name || k_strlen(name) == 0 || k_strlen(name) >= RAMFS_MAX_NAME) return -1;

    // Verificar si ya existe
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && k_strcmp(files[i].name, name) == 0) {
            return -2; // Archivo ya existe
        }
    }

    // Buscar ranura libre
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = true;
            size_t len = k_strlen(name);
            k_memcpy(files[i].name, name, len + 1);
            files[i].size = 0;
            return 0; // Creado exitosamente
        }
    }
    return -3; // Sistema de archivos lleno
}

int ramfs_write(const char* name, const uint8_t* buffer, size_t size) {
    if (!name || !buffer) return -1;
    if (size > RAMFS_MAX_SIZE) size = RAMFS_MAX_SIZE;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && k_strcmp(files[i].name, name) == 0) {
            k_memcpy(files[i].data, buffer, size);
            files[i].size = size;
            return (int)size;
        }
    }
    return -1; // No encontrado
}

int ramfs_read(const char* name, uint8_t* buffer, size_t max_size) {
    if (!name || !buffer) return -1;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && k_strcmp(files[i].name, name) == 0) {
            size_t to_copy = (files[i].size < max_size) ? files[i].size : max_size;
            k_memcpy(buffer, files[i].data, to_copy);
            return (int)to_copy;
        }
    }
    return -1; // No encontrado
}

int ramfs_delete(const char* name) {
    if (!name) return -1;

    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && k_strcmp(files[i].name, name) == 0) {
            files[i].used = false;
            files[i].size = 0;
            files[i].name[0] = '\0';
            return 0;
        }
    }
    return -1;
}

void ramfs_list(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Archivos en RAMFS:\n");
    int count = 0;
    for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            kprintf("  - %s (%u bytes)\n", files[i].name, files[i].size);
            count++;
        }
    }
    if (count == 0) {
        kprintf("  (directorio vacio)\n");
    }
}
