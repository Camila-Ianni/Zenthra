#include "shell.h"
#include "vga.h"
#include "klib.h"
#include "kheap.h"
#include "keyboard.h"
#include "nova_iot.h"

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
        kprintf("  profile [0-3]    - Cambia el perfil activo del usuario\n");
        kprintf("  arduino ping     - Comprueba conectividad con la placa\n");
        kprintf("  arduino led on   - Enciende el LED de la placa\n");
        kprintf("  arduino led off  - Apaga el LED de la placa\n");
        kprintf("  arduino temp     - Lee la temperatura actual del sensor\n");
        kprintf("  arduino status   - Muestra estadisticas de paquetes y errores\n");
    } else if (k_strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (k_strcmp(cmd, "info") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kprintf("NOVA OS v0.1 (Experimental Academic Edition - Octubre 2026)\n");
        kprintf("Arquitectura: x86_32 Modo Protegido (Ring 0 / Ring 3)\n");
        kprintf("Syscalls: Int 0x80 habilitada con despacho protegido\n");
        kprintf("Perfil activo: %s\n", profile_names[current_profile]);
    } else if (k_strcmp(cmd, "mem") == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kprintf("Estado del Kernel Heap:\n");
        kprintf("  Memoria en uso: %u bytes\n", kheap_get_used());
        kprintf("  Memoria libre:  %u bytes\n", kheap_get_free());
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
