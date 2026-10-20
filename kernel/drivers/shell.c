#include "shell.h"
#include "vga.h"
#include "klib.h"
#include "kheap.h"
#include "keyboard.h"
#include "nova_iot.h"
#include "ramfs.h"
#include "rules_engine.h"
#include "pkg_manager.h"
#include "task.h"
#include "acl_security.h"

#define LINE_MAX 80

static char line_buf[LINE_MAX];
static size_t line_len = 0;
static enum nova_profile current_profile = PROFILE_GENERAL;

static const char* profile_names[PROFILE_COUNT] = {
    "General",
    "Estudiante",
    "Frontend",
    "Backend",
    "Fullstack",
    "Disenador",
    "Hardware-IoT",
    "DataScientist",
    "Ciberseguridad",
    "Oficina",
    "Gamer-Streamer"
};

static const char* profile_descriptions[PROFILE_COUNT] = {
    "Uso basico diario, exploracion y utilidades generales",
    "Ofimatica (Office), lector PDF academico y tarjetas de estudio",
    "Diseno UI con Figma, VS Code, entorno Node.js y navegador de desarrollo",
    "Contenedores Docker, cliente PostgreSQL, probador de APIs y Git",
    "Entorno integral con SDKs C/Rust, Node.js, Docker y terminal avanzada",
    "Herramientas de dibujo vectorial, retoque fotografico y tipografias",
    "Arduino IDE, analizador logico de puertos serie y actuadores",
    "Cuadernos Jupyter y bibliotecas graficas de analisis de datos",
    "Escaner de puertos de red, auditorias y analizador de paquetes",
    "Planillas de calculo con formulas y sistema de facturacion",
    "Lanzador optimizado para juegos y software de captura/streaming OBS"
};

const char* profile_get_name(enum nova_profile prof) {
    if (prof < PROFILE_COUNT) return profile_names[prof];
    return "Desconocido";
}

const char* profile_get_description(enum nova_profile prof) {
    if (prof < PROFILE_COUNT) return profile_descriptions[prof];
    return "";
}

void shell_init(void) {
    line_len = 0;
    line_buf[0] = '\0';
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("nova [%s]# ", profile_names[current_profile]);
}

static void show_profiles_list(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Perfiles de Usuario configurados en NOVA OS (0 a 10):\n");
    for (int i = 0; i < PROFILE_COUNT; i++) {
        const char* mark = (i == (int)current_profile) ? "[ACTIVO]" : "        ";
        kprintf("  %s %2d. %-15s - %s\n", mark, i, profile_names[i], profile_descriptions[i]);
    }
    kprintf("\nUsa 'profile <numero>' para cambiar y auto-aprovisionar las apps del perfil.\n");
}

void shell_execute(const char* cmd) {
    if (k_strcmp(cmd, "help") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        kprintf("Comandos disponibles en NOVA OS:\n");
        kprintf("  help                 - Muestra este menu de ayuda\n");
        kprintf("  clear                - Limpia la pantalla de la consola\n");
        kprintf("  info                 - Informacion y version del sistema operativo\n");
        kprintf("  perms                - Muestra los permisos de seguridad (ACL) del perfil\n");
        kprintf("  mem                  - Muestra el estado y uso del Heap del Kernel\n");
        kprintf("  ps                   - Lista los procesos y tareas activas del sistema\n");
        kprintf("  ls                   - Lista los archivos almacenados en RAMFS\n");
        kprintf("  cat <archivo>        - Muestra el contenido de un archivo de texto\n");
        kprintf("  profiles             - Lista los 11 perfiles de usuario disponibles\n");
        kprintf("  profile <0-10>       - Activa un perfil y auto-descarga sus herramientas\n");
        kprintf("  store search         - Muestra el catalogo completo de NOVA Store\n");
        kprintf("  store list           - Muestra las aplicaciones instaladas\n");
        kprintf("  store rec            - Recomienda aplicaciones segun tu perfil activo\n");
        kprintf("  store install <id>   - Instala una aplicacion del catalogo\n");
        kprintf("  store remove <id>    - Desinstala una aplicacion del sistema\n");
        kprintf("  rules                - Lista las reglas de automatizacion activas\n");
        kprintf("  arduino ping         - Comprueba conectividad con la placa\n");
        kprintf("  arduino led on       - Enciende el LED de la placa (Requiere permiso IoT)\n");
        kprintf("  arduino led off      - Apaga el LED de la placa (Requiere permiso IoT)\n");
        kprintf("  arduino temp         - Lee la temperatura actual del sensor\n");
        kprintf("  arduino auto         - Evalua reglas de automatizacion segun sensor\n");
        kprintf("  arduino log          - Registra la lectura actual en RAMFS (telemetria.log)\n");
        kprintf("  arduino status       - Muestra estadisticas de paquetes y errores\n");
    } else if (k_strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (k_strcmp(cmd, "info") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kprintf("NOVA OS v0.1 (Experimental Academic Edition - Octubre 2026)\n");
        kprintf("Arquitectura: x86_32 Modo Protegido (Ring 0 / Ring 3)\n");
        kprintf("Seguridad: Principio de Minimo Privilegio (ACL por perfil)\n");
        kprintf("Multitarea: Planificador Round-Robin / Tareas Cooperativas\n");
        kprintf("Perfil activo actual: %s (#%u)\n", profile_names[current_profile], (uint32_t)current_profile);
    } else if (k_strcmp(cmd, "perms") == 0) {
        acl_list_permissions(current_profile);
    } else if (k_strcmp(cmd, "mem") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kprintf("Estado del Kernel Heap:\n");
        kprintf("  Memoria en uso: %u bytes\n", kheap_get_used());
        kprintf("  Memoria libre:  %u bytes\n", kheap_get_free());
    } else if (k_strcmp(cmd, "ps") == 0) {
        task_list();
    } else if (k_strcmp(cmd, "ls") == 0) {
        if (!acl_check_permission(current_profile, OP_FS_READ)) {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            kprintf("[ACCESO DENEGADO] El perfil actual no tiene permisos de lectura en RAMFS.\n");
            acl_log_violation(current_profile, OP_FS_READ, "ls");
            return;
        }
        ramfs_list();
    } else if (k_strcmp(cmd, "profiles") == 0) {
        show_profiles_list();
    } else if (k_strncmp(cmd, "profile", 7) == 0) {
        if (cmd[7] == ' ') {
            int num = -1;
            if (cmd[8] >= '0' && cmd[8] <= '9') {
                if (cmd[9] >= '0' && cmd[9] <= '9') {
                    num = (cmd[8] - '0') * 10 + (cmd[9] - '0');
                } else {
                    num = cmd[8] - '0';
                }
            }

            if (num >= 0 && num < PROFILE_COUNT) {
                current_profile = (enum nova_profile)num;
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("\n[PERFIL CAMBIADO] Ahora estas en el perfil: %s\n", profile_names[current_profile]);
                kprintf("Descripcion: %s\n\n", profile_descriptions[current_profile]);

                vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
                kprintf("[*] Iniciando aprovisionamiento automatico de aplicaciones...\n");
                int installed = pkg_provision_profile(current_profile);
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[*] Aprovisionamiento completado (%u nuevas aplicaciones preparadas en RAMFS).\n\n", installed);
            } else {
                show_profiles_list();
            }
        } else {
            show_profiles_list();
        }
    } else if (k_strncmp(cmd, "store ", 6) == 0) {
        const char* sub = cmd + 6;
        if (k_strcmp(sub, "search") == 0) {
            pkg_list_catalog();
        } else if (k_strcmp(sub, "list") == 0) {
            pkg_list_installed();
        } else if (k_strcmp(sub, "rec") == 0) {
            pkg_recommend(current_profile);
        } else if (k_strncmp(sub, "install ", 8) == 0) {
            if (!acl_check_permission(current_profile, OP_SYSTEM_ADMIN)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ACCESO DENEGADO] Se requieren permisos de administracion para instalar paquetes.\n");
                acl_log_violation(current_profile, OP_SYSTEM_ADMIN, sub);
                return;
            }
            const char* id = sub + 8;
            int r = pkg_install(id);
            if (r == 0) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[OK] Aplicacion '%s' instalada correctamente.\n", id);
            } else if (r == 1) {
                vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
                kprintf("[INFO] La aplicacion '%s' ya se encuentra instalada.\n", id);
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ERROR] No se encontro el paquete '%s' en el catalogo.\n", id);
            }
        } else if (k_strncmp(sub, "remove ", 7) == 0) {
            if (!acl_check_permission(current_profile, OP_SYSTEM_ADMIN)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ACCESO DENEGADO] Se requieren permisos de administracion para desinstalar paquetes.\n");
                acl_log_violation(current_profile, OP_SYSTEM_ADMIN, sub);
                return;
            }
            const char* id = sub + 7;
            int r = pkg_uninstall(id);
            if (r == 0) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[OK] Aplicacion '%s' desinstalada del sistema.\n", id);
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ERROR] La aplicacion '%s' no esta instalada.\n", id);
            }
        } else {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            kprintf("Uso: store <search | list | rec | install <id> | remove <id>>\n");
        }
    } else if (k_strcmp(cmd, "rules") == 0) {
        rules_engine_list();
    } else if (k_strncmp(cmd, "cat ", 4) == 0) {
        if (!acl_check_permission(current_profile, OP_FS_READ)) {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            kprintf("[ACCESO DENEGADO] El perfil actual no tiene permisos para leer archivos.\n");
            acl_log_violation(current_profile, OP_FS_READ, cmd);
            return;
        }
        const char* filename = cmd + 4;
        uint8_t buffer[512];
        int bytes = ramfs_read(filename, buffer, sizeof(buffer) - 1);
        if (bytes >= 0) {
            buffer[bytes] = '\0';
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            kprintf("%s\n", (const char*)buffer);
        } else {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            kprintf("Archivo no encontrado: '%s'\n", filename);
        }
    } else if (k_strncmp(cmd, "arduino", 7) == 0) {
        if (k_strcmp(cmd, "arduino ping") == 0) {
            kprintf("[*] Enviando PING a Arduino por puerto serie... ");
            if (nova_iot_ping()) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[PONG RECIBIDO - CONECTADO]\n");
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[TIMEOUT / NO RESPONDE]\n");
            }
        } else if (k_strcmp(cmd, "arduino led on") == 0) {
            if (!acl_check_permission(current_profile, OP_ARDUINO_WRITE)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ACCESO DENEGADO] El perfil '%s' no tiene autorizacion para activar actuadores fisicos.\n", profile_names[current_profile]);
                acl_log_violation(current_profile, OP_ARDUINO_WRITE, "arduino led on");
                return;
            }
            kprintf("[*] Orden: Activar LED... ");
            if (nova_iot_set_led(true)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[LED ACTIVADO - OK]\n");
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[FALLO DE COMUNICACION]\n");
            }
        } else if (k_strcmp(cmd, "arduino led off") == 0) {
            if (!acl_check_permission(current_profile, OP_ARDUINO_WRITE)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ACCESO DENEGADO] El perfil '%s' no tiene autorizacion para controlar actuadores fisicos.\n", profile_names[current_profile]);
                acl_log_violation(current_profile, OP_ARDUINO_WRITE, "arduino led off");
                return;
            }
            kprintf("[*] Orden: Desactivar LED... ");
            if (nova_iot_set_led(false)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[LED APAGADO - OK]\n");
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[FALLO DE COMUNICACION]\n");
            }
        } else if (k_strcmp(cmd, "arduino temp") == 0) {
            if (!acl_check_permission(current_profile, OP_ARDUINO_READ)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ACCESO DENEGADO] El perfil '%s' no tiene autorizacion para leer sensores.\n", profile_names[current_profile]);
                acl_log_violation(current_profile, OP_ARDUINO_READ, "arduino temp");
                return;
            }
            uint16_t temp = 0;
            kprintf("[*] Solicitando lectura de temperatura... ");
            if (nova_iot_get_temp(&temp)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
                kprintf("[LECTURA: %u.%u C]\n", temp / 10, temp % 10);
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ERROR AL LEER SENSOR]\n");
            }
        } else if (k_strcmp(cmd, "arduino auto") == 0) {
            uint16_t temp = 0;
            kprintf("[*] Leyendo sensor y evaluando reglas de automatizacion... ");
            if (nova_iot_get_temp(&temp)) {
                rules_engine_evaluate(temp);
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[OK: %u.%u C procesados]\n", temp / 10, temp % 10);
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ERROR AL LEER SENSOR]\n");
            }
        } else if (k_strcmp(cmd, "arduino log") == 0) {
            if (!acl_check_permission(current_profile, OP_FS_WRITE)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[ACCESO DENEGADO] Permiso de escritura en almacenamiento denegado.\n");
                acl_log_violation(current_profile, OP_FS_WRITE, "arduino log");
                return;
            }
            uint16_t temp = 0;
            if (nova_iot_get_temp(&temp)) {
                char log_entry[64];
                k_memset(log_entry, 0, sizeof(log_entry));
                char num1[16], num2[16];
                uitoa(temp / 10, num1, 10);
                uitoa(temp % 10, num2, 10);
                
                size_t idx = 0;
                const char* prefix = "Sensor Temp: ";
                for (size_t i = 0; prefix[i]; i++) log_entry[idx++] = prefix[i];
                for (size_t i = 0; num1[i]; i++) log_entry[idx++] = num1[i];
                log_entry[idx++] = '.';
                for (size_t i = 0; num2[i]; i++) log_entry[idx++] = num2[i];
                log_entry[idx++] = ' ';
                log_entry[idx++] = 'C';
                log_entry[idx++] = '\n';
                log_entry[idx] = '\0';

                ramfs_create("telemetria.log");
                ramfs_write("telemetria.log", (const uint8_t*)log_entry, idx);

                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[OK] Telemetria guardada en 'telemetria.log' (%u.%u C)\n", temp / 10, temp % 10);
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[FALLO] No se pudo leer el sensor para registrar datos.\n");
            }
        } else if (k_strcmp(cmd, "arduino status") == 0) {
            const struct arduino_state* st = nova_iot_get_state();
            vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            kprintf("Estado Arduino: %s\n", st->connected ? "CONECTADO" : "DESCONECTADO");
            kprintf("Estado LED:     %s\n", st->led_state ? "ENCENDIDO" : "APAGADO");
            kprintf("Paquetes TX:    %u\n", st->packets_sent);
            kprintf("Paquetes RX:    %u\n", st->packets_received);
            kprintf("Errores CKSUM:  %u\n", st->errors_count);
        } else {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            kprintf("Subcomando no reconocido. Usa 'help' para ver la lista.\n");
        }
    } else if (line_len > 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        kprintf("Comando desconocido: '%s'. Escribe 'help'.\n", cmd);
    }
}

void shell_step(void) {
    char c = keyboard_getchar_nonblocking();
    if (!c) return;

    if (c == '\n') {
        vga_putc('\n');
        line_buf[line_len] = '\0';
        shell_execute(line_buf);
        line_len = 0;
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        kprintf("nova [%s]# ", profile_names[current_profile]);
    } else if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            vga_putc('\b');
        }
    } else if (line_len < LINE_MAX - 1) {
        if (c >= ' ' && c <= '~') {
            line_buf[line_len++] = c;
            vga_putc(c);
        }
    }
}
