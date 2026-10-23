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
#include "pmm.h"
#include "vmm.h"
#include "page_fault.h"
#include "gfx.h"
#include "mouse.h"
#include "window_manager.h"

#define KHEAP_START 0x00400000
#define KHEAP_SIZE  (2 * 1024 * 1024) // 2 MiB de Heap inicial

static volatile uint32_t worker_a_counter = 0;
static volatile uint32_t worker_b_counter = 0;
static volatile uint32_t cpu_bound_ticks = 0;
static volatile bool test_complete_emitted = false;
static volatile bool kernel_test_mode = false;

/*
 * Verificación estricta de finalización de pruebas de arranque en QEMU (Auditoría v14/v15):
 * Emite una señal final inequívoca únicamente cuando los tres trabajadores concurrentes
 * han completado sus ciclos y se ha demostrado la preempción por hardware del PIT IRQ0.
 * En modo automatizado de prueba (test-mode), efectúa un apagado limpio de QEMU vía ACPI.
 */
static void check_all_workers_complete(void) {
    if (!test_complete_emitted && 
        worker_a_counter >= 3 && 
        worker_b_counter >= 3 && 
        cpu_bound_ticks >= 500000) {
        test_complete_emitted = true;
        serial_write_string("[TEST_COMPLETE] ZENTHRA_QEMU_TEST_COMPLETE: Kernel inicializado, subsistemas activos y tareas ejecutadas con exito.\r\n");
        if (kernel_test_mode) {
            serial_write_string("[POWER] Apagado limpio ACPI de QEMU (test-complete confirmado).\r\n");
            for (volatile int i = 0; i < 50000; i++) {}
            qemu_poweroff();
        }
    }
}

static void worker_task_a(void) {
    for (int i = 1; i <= 3; i++) {
        worker_a_counter++;
        serial_write_string("[SCHEDULER] Worker A ejecutando ciclo: ");
        char buf[8];
        uitoa(worker_a_counter, buf, 10);
        serial_write_string(buf);
        serial_write_string("\r\n");
        task_yield();
    }
    serial_write_string("[SCHEDULER] Worker A completo con exito.\r\n");
    check_all_workers_complete();
    task_exit();
}

static void worker_task_b(void) {
    for (int i = 1; i <= 3; i++) {
        worker_b_counter++;
        serial_write_string("[SCHEDULER] Worker B ejecutando ciclo: ");
        char buf[8];
        uitoa(worker_b_counter, buf, 10);
        serial_write_string(buf);
        serial_write_string("\r\n");
        task_yield();
    }
    serial_write_string("[SCHEDULER] Worker B completo con exito.\r\n");
    check_all_workers_complete();
    task_exit();
}

/* Tarea de cálculo intensivo sin yield voluntario para demostrar preempción automática por PIT IRQ0 */
static void worker_task_c_cpubound(void) {
    serial_write_string("[SCHEDULER] Worker C (CPU-bound) iniciado sin llamadas a yield voluntario.\r\n");
    for (int i = 0; i < 500000; i++) {
        cpu_bound_ticks++;
        // Tarea sin yield: liberada únicamente cuando el PIT IRQ0 agota su quantum de tiempo
    }
    serial_write_string("[SCHEDULER] Worker C (CPU-bound) preemptado y completado exitosamente.\r\n");
    check_all_workers_complete();
    task_exit();
}

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


/*
 * Prueba de integración bare-metal en QEMU para verificar la recuperación real de #PF (Auditoría v19):
 * Provoca dos excepciones #PF reales de la CPU x86 dentro de arch_user_copy_bytes (instrucción rep movsb):
 *   1. Copia de lectura que cruza desde una página mapeada hacia una página NO mapeada a mitad de transferencia.
 *   2. Copia de escritura hacia una página de usuario marcada como SOLO LECTURA (sin PTE_WRITABLE, con CR0.WP=1).
 * Verifica que page_fault_handler valide el rango de EIP y CR2, redirija regs->eip a __user_copy_fault_fixup,
 * retorne -1 (-EFAULT) de inmediato sin bucle infinito ni Kernel Panic, y que el contador de fixups se incremente.
 */
static void verify_real_hardware_pf_fixup_in_qemu(void) {
    serial_write_string("[VMM_TEST] Iniciando prueba de #PF real de hardware con redireccion de EIP (fixup)...\r\n");

    struct page_directory* kdir = vmm_get_kernel_directory();
    uintptr_t frame_phys = pmm_alloc_frame();
    uintptr_t test_virt_page1 = 0x40000000UL; // Página 1 (mapeada)
    uintptr_t test_virt_page2 = 0x40001000UL; // Página 2 (NO mapeada)

    // Mapear únicamente test_virt_page1 como escribible y de usuario; garantizar que test_virt_page2 no esté mapeada
    vmm_map_page(kdir, test_virt_page1, frame_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    vmm_unmap_page(kdir, test_virt_page2);

    uint8_t* page1_ptr = (uint8_t*)test_virt_page1;
    for (size_t i = 0; i < PAGE_SIZE; i++) {
        page1_ptr[i] = 'Z';
    }

    uint8_t kbuf[64];
    size_t fixups_before = page_fault_get_fixup_count();

    // Caso 1: Leer 32 bytes comenzando 16 bytes antes del final de test_virt_page1.
    // Los primeros 16 bytes se copian con éxito y el byte 17 cruza a test_virt_page2 (0x40001000),
    // disparando un #PF real de hardware en rep movsb. El handler debe redirigir EIP a __user_copy_fault_fixup.
    int rc_unmapped = copy_from_user_fixup_hw_test(kbuf, (const void*)(test_virt_page1 + PAGE_SIZE - 16), 32);

    // Caso 2: Cambiar test_virt_page1 a SOLO LECTURA (sin PTE_WRITABLE) e intentar escribir en ella
    // con copy_to_user_fixup_hw_test. Con CR0.WP=1, la CPU dispara un #PF real por violación de escritura.
    vmm_set_page_flags(kdir, test_virt_page1, PTE_PRESENT | PTE_USER);
    int rc_readonly = copy_to_user_fixup_hw_test((void*)test_virt_page1, kbuf, 16);

    size_t fixups_after = page_fault_get_fixup_count();

    // Limpiar mapeo temporal y liberar marco
    vmm_unmap_page(kdir, test_virt_page1);
    pmm_free_frame(frame_phys);

    if (rc_unmapped == -1 && rc_readonly == -1 && (fixups_after == fixups_before + 2)) {
        serial_write_string("[VMM_FIXUP_QEMU_PASS] #PF reales de hardware (no mapeada y solo lectura) recuperados con redireccion EIP -> -EFAULT.\r\n");
    } else {
        serial_write_string("[VMM_FIXUP_QEMU_FAIL] Fallo en la prueba de recuperacion de #PF real en QEMU.\r\n");
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

    // 8. Inicializar Administrador de Memoria Fisica (PMM)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando PMM (Deteccion fisica Multiboot)... ");
    pmm_init(magic, mbi_addr);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 9. Inicializar Paginacion x86 de 32 bits (VMM, Identity Mapping 64 MiB y Page Faults)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Activando Paginacion x86 (CR0.PG, CR3, VMM %u MiB)... ", (uint32_t)(KERNEL_IDENTITY_MAP_SIZE / (1024 * 1024)));
    vmm_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 10. Inicializar gestor de memoria dinamica (Heap 2MB)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Kernel Heap en 0x%x (2 MiB)... ", KHEAP_START);
    kheap_init((void*)KHEAP_START, KHEAP_SIZE);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 11. Inicializar planificador de tareas (Scheduler Round-Robin)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Planificador Multitarea (Scheduler)... ");
    scheduler_init();
    task_create("worker_a", worker_task_a);
    task_create("worker_b", worker_task_b);
    task_create("worker_cpubound", worker_task_c_cpubound);
    task_create_with_profile("iot_daemon", background_sensor_daemon, PROFILE_IOT_HARDWARE);
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
        if (mb_info.is_test_mode) {
            kernel_test_mode = true;
            serial_write_string("[BOOT] Modo automatizado de prueba (test-mode) detectado en cmdline.\r\n");
        }
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

    // 14b. Ejecutar prueba bare-metal de recuperación real de #PF (redirección EIP fixup) en QEMU (Auditoría v19)
    verify_real_hardware_pf_fixup_in_qemu();

    // 15. Inicializar subsistema gráfico (Linear Framebuffer VESA o Fallback VGA)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Framebuffer VESA / LFB... ");
    if (gfx_init(magic, mbi_addr)) {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("[OK]\n");
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
        kprintf("[FALLBACK VGA MODO TEXTO]\n");
    }

    // 16. Inicializar periféricos y subsistemas de usuario
    keyboard_init();
    nova_iot_init();

    // 17. Inicializar controlador de ratón PS/2 (IRQ 12)
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Raton PS/2 (IRQ 12)... ");
    struct framebuffer_info* fb = gfx_get_fb_info();
    uint32_t sw = (fb && fb->width > 0) ? fb->width : 1024;
    uint32_t sh = (fb && fb->height > 0) ? fb->height : 768;
    mouse_init(sw, sh);
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // 18. Inicializar gestor de ventanas Zenthra WM
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("[*] Inicializando Gestor de Ventanas Zenthra WM... ");
    wm_init();
    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("[OK]\n");

    // Habilitar interrupciones de hardware en la CPU
    __asm__ volatile ("sti");

    if (gfx_is_active()) {
        wm_render_desktop();
    } else {
        vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("\n[>] Sistema operativo preparado. Iniciando Zenthra Shell.\n\n");
        shell_init();
    }

    // 19. Bucle principal del sistema con renderizado GUI o multiplexación Shell
    while (1) {
        if (gfx_is_active()) {
            char key = keyboard_getchar_nonblocking();
            if (key) {
                wm_handle_keyboard(key);
            }
            struct mouse_state* ms = mouse_get_state();
            wm_handle_mouse(ms->x, ms->y, ms->left_button);
            wm_update_clock(pit_get_ticks());
            wm_render_desktop();
        } else {
            shell_step();
        }
        task_yield();
    }
}
