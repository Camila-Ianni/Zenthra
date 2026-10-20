#include "ktypes.h"
#include "vga.h"
#include "klib.h"
#include "serial.h"
#include "gdt.h"
#include "idt.h"
#include "pit.h"
#include "syscall.h"
#include "kheap.h"
#include "ramfs.h"
#include "rules_engine.h"
#include "pkg_manager.h"
#include "keyboard.h"
#include "nova_iot.h"
#include "shell.h"

/* Estructura básica de información Multiboot pasada por el bootloader */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
};

#define KHEAP_START 0x00400000
#define KHEAP_SIZE  (2 * 1024 * 1024) // 2 MiB de Heap inicial

void kernel_main(uint32_t magic, struct multiboot_info* mbi) {
    // 1. Inicializar pantalla VGA en modo texto
    vga_init();
    
    // 2. Banner de inicio de NOVA OS
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("================================================================================\n");
    kprintf("                           NOVA OPERATING SYSTEM                                \n");
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

    // 6. Inicializar despachador seguro de llamadas al sistema (Syscalls Int 0x80)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Configurando Syscalls (Int 0x80)... ");
    syscall_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 7. Inicializar gestor de memoria dinámica (Heap 2MB)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Kernel Heap en 0x%x (2 MiB)... ", KHEAP_START);
    kheap_init((void*)KHEAP_START, KHEAP_SIZE);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 8. Inicializar sistema de archivos en memoria (RAMFS)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Montando sistema de archivos RAMFS... ");
    ramfs_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 9. Inicializar catálogo y tienda de aplicaciones (NOVA Store)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando catalogo NOVA Store... ");
    pkg_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 10. Inicializar motor de automatización y reglas IoT
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Cargando motor de reglas IoT... ");
    rules_engine_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 11. Inicializar driver serial (COM1) para log de depuración
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando puerto serie COM1 (UART 16550A)... ");
    if (serial_init() == 0) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("[OK]\n");
        serial_write_string("\r\n[NOVA OS SERIAL LOG] Kernel iniciado con GDT, IDT, PIT, Syscalls, Heap, RAMFS, Store y Rules Engine.\r\n");
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        kprintf("[FALLO O SIN HARDWARE]\n");
    }

    // 12. Verificar firma mágica de arranque Multiboot (0x2BADB002)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Verificando arranque Multiboot... ");
    if (magic == 0x2BADB002) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("[OK] (Magic: 0x%x)\n", magic);
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        kprintf("[ADVERTENCIA] Bootloader no estandar (0x%x)\n", magic);
    }

    // 13. Diagnóstico de memoria si está disponible en Multiboot
    if (mbi && (mbi->flags & 1)) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        kprintf("[*] Memoria convencional baja: %u KiB | Alta: %u KiB (~%u MiB)\n", 
                mbi->mem_lower, mbi->mem_upper, mbi->mem_upper / 1024);
    }

    // 14. Inicializar periféricos y subsistemas de usuario
    keyboard_init();
    nova_iot_init();

    // Habilitar interrupciones de hardware en la CPU
    __asm__ volatile ("sti");

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("\n[>] Sistema operativo preparado. Iniciando NOVA Shell.\n\n");

    // 15. Iniciar shell interactivo
    shell_init();

    // 16. Bucle principal del sistema
    while (1) {
        shell_step();
    }
}
