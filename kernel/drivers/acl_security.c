#include "acl_security.h"
#include "klib.h"
#include "vga.h"
#include "ramfs.h"

/* Máscara de permisos por perfil */
static uint8_t profile_acl[PROFILE_COUNT];
static uint32_t security_violations_count = 0;
static enum nova_profile active_profile = PROFILE_GENERAL;

enum nova_profile acl_get_active_profile(void) {
    return active_profile;
}

void acl_set_active_profile(enum nova_profile profile) {
    if (profile < PROFILE_COUNT) {
        active_profile = profile;
    }
}

void acl_init(void) {
    security_violations_count = 0;
    active_profile = PROFILE_GENERAL;

    // Perfil General: Solo lectura de archivos y sensores básicos
    profile_acl[PROFILE_GENERAL] = OP_FS_READ | OP_ARDUINO_READ;

    // Perfil Estudiante: Lectura y escritura en RAMFS, lectura de sensores
    profile_acl[PROFILE_STUDENT] = OP_FS_READ | OP_FS_WRITE | OP_ARDUINO_READ;

    // Perfil Frontend: Lectura/escritura FS y paquetes
    profile_acl[PROFILE_FRONTEND] = OP_FS_READ | OP_FS_WRITE | OP_SYSTEM_ADMIN;

    // Perfil Backend: Todos los permisos excepto hardware directo de actuadores por defecto
    profile_acl[PROFILE_BACKEND] = OP_FS_READ | OP_FS_WRITE | OP_SYSTEM_ADMIN;

    // Perfil Fullstack: Todos los permisos de software
    profile_acl[PROFILE_FULLSTACK] = OP_FS_READ | OP_FS_WRITE | OP_SYSTEM_ADMIN;

    // Perfil Diseñador: Solo lectura y escritura en almacenamiento de diseño
    profile_acl[PROFILE_DESIGNER] = OP_FS_READ | OP_FS_WRITE;

    // Perfil Hardware/IoT: Acceso total a sensores, actuadores y gestión de reglas
    profile_acl[PROFILE_IOT_HARDWARE] = OP_FS_READ | OP_FS_WRITE | OP_ARDUINO_READ | OP_ARDUINO_WRITE | OP_SYSTEM_ADMIN;

    // Perfil Data Scientist: Lectura FS y telemetría de sensores
    profile_acl[PROFILE_DATA_SCIENTIST] = OP_FS_READ | OP_FS_WRITE | OP_ARDUINO_READ;

    // Perfil Ciberseguridad: Acceso a inspección y logs
    profile_acl[PROFILE_CYBERSECURITY] = OP_FS_READ | OP_FS_WRITE | OP_SYSTEM_ADMIN;

    // Perfil Oficina: Documentación y archivos
    profile_acl[PROFILE_OFFICE_ADMIN] = OP_FS_READ | OP_FS_WRITE;

    // Perfil Gamer / Streamer: Acceso estándar a juegos y medios
    profile_acl[PROFILE_GAMER_STREAMER] = OP_FS_READ | OP_FS_WRITE;
}

bool acl_check_permission(enum nova_profile profile, enum sec_operation op) {
    if (profile >= PROFILE_COUNT) return false;
    return (profile_acl[profile] & op) == op;
}

uint32_t acl_get_violations_count(void) {
    return security_violations_count;
}

void acl_log_violation(enum nova_profile profile, enum sec_operation op, const char* context) {
    security_violations_count++;

    // Generar registro de auditoría en RAMFS (security.log)
    char log_buf[96];
    k_memset(log_buf, 0, sizeof(log_buf));
    size_t idx = 0;
    const char* prefix = "[SEC_VIOLATION] Perfil: ";
    for (size_t i = 0; prefix[i]; i++) log_buf[idx++] = prefix[i];

    const char* prof_name = profile_get_name(profile);
    for (size_t i = 0; prof_name[i]; i++) log_buf[idx++] = prof_name[i];

    const char* mid = " | Op rechazada: 0x";
    for (size_t i = 0; mid[i]; i++) log_buf[idx++] = mid[i];
    char hex[8];
    uitoa(op, hex, 16);
    for (size_t i = 0; hex[i]; i++) log_buf[idx++] = hex[i];

    if (context) {
        const char* ctx_tag = " (";
        for (size_t i = 0; ctx_tag[i]; i++) log_buf[idx++] = ctx_tag[i];
        for (size_t i = 0; context[i] && idx < 90; i++) log_buf[idx++] = context[i];
        log_buf[idx++] = ')';
    }
    log_buf[idx++] = '\n';
    log_buf[idx] = '\0';

    ramfs_create("security.log");
    ramfs_write("security.log", (const uint8_t*)log_buf, idx);
}

void acl_list_permissions(enum nova_profile profile) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Matriz de Permisos para el perfil: %s\n", profile_get_name(profile));
    kprintf("  - Lectura en Sistema de Archivos (RAMFS):   %s\n", 
            acl_check_permission(profile, OP_FS_READ) ? "[PERMITIDO]" : "[DENEGADO]");
    kprintf("  - Escritura/Creacion en Archivos:          %s\n", 
            acl_check_permission(profile, OP_FS_WRITE) ? "[PERMITIDO]" : "[DENEGADO]");
    kprintf("  - Lectura de Sensores Arduino (IoT):        %s\n", 
            acl_check_permission(profile, OP_ARDUINO_READ) ? "[PERMITIDO]" : "[DENEGADO]");
    kprintf("  - Control de Actuadores Arduino (LED/Relé): %s\n", 
            acl_check_permission(profile, OP_ARDUINO_WRITE) ? "[PERMITIDO]" : "[DENEGADO]");
    kprintf("  - Administracion (Instalar Apps, Reglas):   %s\n", 
            acl_check_permission(profile, OP_SYSTEM_ADMIN) ? "[PERMITIDO]" : "[DENEGADO]");
    kprintf("Total de violaciones de acceso registradas: %u\n", security_violations_count);
}
