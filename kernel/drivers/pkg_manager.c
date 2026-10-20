#include "pkg_manager.h"
#include "klib.h"
#include "vga.h"
#include "ramfs.h"

static struct package_info catalog[MAX_PACKAGES];
static size_t catalog_count = 0;

static void register_pkg(const char* id, const char* name, const char* ver, const char* desc, enum nova_profile prof, size_t size) {
    if (catalog_count >= MAX_PACKAGES) return;

    struct package_info* p = &catalog[catalog_count++];
    k_memset(p, 0, sizeof(struct package_info));

    size_t len = k_strlen(id);
    if (len >= 15) len = 15;
    k_memcpy(p->id, id, len);

    len = k_strlen(name);
    if (len >= 31) len = 31;
    k_memcpy(p->name, name, len);

    len = k_strlen(ver);
    if (len >= 11) len = 11;
    k_memcpy(p->version, ver, len);

    len = k_strlen(desc);
    if (len >= 63) len = 63;
    k_memcpy(p->description, desc, len);

    p->target_profile = prof;
    p->size_bytes = size;
    p->installed = false;
}

void pkg_init(void) {
    catalog_count = 0;
    k_memset(catalog, 0, sizeof(catalog));

    // Paquetes para perfil Estudiante
    register_pkg("notas", "NOVA Notes", "1.0", "Editor de notas de estudio y tareas", PROFILE_STUDENT, 512);
    register_pkg("cal", "NOVA Calendar", "1.0", "Agenda y calendario de entregas", PROFILE_STUDENT, 768);

    // Paquetes para perfil Desarrollador
    register_pkg("hexview", "Hex Viewer", "1.2", "Visor binario e inspeccion de memoria", PROFILE_DEVELOPER, 1024);
    register_pkg("asm-dbg", "ASM Debugger", "0.9", "Desensamblador y depurador basico", PROFILE_DEVELOPER, 2048);

    // Paquetes para perfil Hardware / IoT
    register_pkg("telemetry", "Sensor Plotter", "1.1", "Panel de metricas y graficos de Arduino", PROFILE_IOT_HARDWARE, 1536);
    register_pkg("relay-ctl", "Relay Controller", "1.0", "Control de salidas y actuadores de potencia", PROFILE_IOT_HARDWARE, 896);

    // Paquetes de perfil General
    register_pkg("calc", "NOVA Calc", "1.0", "Calculadora cientifica basica", PROFILE_GENERAL, 384);
}

void pkg_list_catalog(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Catalogo de Aplicaciones NOVA Store:\n");
    for (size_t i = 0; i < catalog_count; i++) {
        const char* status = catalog[i].installed ? "[INSTALADO]" : "[DISPONIBLE]";
        kprintf("  - %s (%s, v%s, %u bytes) %s\n    %s\n",
                catalog[i].id, catalog[i].name, catalog[i].version, 
                catalog[i].size_bytes, status, catalog[i].description);
    }
}

void pkg_list_installed(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Aplicaciones instaladas en el sistema:\n");
    int count = 0;
    for (size_t i = 0; i < catalog_count; i++) {
        if (catalog[i].installed) {
            kprintf("  * %s (%s, v%s)\n", catalog[i].id, catalog[i].name, catalog[i].version);
            count++;
        }
    }
    if (count == 0) {
        kprintf("  (ninguna aplicacion instalada aun)\n");
    }
}

void pkg_recommend(enum nova_profile profile) {
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("Aplicaciones recomendadas para tu perfil:\n");
    int count = 0;
    for (size_t i = 0; i < catalog_count; i++) {
        if (catalog[i].target_profile == profile && !catalog[i].installed) {
            kprintf("  -> %s: %s (v%s, %u bytes)\n",
                    catalog[i].id, catalog[i].name, catalog[i].version, catalog[i].size_bytes);
            count++;
        }
    }
    if (count == 0) {
        kprintf("  (todas las aplicaciones recomendadas ya estan instaladas)\n");
    }
}

int pkg_install(const char* pkg_id) {
    if (!pkg_id) return -1;

    for (size_t i = 0; i < catalog_count; i++) {
        if (k_strcmp(catalog[i].id, pkg_id) == 0) {
            if (catalog[i].installed) {
                return 1; // Ya estaba instalado
            }
            catalog[i].installed = true;

            // Registrar binario/metadata en el sistema de archivos RAMFS
            char filename[32];
            k_memset(filename, 0, sizeof(filename));
            size_t idx = 0;
            const char* prefix = "app_";
            for (size_t j = 0; prefix[j]; j++) filename[idx++] = prefix[j];
            for (size_t j = 0; catalog[i].id[j] && idx < 28; j++) filename[idx++] = catalog[i].id[j];
            filename[idx] = '\0';

            const char* app_meta = "NOVA_APP_EXEC_V1";
            ramfs_create(filename);
            ramfs_write(filename, (const uint8_t*)app_meta, k_strlen(app_meta));

            return 0; // Exito
        }
    }
    return -1; // Paquete no encontrado en catálogo
}

int pkg_uninstall(const char* pkg_id) {
    if (!pkg_id) return -1;

    for (size_t i = 0; i < catalog_count; i++) {
        if (k_strcmp(catalog[i].id, pkg_id) == 0) {
            if (!catalog[i].installed) {
                return 1; // No estaba instalado
            }
            catalog[i].installed = false;

            char filename[32];
            k_memset(filename, 0, sizeof(filename));
            size_t idx = 0;
            const char* prefix = "app_";
            for (size_t j = 0; prefix[j]; j++) filename[idx++] = prefix[j];
            for (size_t j = 0; catalog[i].id[j] && idx < 28; j++) filename[idx++] = catalog[i].id[j];
            filename[idx] = '\0';

            ramfs_delete(filename);
            return 0; // Desinstalado
        }
    }
    return -1; // No encontrado
}
