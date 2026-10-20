#include "shell.h"
#include "vga.h"
#include "klib.h"
#include "kheap.h"
#include "keyboard.h"
#include "nova_iot.h"
#include "ramfs.h"
#include "rules_engine.h"

#define LINE_MAX 80

static char line_buf[LINE_MAX];
static size_t line_len = 0;
static enum nova_profile current_profile = PROFILE_GENERAL;

static const char* profile_names[] = {
    "General",
    "Estudiante",
    "Desarrollador",
    "Hardware/IoT"
};

void shell_init(void) {
    line_len = 0;
    line_buf[0] = '\0';
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("nova [%s]# ", profile_names[current_profile]);
}

void shell_execute(const char* cmd) {
    if (k_strcmp(cmd, "help") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        kprintf("Comandos disponibles en NOVA OS:\n");
        kprintf("  help             - Muestra este menu de ayuda\n");
        kprintf("  clear            - Limpia la pantalla de la consola\n");
        kprintf("  info             - Informacion y version del sistema operativo\n");
        kprintf("  mem              - Muestra el estado y uso del Heap del Kernel\n");
        kprintf("  ls               - Lista los archivos almacenados en RAMFS\n");
        kprintf("  cat <archivo>    - Muestra el contenido de un archivo de texto\n");
        kprintf("  rules            - Lista las reglas de automatizacion activas\n");
        kprintf("  profile [0-3]    - Cambia el perfil activo del usuario\n");
        kprintf("  arduino ping     - Comprueba conectividad con la placa\n");
        kprintf("  arduino led on   - Enciende el LED de la placa\n");
        kprintf("  arduino led off  - Apaga el LED de la placa\n");
        kprintf("  arduino temp     - Lee la temperatura actual del sensor\n");
        kprintf("  arduino auto     - Evalua reglas de automatizacion segun sensor\n");
        kprintf("  arduino log      - Registra la lectura actual en RAMFS (telemetria.log)\n");
        kprintf("  arduino status   - Muestra estadisticas de paquetes y errores\n");
    } else if (k_strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (k_strcmp(cmd, "info") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kprintf("NOVA OS v0.1 (Experimental Academic Edition - Octubre 2026)\n");
        kprintf("Arquitectura: x86_32 Modo Protegido (Ring 0 / Ring 3)\n");
        kprintf("Syscalls: Int 0x80 | FS: RAMFS | Motor: Reglas IoT\n");
        kprintf("Perfil activo: %s\n", profile_names[current_profile]);
    } else if (k_strcmp(cmd, "mem") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kprintf("Estado del Kernel Heap:\n");
        kprintf("  Memoria en uso: %u bytes\n", kheap_get_used());
        kprintf("  Memoria libre:  %u bytes\n", kheap_get_free());
    } else if (k_strcmp(cmd, "ls") == 0) {
        ramfs_list();
    } else if (k_strcmp(cmd, "rules") == 0) {
        rules_engine_list();
    } else if (k_strncmp(cmd, "cat ", 4) == 0) {
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
    } else if (k_strncmp(cmd, "profile", 7) == 0) {
        if (cmd[7] == ' ' && cmd[8] >= '0' && cmd[8] <= '3') {
            current_profile = (enum nova_profile)(cmd[8] - '0');
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
            kprintf("Perfil cambiado con exito a: %s\n", profile_names[current_profile]);
        } else {
            vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            kprintf("Uso: profile <0:General, 1:Estudiante, 2:Desarrollador, 3:IoT>\n");
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
            kprintf("[*] Orden: Activar LED... ");
            if (nova_iot_set_led(true)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[LED ACTIVADO - OK]\n");
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[FALLO DE COMUNICACION]\n");
            }
        } else if (k_strcmp(cmd, "arduino led off") == 0) {
            kprintf("[*] Orden: Desactivar LED... ");
            if (nova_iot_set_led(false)) {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
                kprintf("[LED APAGADO - OK]\n");
            } else {
                vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
                kprintf("[FALLO DE COMUNICACION]\n");
            }
        } else if (k_strcmp(cmd, "arduino temp") == 0) {
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
