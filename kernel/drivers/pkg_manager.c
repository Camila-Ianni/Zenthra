#include "pkg_manager.h"
#include "klib.h"
#include "vga.h"
#include "ramfs.h"

static struct package_info catalog[MAX_PACKAGES];
static size_t catalog_count = 0;

/* Implementación estándar de CRC32 (IEEE 802.3 polinomial 0xEDB88320) */
uint32_t pkg_compute_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        crc ^= byte;
        for (int j = 0; j < 8; j++) {
            uint32_t mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

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

    // Calcular firma CRC32 estándar sobre el binario/payload predeterminado
    const char* default_payload = "ZENTHRA_APP_EXEC_V1";
    p->expected_crc32 = pkg_compute_crc32((const uint8_t*)default_payload, k_strlen(default_payload));
    p->installed = false;
}

bool pkg_verify_integrity(const char* pkg_id, const uint8_t* data, size_t length) {
    if (!pkg_id || !data) return false;

    for (size_t i = 0; i < catalog_count; i++) {
        if (k_strcmp(catalog[i].id, pkg_id) == 0) {
            uint32_t computed = pkg_compute_crc32(data, length);
            return (computed == catalog[i].expected_crc32);
        }
    }
    return false;
}

void pkg_init(void) {
    catalog_count = 0;
    k_memset(catalog, 0, sizeof(catalog));

    // 1. Perfil Estudiante (Ofimática, apuntes, lecturas)
    register_pkg("office", "Zenthra Office Suite", "2.1", "Procesador de textos, calculo y diapositivas", PROFILE_STUDENT, 4096);
    register_pkg("pdf-reader", "PDF Scholar", "1.4", "Lector de documentos academicos y anotaciones", PROFILE_STUDENT, 1024);
    register_pkg("flashcards", "AnkiZenthra Cards", "1.0", "Tarjetas mnemotecnicas de estudio espaciado", PROFILE_STUDENT, 512);

    // 2. Perfil Desarrollador Front-end (Figma, VS Code, Node, Browser)
    register_pkg("vscode", "VS Code Core", "1.85", "Editor de codigo extensible con resaltado", PROFILE_FRONTEND, 3072);
    register_pkg("figma", "Figma Desktop", "2.0", "Editor de interfaces UI/UX y prototipado", PROFILE_FRONTEND, 2048);
    register_pkg("nodejs", "Node.js Runtime", "20.1", "Entorno JavaScript/TypeScript y gestor npm", PROFILE_FRONTEND, 2560);
    register_pkg("dev-browser", "Zenthra Web Browser", "1.0", "Navegador con inspector de elementos y consola", PROFILE_FRONTEND, 4096);

    // 3. Perfil Desarrollador Back-end (Docker, DBs, Postman, Go/Python)
    register_pkg("docker", "Docker Engine", "24.0", "Gestor de contenedores y microservicios", PROFILE_BACKEND, 4096);
    register_pkg("postgres", "PostgreSQL Client", "16.0", "Cliente SQL y visor relacional de bases de datos", PROFILE_BACKEND, 2048);
    register_pkg("api-postman", "Zenthra API Client", "1.2", "Probador de endpoints REST/GraphQL", PROFILE_BACKEND, 1536);
    register_pkg("git-cli", "Git Version Control", "2.42", "Control de versiones distribuido", PROFILE_BACKEND, 1024);

    // 4. Perfil Desarrollador Full-Stack (Integración completa)
    register_pkg("fullstack-sdk", "FullStack SDK Suite", "1.0", "Compiladores C/Rust, Node.js y Docker integrados", PROFILE_FULLSTACK, 5120);
    register_pkg("terminal-pro", "Zenthra Terminal Pro", "2.0", "Multiplexor de terminales con pestañas", PROFILE_FULLSTACK, 1024);

    // 5. Perfil Diseñador / Creativo (Edición visual, vectores, paletas)
    register_pkg("vector-draw", "Vector Master", "1.5", "Ilustracion vectorial y diseno de logos", PROFILE_DESIGNER, 3584);
    register_pkg("photo-editor", "PixelForge Editor", "1.1", "Retoque fotografico y mapas de bits", PROFILE_DESIGNER, 4096);
    register_pkg("font-manager", "TypeVault Fonts", "1.0", "Organizador y previsualizador de fuentes", PROFILE_DESIGNER, 768);

    // 6. Perfil Hardware / IoT (Herramientas Arduino, Serie, Osciloscopio)
    register_pkg("arduino-ide", "Arduino Zenthra Studio", "2.2", "Entorno de compilacion y flasheo de placas", PROFILE_IOT_HARDWARE, 3072);
    register_pkg("serial-mon", "UART Logic Analyzer", "1.3", "Monitor de puertos COM con graficos de senales", PROFILE_IOT_HARDWARE, 1536);
    register_pkg("relay-ctl", "Power Actuator Hub", "1.0", "Control de reles y conmutacion digital", PROFILE_IOT_HARDWARE, 896);

    // 7. Perfil Científico de Datos / IA (Jupyter, Python, Visualización)
    register_pkg("jupyter-nb", "Zenthra Notebooks", "3.0", "Cuadernos interactivos para analisis de datos", PROFILE_DATA_SCIENTIST, 3584);
    register_pkg("stats-plot", "DataPlot Lib", "1.2", "Generador de diagramas y graficos estadisticos", PROFILE_DATA_SCIENTIST, 1792);

    // 8. Perfil Ciberseguridad / Pentesting (Wireshark, Scanner de puertos)
    register_pkg("net-scanner", "PortScan & Auditing", "2.0", "Analizador de puertos, servicios y sockets", PROFILE_CYBERSECURITY, 1536);
    register_pkg("pkt-analyzer", "Packet Sniffer", "1.4", "Captura e inspeccion de paquetes de red", PROFILE_CYBERSECURITY, 2048);

    // 9. Perfil Ofimática / Administración (Hojas de cálculo, Contabilidad)
    register_pkg("calc-pro", "Zenthra Spreadsheets", "2.0", "Planillas de calculo con tablas dinamicas", PROFILE_OFFICE_ADMIN, 2048);
    register_pkg("invoice-mgr", "Billing & Invoices", "1.0", "Gestion de facturacion y contabilidad local", PROFILE_OFFICE_ADMIN, 1024);

    // 10. Perfil Gamer / Streamer (Grabador, Overlay, Optimizador de FPS)
    register_pkg("game-launcher", "GameHub Zenthra", "1.0", "Lanzador de juegos y optimizador de rendimiento", PROFILE_GAMER_STREAMER, 2048);
    register_pkg("stream-obs", "OBS Zenthra Streamer", "1.2", "Captura de pantalla, audio y streaming en vivo", PROFILE_GAMER_STREAMER, 4096);

    // Perfil General / Usuario Diario
    register_pkg("media-player", "Zenthra Media Player", "1.1", "Reproductor de audio y video multiformato", PROFILE_GENERAL, 1024);
    register_pkg("calc", "Zenthra Basic Calc", "1.0", "Calculadora basica de escritorio", PROFILE_GENERAL, 384);
}

void pkg_list_catalog(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Catalogo de Aplicaciones Zenthra Store (%u disponibles):\n", catalog_count);
    for (size_t i = 0; i < catalog_count; i++) {
        const char* status = catalog[i].installed ? "[INSTALADO]" : "[DISPONIBLE]";
        kprintf("  - %s (%s, v%s, %u B) %s\n    %s\n",
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
            kprintf("  * %s (%s, v%s, %u bytes)\n", catalog[i].id, catalog[i].name, catalog[i].version, catalog[i].size_bytes);
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
            kprintf("  -> %s: %s (v%s, %u bytes)\n     %s\n",
                    catalog[i].id, catalog[i].name, catalog[i].version, catalog[i].size_bytes, catalog[i].description);
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
            // Verificación obligatoria de integridad por Checksum CRC32
            const char* app_meta = "ZENTHRA_APP_EXEC_V1";
            if (!pkg_verify_integrity(pkg_id, (const uint8_t*)app_meta, k_strlen(app_meta))) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("\n[ERROR DE SEGURIDAD] Fallo de integridad CRC32 en paquete '%s'. Instalacion cancelada.\n", pkg_id);
                return -2; // Fallo de verificación de integridad
            }

            // Registrar binario/metadata en el sistema de archivos RAMFS
            char filename[32];
            k_memset(filename, 0, sizeof(filename));
            size_t idx = 0;
            const char* prefix = "app_";
            for (size_t j = 0; prefix[j]; j++) filename[idx++] = prefix[j];
            for (size_t j = 0; catalog[i].id[j] && idx < 28; j++) filename[idx++] = catalog[i].id[j];
            filename[idx] = '\0';

            // Comprobar retorno de creación en RAMFS
            int c_res = ramfs_create(filename);
            if (c_res != 0 && c_res != -2) {
                // Error al crear archivo (e.g. filesystem lleno)
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("\n[ERROR RAMFS] No se pudo crear archivo para paquete '%s' (FS lleno o error %d).\n", pkg_id, c_res);
                return -3;
            }

            size_t meta_len = k_strlen(app_meta);
            int written = ramfs_write(filename, (const uint8_t*)app_meta, meta_len);
            if (written < 0 || (size_t)written != meta_len) {
                // Escritura incompleta o fallida: limpiar y no marcar como instalado
                if (c_res == 0) {
                    ramfs_delete(filename);
                }
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("\n[ERROR RAMFS] Fallo al escribir metadata de paquete '%s'.\n", pkg_id);
                return -4;
            }

            // Marcar como instalado únicamente tras éxito completo en RAMFS
            catalog[i].installed = true;
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

int pkg_provision_profile(enum nova_profile profile) {
    int installed_count = 0;
    for (size_t i = 0; i < catalog_count; i++) {
        if (catalog[i].target_profile == profile && !catalog[i].installed) {
            vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            kprintf("  [Aprovisionando] Descargando e instalando %s (%s)... ", catalog[i].id, catalog[i].name);
            if (pkg_install(catalog[i].id) == 0) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[LISTO]\n");
                installed_count++;
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ERROR]\n");
            }
        }
    }
    return installed_count;
}
