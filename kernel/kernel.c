#include "ktypes.h"
#include "vga.h"
#include "klib.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "pit.h"
#include "syscall.h"
#include "kheap.h"
#include "task.h"
#include "ramfs.h"
#include "rules_engine.h"
#include "pkg_manager.h"
#include "acl_security.h"
#include "keyboard.h"
#include "nova_iot.h"
#include "shell.h"
#include "multiboot.h"
#include "power.h"

#define KHEAP_START 0x00400000
#define KHEAP_SIZE  (2 * 1024 * 1024) // 2 MiB de Heap inicial

/* Tarea de fondo de diagnóstico continuo de sensores IoT */
static void background_sensor_daemon(void) {
    while (1) {
        uint16_t temp = 0;
        if (nova_iot_get_temp(&temp)) {
            rules_engine_evaluate(temp);
        }
        task_yield();
    }
}

void kernel_main(uint32_t magic, uintptr_t mbi_addr) {
    // 1. Inicializar pantalla VGA en modo texto
    vga_init();
    
    // 2. Banner de inicio de Zenthra OS
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("================================================================================\n");
    kprintf("                         ZENTHRA OPERATING SYSTEM                               \n");
    kprintf("                 Propuesta Experimental de Grado - 2026                         \n");
    kprintf("================================================================================\n\n");

    // 3. Inicializar GDT (Global Descriptor Table)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Cargando GDT (Kernel & User Segments)... ");
    gdt_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 4. Inicializar IDT (Interrupt Descriptor Table) y PIC 8259
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando IDT y remapeo PIC 8259... ");
    idt_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 5. Inicializar temporizador PIT (100 Hz / IRQ0)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Configurando Timer PIT (100 Hz / IRQ0)... ");
    pit_init(100);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 6. Inicializar subsistema de seguridad y permisos ACL
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Subsistema de Seguridad (ACL)... ");
    acl_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 7. Inicializar despachador seguro de llamadas al sistema (Syscalls Int 0x80)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Configurando Syscalls (Int 0x80)... ");
    syscall_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 8. Inicializar gestor de memoria dinámica (Heap 2MB)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Kernel Heap en 0x%x (2 MiB)... ", KHEAP_START);
    kheap_init((void*)KHEAP_START, KHEAP_SIZE);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 9. Inicializar planificador de tareas (Scheduler Round-Robin)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Planificador Multitarea (Scheduler)... ");
    scheduler_init();
    task_create("iot_daemon", background_sensor_daemon);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 10. Inicializar sistema de archivos en memoria (RAMFS)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Montando sistema de archivos RAMFS... ");
    ramfs_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 11. Inicializar catálogo y tienda de aplicaciones (Zenthra Store)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando catalogo Zenthra Store (11 Perfiles)... ");
    pkg_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 12. Inicializar motor de automatización y reglas IoT
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Cargando motor de reglas IoT... ");
    rules_engine_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 13. Inicializar driver serial (COM1) para log de depuración
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando puerto serie COM1 (UART 16550A)... ");
    if (serial_init() == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("[OK]\n");
        serial_write_string("\r\n[ZENTHRA OS SERIAL LOG] Kernel iniciado con Seguridad ACL, Multitarea, Heap, Syscalls, Store y 11 Perfiles.\r\n");
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        kprintf("[FALLO O SIN HARDWARE]\n");
    }

    // 14. Verificar y parsear protocolo Multiboot (Soporte seguro Multiboot 1 y 2)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Verificando protocolo Multiboot... ");
    struct multiboot_parsed_info mb_info;
    bool mb_ok = multiboot_parse(magic, mbi_addr, &mb_info);
    if (mb_ok) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("[OK] (Multiboot %u, Magic: 0x%x)\n", mb_info.version, magic);
        if (mb_info.version == 2) {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            kprintf("    Etiquetas parseadas de forma segura: %u tags.\n", mb_info.tags_count);
        }
        if (mb_info.mem_upper_kib > 0) {
            vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            kprintf("[*] Memoria detectada: Baja %u KiB | Alta %u KiB (~%u MiB)\n", 
                    mb_info.mem_lower_kib, mb_info.mem_upper_kib, mb_info.mem_upper_kib / 1024);
        }
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        kprintf("[ADVERTENCIA] Firma no reconocida (Magic: 0x%x)\n", magic);
    }

    // 16. Inicializar periféricos y subsistemas de usuario
    keyboard_init();
    nova_iot_init();

    // Habilitar interrupciones de hardware en la CPU
    __asm__ volatile ("sti");

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("\n[>] Sistema operativo preparado. Iniciando Zenthra Shell.\n\n");

    // 17. Iniciar shell interactivo
    shell_init();

    // 18. Bucle principal del sistema con multiplexación de tareas
    while (1) {
        shell_step();
        task_yield();
    }
}
