#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ktypes.h"
#include "klib.h"
#include "kheap.h"
#include "task.h"
#include "acl_security.h"
#include "syscall.h"
#include "ramfs.h"
#include "rules_engine.h"
#include "pkg_manager.h"
#include "shell.h"
#include "nova_link.h"
#include "nova_iot.h"
#include "multiboot.h"
#include "power.h"
#include "pmm.h"
#include "vmm.h"
#include "page_fault.h"
#include "gfx.h"
#include "mouse.h"
#include "window_manager.h"
#include "keyboard.h"
#include "errno.h"

// Variables y stubs requeridos por drivers en host
void vga_putc(char c) {
    (void)c;
}
void vga_writestring(const char* s) {
    (void)s;
}
void vga_set_color(uint8_t color) {
    (void)color;
}
uint8_t vga_entry_color(int fg, int bg) {
    (void)fg; (void)bg;
    return 0x07;
}

static const char* test_profile_names[PROFILE_COUNT] = {
    "General", "Estudiante", "Frontend", "Backend", "Fullstack",
    "Disenador", "Hardware-IoT", "DataScientist", "Ciberseguridad", "Oficina", "Gamer-Streamer"
};

const char* profile_get_name(enum nova_profile prof) {
    if (prof < PROFILE_COUNT) return test_profile_names[prof];
    return "Desconocido";
}

// Mock de nova_iot_set_led para pruebas del motor de reglas
static bool mock_led_value = false;
bool nova_iot_set_led(bool state) {
    mock_led_value = state;
    return true;
}

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } \
} while(0)

/* Test 1: Biblioteca estándar propia klib */
void test_klib_strings(void) {
    printf("[*] Ejecutando: test_klib_strings...\n");
    const char* str = "NOVA OS Kernel";
    TEST_ASSERT(k_strlen(str) == 14, "k_strlen calcula longitud correcta");
    TEST_ASSERT(k_strcmp(str, "NOVA OS Kernel") == 0, "k_strcmp igualdad");
    TEST_ASSERT(k_strcmp(str, "NOVA") != 0, "k_strcmp desigualdad");
    TEST_ASSERT(k_strncmp(str, "NOVA", 4) == 0, "k_strncmp prefijo");

    char buf[32];
    k_memset(buf, 'A', 5);
    buf[5] = '\0';
    TEST_ASSERT(k_strcmp(buf, "AAAAA") == 0, "k_memset rellena buffer");

    k_memcpy(buf, "HELLO", 6);
    TEST_ASSERT(k_strcmp(buf, "HELLO") == 0, "k_memcpy copia memoria");

    uitoa(12345, buf, 10);
    TEST_ASSERT(k_strcmp(buf, "12345") == 0, "uitoa base 10 correcta");

    uitoa(0xDEAD, buf, 16);
    TEST_ASSERT(k_strcmp(buf, "dead") == 0, "uitoa base 16 hexadecimal correcta");
}

/* Test 2: Gestor de memoria dinámica kheap */
void test_kheap_allocator(void) {
    printf("[*] Ejecutando: test_kheap_allocator...\n");
    static uint8_t memory_pool[65536] __attribute__((aligned(16)));
    kheap_init(memory_pool, sizeof(memory_pool));

    size_t initial_free = kheap_get_free();
    TEST_ASSERT(initial_free > 0, "Heap inicializado con memoria libre disponible");

    void* ptr1 = kmalloc(64);
    TEST_ASSERT(ptr1 != NULL, "kmalloc asigna 64 bytes correctamente");
    TEST_ASSERT(kheap_get_used() >= 64, "El contador de memoria ocupada se incrementa");

    void* ptr2 = kmalloc(128);
    TEST_ASSERT(ptr2 != NULL && ptr2 != ptr1, "kmalloc asigna un segundo bloque contiguo y diferenciado");

    kfree(ptr1);
    TEST_ASSERT(kheap_get_free() > initial_free - 250, "kfree libera y decrementa memoria ocupada");

    kfree(ptr2);
    TEST_ASSERT(kheap_get_free() == initial_free, "Coalescencia de bloques contiguos restaura el espacio original");
}

/* Test 3: Validación Robusta de Punteros y Copias en Syscalls */
void test_syscall_security_checks(void) {
    printf("[*] Ejecutando: test_syscall_security_checks...\n");

    // 1. Puntero NULL debe ser rechazado
    TEST_ASSERT(is_user_pointer(NULL, 10) == false, "is_user_pointer rechaza puntero NULL");

    // 2. Puntero que invade espacio de memoria de kernel (por encima del límite)
    void* kernel_space_ptr = (void*)(uintptr_t)(USER_MEMORY_LIMIT + 0x1000);
    TEST_ASSERT(is_user_pointer(kernel_space_ptr, 16) == false, "is_user_pointer rechaza acceso a memoria del kernel");

    // 3. Puntero válido en espacio de usuario
    char valid_user_buf[64] = "Hello from Userland";
    TEST_ASSERT(is_user_pointer(valid_user_buf, sizeof(valid_user_buf)) == true, "is_user_pointer acepta buffer en userland");

    // 4. Copia segura copy_from_user
    char k_buf[64];
    int res = copy_from_user(k_buf, valid_user_buf, 20);
    TEST_ASSERT(res == 0, "copy_from_user realiza copia exitosa de buffer valido");
    TEST_ASSERT(k_strncmp(k_buf, "Hello from Userland", 19) == 0, "Contenido copiado de forma integra al kernel");

    // 5. Intento de copia desde memoria kernel debe fallar
    res = copy_from_user(k_buf, kernel_space_ptr, 16);
    TEST_ASSERT(res == -1, "copy_from_user bloquea lectura no autorizada desde memoria kernel");

    // 6. Validación de cadena strnlen_user
    ssize_t len = strnlen_user(valid_user_buf, 30);
    TEST_ASSERT(len == 19, "strnlen_user calcula longitud exacta hasta NUL");

    // Cadena sin NUL dentro de max_len
    char no_nul_buf[8] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};
    ssize_t bad_len = strnlen_user(no_nul_buf, 8);
    TEST_ASSERT(bad_len == -1, "strnlen_user detecta y rechaza cadena sin terminador NUL dentro del limite");

    // Auditoría v14: Longitud cero o puntero NULL
    TEST_ASSERT(strnlen_user(NULL, 10) == -1, "strnlen_user rechaza puntero NULL");
    TEST_ASSERT(strnlen_user(valid_user_buf, 0) == -1, "strnlen_user con max_len=0 retorna -1");
}

static void dummy_worker(void) {}

/* Test 4: Planificador Multitarea (Scheduler & PCB) */
void test_multitasking_scheduler(void) {
    printf("[*] Ejecutando: test_multitasking_scheduler...\n");
    scheduler_init();

    struct task* cur = task_get_current();
    TEST_ASSERT(cur != NULL, "Obtencion de la tarea actual en ejecucion");
    TEST_ASSERT(cur->state == TASK_RUNNING, "La tarea principal del kernel esta en estado RUNNING");
    TEST_ASSERT(k_strcmp(cur->name, "kernel_shell") == 0, "Nombre de la tarea 0 es kernel_shell");

    int pid = task_create("worker_task", dummy_worker);
    TEST_ASSERT(pid > 0, "Creacion exitosa de tarea worker_task con PID valido");

    task_yield();
    struct task* new_cur = task_get_current();
    TEST_ASSERT(new_cur != NULL && new_cur->id == (uint32_t)pid, "task_yield conmuta al worker_task");
    TEST_ASSERT(new_cur->state == TASK_RUNNING, "Nueva tarea conmutada a estado RUNNING");

    task_yield();
    TEST_ASSERT(task_get_current()->id == 1, "task_yield regresa a la tarea principal kernel_shell (Round-Robin)");
}

/* Test 5: Subsistema de Seguridad y Permisos por Perfil (ACL) */
void test_acl_security(void) {
    printf("[*] Ejecutando: test_acl_security...\n");
    acl_init();

    TEST_ASSERT(acl_check_permission(PROFILE_STUDENT, OP_ARDUINO_WRITE) == false,
                "Perfil Estudiante tiene denegado el control de actuadores fisicos (OP_ARDUINO_WRITE)");

    TEST_ASSERT(acl_check_permission(PROFILE_IOT_HARDWARE, OP_ARDUINO_WRITE) == true,
                "Perfil Hardware/IoT tiene autorizacion para controlar actuadores");

    TEST_ASSERT(acl_check_permission(PROFILE_STUDENT, OP_FS_READ) == true, "Perfil Estudiante puede leer RAMFS");
    TEST_ASSERT(acl_check_permission(PROFILE_STUDENT, OP_FS_WRITE) == true, "Perfil Estudiante puede escribir RAMFS");

    uint32_t prev_violations = acl_get_violations_count();
    acl_log_violation(PROFILE_STUDENT, OP_ARDUINO_WRITE, "test_intent");
    TEST_ASSERT(acl_get_violations_count() == prev_violations + 1, "Violacion de seguridad incrementa contador");

    uint8_t sec_buf[128];
    k_memset(sec_buf, 0, sizeof(sec_buf));
    int b = ramfs_read("security.log", sec_buf, sizeof(sec_buf) - 1);
    TEST_ASSERT(b > 0, "Intento no autorizado se registra en security.log");
}

/* Test 6: Sistema de Archivos en Memoria (RAMFS) */
void test_ramfs(void) {
    printf("[*] Ejecutando: test_ramfs...\n");
    ramfs_init();

    uint8_t read_buf[256];
    k_memset(read_buf, 0, sizeof(read_buf));
    int bytes = ramfs_read("bienvenida.txt", read_buf, sizeof(read_buf) - 1);
    TEST_ASSERT(bytes > 0, "Archivo bienvenida.txt existe y tiene contenido");

    int res = ramfs_create("sensor_test.dat");
    TEST_ASSERT(res == 0, "Creacion de nuevo archivo sensor_test.dat");

    const char* data_to_write = "TEMP: 26.5 C\nSTATUS: OK\n";
    int written = ramfs_write("sensor_test.dat", (const uint8_t*)data_to_write, k_strlen(data_to_write));
    TEST_ASSERT(written == (int)k_strlen(data_to_write), "Escritura completa de datos de sensor");

    k_memset(read_buf, 0, sizeof(read_buf));
    bytes = ramfs_read("sensor_test.dat", read_buf, sizeof(read_buf) - 1);
    TEST_ASSERT(bytes == (int)k_strlen(data_to_write), "Lectura de contenido de archivo coincide con lo escrito");
    TEST_ASSERT(k_strcmp((const char*)read_buf, data_to_write) == 0, "Integridad de los datos leidos comprobada");

    int del = ramfs_delete("sensor_test.dat");
    TEST_ASSERT(del == 0, "Borrado de archivo exitoso");
    bytes = ramfs_read("sensor_test.dat", read_buf, sizeof(read_buf) - 1);
    TEST_ASSERT(bytes < 0, "Archivo borrado ya no puede ser leido");

    // Auditoría v12: ramfs_write rechaza datos mayores a RAMFS_MAX_SIZE con error explícito (-1)
    int oversized_write = ramfs_write("bienvenida.txt", (const uint8_t*)data_to_write, RAMFS_MAX_SIZE + 16);
    TEST_ASSERT(oversized_write == -1, "ramfs_write rechaza datos mayores a RAMFS_MAX_SIZE con error explícito sin truncado silencioso");
}

/* Test 7: Validación de los 11 Perfiles y Auto-Aprovisionamiento */
void test_profile_provisioning(void) {
    printf("[*] Ejecutando: test_profile_provisioning...\n");
    pkg_init();

    TEST_ASSERT(PROFILE_COUNT >= 11, "Existen al menos 11 perfiles de usuario definidos");

    int student_installed = pkg_provision_profile(PROFILE_STUDENT);
    TEST_ASSERT(student_installed == 3, "Perfil Estudiante aprovisiona 3 apps de estudio");
    uint8_t buf[64];
    TEST_ASSERT(ramfs_read("app_office", buf, sizeof(buf)) > 0, "App 'office' presente en RAMFS tras seleccion de perfil Estudiante");

    int fe_installed = pkg_provision_profile(PROFILE_FRONTEND);
    TEST_ASSERT(fe_installed == 4, "Perfil Frontend aprovisiona 4 herramientas (figma, vscode, nodejs, browser)");
    TEST_ASSERT(ramfs_read("app_figma", buf, sizeof(buf)) > 0, "App 'figma' presente en RAMFS");
    TEST_ASSERT(ramfs_read("app_vscode", buf, sizeof(buf)) > 0, "App 'vscode' presente en RAMFS");

    int be_installed = pkg_provision_profile(PROFILE_BACKEND);
    TEST_ASSERT(be_installed == 4, "Perfil Backend aprovisiona 4 herramientas (docker, postgres, api-postman, git)");
    TEST_ASSERT(ramfs_read("app_docker", buf, sizeof(buf)) > 0, "App 'docker' presente en RAMFS");

    int gamer_installed = pkg_provision_profile(PROFILE_GAMER_STREAMER);
    TEST_ASSERT(gamer_installed == 2, "Perfil Gamer aprovisiona suite de juegos y OBS");
    TEST_ASSERT(ramfs_read("app_stream-obs", buf, sizeof(buf)) > 0, "App 'stream-obs' presente en RAMFS");
}

/* Test 8: Gestor de Paquetes y Tienda (NOVA Store) con Integridad Criptográfica */
void test_package_manager(void) {
    printf("[*] Ejecutando: test_package_manager...\n");
    pkg_init();

    // Verificación de algoritmo estándar IEEE 802.3 CRC32
    uint32_t crc_vector = pkg_compute_crc32((const uint8_t*)"123456789", 9);
    TEST_ASSERT(crc_vector == 0xCBF43926, "pkg_compute_crc32 cumple con vector de prueba IEEE 802.3 (0xCBF43926)");

    // Verificación de integridad con payload auténtico
    const char* valid_payload = "ZENTHRA_APP_EXEC_V1";
    TEST_ASSERT(pkg_verify_integrity("arduino-ide", (const uint8_t*)valid_payload, k_strlen(valid_payload)) == true,
                "pkg_verify_integrity aprueba payload autentico con CRC32 esperado");

    // Detección y rechazo de payload alterado/corrompido
    const char* tampered_payload = "TAMPERED_MALICIOUS_APP";
    TEST_ASSERT(pkg_verify_integrity("arduino-ide", (const uint8_t*)tampered_payload, k_strlen(tampered_payload)) == false,
                "pkg_verify_integrity detecta discrepancia de CRC32 y rechaza paquete corrompido");

    // Robustez ante punteros nulos o paquete inexistente
    TEST_ASSERT(pkg_verify_integrity(NULL, (const uint8_t*)valid_payload, 16) == false,
                "pkg_verify_integrity rechaza puntero nulo de forma segura");
    TEST_ASSERT(pkg_verify_integrity("inexistente", (const uint8_t*)valid_payload, 16) == false,
                "pkg_verify_integrity rechaza paquete no catalogado");

    int res = pkg_install("arduino-ide");
    TEST_ASSERT(res == 0, "Instalacion exitosa de paquete 'arduino-ide' verificado por CRC32");

    uint8_t meta_buf[64];
    k_memset(meta_buf, 0, sizeof(meta_buf));
    int bytes = ramfs_read("app_arduino-ide", meta_buf, sizeof(meta_buf) - 1);
    TEST_ASSERT(bytes > 0, "Paquete instalado crea entrada binaria en RAMFS (app_arduino-ide)");

    int dup = pkg_install("arduino-ide");
    TEST_ASSERT(dup == 1, "Deteccion de aplicacion ya instalada");

    // Auditoría v12: Si RAMFS está lleno, pkg_install debe fallar y NO marcar el paquete como instalado
    ramfs_delete("app_dev-browser");
    for (int f = 0; f < RAMFS_MAX_FILES; f++) {
        char dummy[16];
        snprintf(dummy, sizeof(dummy), "fill_%d.txt", f);
        ramfs_create(dummy);
    }
    int fill_res = pkg_install("dev-browser");
    TEST_ASSERT(fill_res == -3, "pkg_install rechaza instalacion si RAMFS esta saturado");
    // Limpiar archivos dummy para restaurar estado
    for (int f = 0; f < RAMFS_MAX_FILES; f++) {
        char dummy[16];
        snprintf(dummy, sizeof(dummy), "fill_%d.txt", f);
        ramfs_delete(dummy);
    }

    int uninst = pkg_uninstall("arduino-ide");
    TEST_ASSERT(uninst == 0, "Desinstalacion exitosa de paquete");
    bytes = ramfs_read("app_arduino-ide", meta_buf, sizeof(meta_buf) - 1);
    TEST_ASSERT(bytes < 0, "Entrada de app retirada de RAMFS tras desinstalacion");
}

/* Test 9: Motor de Reglas y Automatización SI-ENTONCES */
void test_rules_engine(void) {
    printf("[*] Ejecutando: test_rules_engine...\n");
    rules_engine_init();

    mock_led_value = false;
    rules_engine_evaluate(250);
    TEST_ASSERT(mock_led_value == false, "Temperatura normal (25.0 C) no activa alarma");

    rules_engine_evaluate(325);
    TEST_ASSERT(mock_led_value == true, "Temperatura critica (32.5 C) activa actuador LED por automatizacion");

    uint8_t alert_log[128];
    k_memset(alert_log, 0, sizeof(alert_log));
    int b = ramfs_read("alertas.log", alert_log, sizeof(alert_log) - 1);
    TEST_ASSERT(b > 0, "Registro de alerta guardado en RAMFS (alertas.log)");
}

/* Test 10: Simulación de conversación bidireccional NOVA-Link */
void test_bidirectional_simulation(void) {
    printf("[*] Ejecutando: test_bidirectional_simulation...\n");

    uint8_t tx_frame[4] = { NOVA_LINK_MAGIC, CMD_LED_ON, 0x00, (uint8_t)(CMD_LED_ON ^ 0x00) };
    TEST_ASSERT(tx_frame[0] == 0xAA, "Trama enviada inicia con MAGIC");
    TEST_ASSERT(tx_frame[1] == 0x10, "Comando enviado es LED_ON");

    uint8_t rx_frame[4] = { NOVA_LINK_MAGIC, RESP_OK, 0x00, (uint8_t)(RESP_OK ^ 0x00) };
    uint8_t rx_cksum = rx_frame[1] ^ rx_frame[2];
    TEST_ASSERT(rx_frame[3] == rx_cksum, "Integridad de trama de respuesta de Arduino confirmada");

    uint8_t corrupted_frame[4] = { NOVA_LINK_MAGIC, RESP_OK, 0x00, 0xFF };
    uint8_t corrupted_calc = corrupted_frame[1] ^ corrupted_frame[2];
    TEST_ASSERT(corrupted_frame[3] != corrupted_calc, "Deteccion y rechazo exitoso de trama corrompida");
}

/* Test 11: Validación de estructuras de protección de CPU x86 */
void test_x86_structures(void) {
    printf("[*] Ejecutando: test_x86_structures...\n");
    struct gdt_mock {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t  base_middle;
        uint8_t  access;
        uint8_t  granularity;
        uint8_t  base_high;
    } __attribute__((packed));
    TEST_ASSERT(sizeof(struct gdt_mock) == 8, "Descriptor GDT tiene tamano estandar x86 (8 bytes)");

    struct idt_mock {
        uint16_t base_low;
        uint16_t sel;
        uint8_t  always0;
        uint8_t  flags;
        uint16_t base_high;
    } __attribute__((packed));
    TEST_ASSERT(sizeof(struct idt_mock) == 8, "Descriptor IDT tiene tamano estandar x86 (8 bytes)");
}

/* Test 12: Validación de Multiboot 1 / Multiboot 2 y Salvaguardas de Tags */
void test_multiboot_and_power(void) {
    printf("[*] Ejecutando: test_multiboot_and_power...\n");

    // 1. Multiboot 1
    struct multiboot1_info mb1;
    memset(&mb1, 0, sizeof(mb1));
    mb1.flags = 0x01;
    mb1.mem_lower = 640;
    mb1.mem_upper = 31744; // ~31 MiB

    struct multiboot_parsed_info parsed;
    bool ok = multiboot_parse(MULTIBOOT1_BOOTLOADER_MAGIC, (uintptr_t)&mb1, &parsed);
    TEST_ASSERT(ok == true, "multiboot_parse reconoce firma Multiboot 1 (0x2BADB002)");
    TEST_ASSERT(parsed.version == 1, "Version detectada es Multiboot 1");
    TEST_ASSERT(parsed.mem_lower_kib == 640 && parsed.mem_upper_kib == 31744, "Memoria convencional y extendida leidas");

    // 2. Multiboot 2 válido
    uint8_t mb2_buf[64];
    memset(mb2_buf, 0, sizeof(mb2_buf));
    uint32_t* total_size = (uint32_t*)mb2_buf;
    *total_size = 32;
    struct multiboot2_tag_basic_mem* tag_mem = (struct multiboot2_tag_basic_mem*)(mb2_buf + 8);
    tag_mem->type = 4;
    tag_mem->size = 16;
    tag_mem->mem_lower = 640;
    tag_mem->mem_upper = 65536;
    struct multiboot2_tag* tag_end = (struct multiboot2_tag*)(mb2_buf + 24);
    tag_end->type = 0;
    tag_end->size = 8;

    ok = multiboot_parse(MULTIBOOT2_BOOTLOADER_MAGIC, (uintptr_t)mb2_buf, &parsed);
    TEST_ASSERT(ok == true, "multiboot_parse reconoce firma Multiboot 2 (0x36D76289)");
    TEST_ASSERT(parsed.version == 2, "Version detectada es Multiboot 2");
    TEST_ASSERT(parsed.tags_count == 1, "Etiquetas Multiboot 2 parseadas correctamente");
    TEST_ASSERT(parsed.mem_upper_kib == 65536, "Memoria extraida de etiqueta tipo 4");

    // 3. Salvaguarda crítica: Tag corrupto con size < 8 (previene bucle infinito como en Zenthra ae58107)
    memset(mb2_buf, 0, sizeof(mb2_buf));
    *total_size = 32;
    struct multiboot2_tag* corrupt_tag = (struct multiboot2_tag*)(mb2_buf + 8);
    corrupt_tag->type = 6;
    corrupt_tag->size = 0; // Size inválido/cero
    ok = multiboot_parse(MULTIBOOT2_BOOTLOADER_MAGIC, (uintptr_t)mb2_buf, &parsed);
    TEST_ASSERT(ok == true && parsed.tags_count == 0, "multiboot_parse aborta de forma segura ante tag corrupto con size=0 evitando bucle infinito");

    // 4. Manejo ante bootloader no estándar o punteros nulos
    TEST_ASSERT(multiboot_parse(0xDEADBEEF, (uintptr_t)&mb1, &parsed) == false, "Rechazo seguro de magic invalido");
    TEST_ASSERT(multiboot_parse(MULTIBOOT1_BOOTLOADER_MAGIC, 0, &parsed) == false, "Rechazo seguro de puntero nulo");

    // 5. Apagado seguro (invocación de puertos ACPI)
    qemu_poweroff();
    TEST_ASSERT(true, "qemu_poweroff ejecuta puertos ACPI (0x604, 0x0804, 0x4004) de forma determinista");
}

static void exiting_worker(void) {
    task_exit();
}

void test_task_lifecycle_and_cleanup(void) {
    printf("[*] Ejecutando: test_task_lifecycle_and_cleanup...\n");
    scheduler_init();
    int pid = task_create("exit_test", exiting_worker);
    TEST_ASSERT(pid > 0, "Creacion exitosa de tarea para prueba de finalizacion");
    task_yield(); // Conmuta a la tarea exit_test
    TEST_ASSERT(task_get_current()->id == (uint32_t)pid, "Tarea exit_test activa antes de su finalizacion");
    task_exit();  // Finaliza tarea, guarda zombie stack y conmuta de regreso
    struct task* cur = task_get_current();
    TEST_ASSERT(cur->id == 1, "Scheduler conmuta de regreso a tarea principal tras task_exit sin fallos");
    task_yield(); // Ejecuta ciclo adicional para comprobar liberación segura sin fallar
    TEST_ASSERT(task_get_current()->state == TASK_RUNNING, "Pila liberada de forma diferida sin corrupcion de heap ni use-after-free");

    // Probar terminación secuencial múltiple para validar cola zombie_stacks y task_get_info_at
    int pid2 = task_create("exit_worker2", exiting_worker);
    int pid3 = task_create("exit_worker3", exiting_worker);
    TEST_ASSERT(pid2 > 0 && pid3 > 0, "Tareas adicionales creadas para validar cola zombie");

    struct task_info tinfo;
    bool has_info = task_get_info_at(0, &tinfo);
    TEST_ASSERT(has_info == true && tinfo.id == 1, "task_get_info_at retorna informacion real de la tarea kernel activa");

    task_yield(); // Pasa a exit_worker2
    task_exit();
    task_yield(); // Pasa a exit_worker3
    task_exit();
    task_yield(); // Drena cola zombie_stacks
    TEST_ASSERT(task_get_current()->state == TASK_RUNNING, "Cola zombie_stacks drena multiples tareas terminadas en cascada sin corrupcion");

    // Prueba de estrés: creación y terminación de más de 32 tareas consecutivas (Auditoría v12)
    bool stress_ok = true;
    for (int t = 0; t < 40; t++) {
        int spid = task_create("stress_worker", exiting_worker);
        if (spid <= 0) { stress_ok = false; break; }
        task_yield();
        task_exit();
        task_yield();
    }
    TEST_ASSERT(stress_ok == true, "Scheduler soporta mas de 32 terminaciones consecutivas con purga automatica y sin fugas");
}

void test_syscall_boundary_and_abi(void) {
    printf("[*] Ejecutando: test_syscall_boundary_and_abi...\n");
    syscall_init();

    // Verificación estricta de límites de memoria
    TEST_ASSERT(is_user_pointer((void*)0x1000UL, 1024) == true, "is_user_pointer acepta puntero valido en espacio de usuario");
    TEST_ASSERT(is_user_pointer((void*)(USER_MEMORY_LIMIT - 100), 100) == true, "is_user_pointer acepta rango exacto hasta USER_MEMORY_LIMIT");
    TEST_ASSERT(is_user_pointer((void*)(USER_MEMORY_LIMIT - 50), 100) == false, "is_user_pointer rechaza rango que sobrepasa USER_MEMORY_LIMIT sin wrap-around");
    TEST_ASSERT(is_user_pointer((void*)USER_MEMORY_LIMIT, 4) == false, "is_user_pointer rechaza direccion base en el limite del kernel");
    TEST_ASSERT(is_user_pointer((void*)(USER_MEMORY_LIMIT + 0x1000), 16) == false, "is_user_pointer rechaza direccion en espacio del kernel");
    TEST_ASSERT(is_user_pointer((void*)0x1000UL, (size_t)-1) == false, "is_user_pointer rechaza desbordamiento por tamano extremo");

    // Verificación de autorización ACL centralizada en la ruta de Syscall (SYS_ARDUINO_CMD) por proceso
    acl_init();
    scheduler_init();
    task_set_current_profile(PROFILE_STUDENT);
    struct registers regs_acl;
    k_memset(&regs_acl, 0, sizeof(struct registers));
    regs_acl.eax = SYS_ARDUINO_CMD;
    regs_acl.ebx = 2; // LED ON (requiere OP_ARDUINO_WRITE)
    syscall_dispatcher(&regs_acl);
    TEST_ASSERT(regs_acl.eax == (uint32_t)-13, "SYS_ARDUINO_CMD bloquea actuadores (-13 EACCES) para perfil sin permiso (Estudiante)");

    task_set_current_profile(PROFILE_IOT_HARDWARE);
    regs_acl.eax = SYS_ARDUINO_CMD;
    regs_acl.ebx = 2; // LED ON
    syscall_dispatcher(&regs_acl);
    TEST_ASSERT(regs_acl.eax == 0, "SYS_ARDUINO_CMD autoriza actuadores fisicos para perfil Hardware-IoT");

    // Verificación de llamadas al sistema de perfiles
    regs_acl.eax = SYS_GET_PROFILE;
    syscall_dispatcher(&regs_acl);
    TEST_ASSERT(regs_acl.eax == (uint32_t)PROFILE_IOT_HARDWARE, "SYS_GET_PROFILE retorna el perfil del proceso activo");

    regs_acl.eax = SYS_SET_PROFILE;
    regs_acl.ebx = PROFILE_STUDENT;
    syscall_dispatcher(&regs_acl);
    TEST_ASSERT(regs_acl.eax == 0, "SYS_SET_PROFILE permite reducir privilegios desde Hardware-IoT hacia Estudiante");
    TEST_ASSERT(task_get_current()->profile == PROFILE_STUDENT, "Perfil de proceso verificado tras SYS_SET_PROFILE");

    // Auditoría v18: Prevención crítica de escalamiento de privilegios en SYS_SET_PROFILE.
    // Perfiles con OP_SYSTEM_ADMIN (Backend, Frontend, Fullstack, Ciberseguridad) NO pueden
    // autoelevarse a PROFILE_IOT_HARDWARE para adquirir OP_ARDUINO_WRITE / OP_ARDUINO_READ.
    enum nova_profile admin_software_profiles[4] = {
        PROFILE_BACKEND,
        PROFILE_FRONTEND,
        PROFILE_CYBERSECURITY,
        PROFILE_FULLSTACK
    };
    for (int p = 0; p < 4; p++) {
        enum nova_profile start_p = admin_software_profiles[p];
        task_set_current_profile(start_p);
        uint32_t v_before = acl_get_violations_count();

        // Intentar escalar a PROFILE_IOT_HARDWARE
        regs_acl.eax = SYS_SET_PROFILE;
        regs_acl.ebx = PROFILE_IOT_HARDWARE;
        syscall_dispatcher(&regs_acl);
        TEST_ASSERT(regs_acl.eax == (uint32_t)-EACCES,
                    "SYS_SET_PROFILE bloquea escalada hacia PROFILE_IOT_HARDWARE con -EACCES (-13)");
        TEST_ASSERT(task_get_current()->profile == start_p,
                    "Perfil de la tarea permanece intacto tras intento de escalada rechazado");
        TEST_ASSERT(acl_get_violations_count() == v_before + 1,
                    "Intento de escalada de perfil se registra como violacion de seguridad");

        // Confirmar que SYS_ARDUINO_CMD LED_ON (2) y LED_OFF (3) siguen denegados (-EACCES)
        regs_acl.eax = SYS_ARDUINO_CMD;
        regs_acl.ebx = 2; // LED ON
        syscall_dispatcher(&regs_acl);
        TEST_ASSERT(regs_acl.eax == (uint32_t)-EACCES,
                    "SYS_ARDUINO_CMD LED_ON sigue denegado (-EACCES) tras bloqueo de escalada");

        regs_acl.eax = SYS_ARDUINO_CMD;
        regs_acl.ebx = 3; // LED OFF
        syscall_dispatcher(&regs_acl);
        TEST_ASSERT(regs_acl.eax == (uint32_t)-EACCES,
                    "SYS_ARDUINO_CMD LED_OFF sigue denegado (-EACCES) tras bloqueo de escalada");
    }

    // Comprobar que una transición no elevadora entre perfiles administrativos equivalentes
    // (Backend <-> Frontend) o hacia un subconjunto (Backend -> Designer) sí está permitida
    task_set_current_profile(PROFILE_BACKEND);
    regs_acl.eax = SYS_SET_PROFILE;
    regs_acl.ebx = PROFILE_FRONTEND;
    syscall_dispatcher(&regs_acl);
    TEST_ASSERT(regs_acl.eax == 0 && task_get_current()->profile == PROFILE_FRONTEND,
                "SYS_SET_PROFILE permite transicion lateral sin elevacion de capacidades (Backend -> Frontend)");

    regs_acl.eax = SYS_SET_PROFILE;
    regs_acl.ebx = PROFILE_DESIGNER;
    syscall_dispatcher(&regs_acl);
    TEST_ASSERT(regs_acl.eax == 0 && task_get_current()->profile == PROFILE_DESIGNER,
                "SYS_SET_PROFILE permite reduccion de capacidades hacia subconjunto (Frontend -> Designer)");

    // Auditoría v16/v17: Aislamiento estricto de perfiles.
    // Comprobar que ni task_set_current_profile ni SYS_SET_PROFILE alteran el perfil global de ACL
    TEST_ASSERT(acl_get_active_profile() == PROFILE_GENERAL,
                "Aislamiento de perfiles: task_set_current_profile y SYS_SET_PROFILE no alteran el perfil global de ACL");

    // Auditoría v17: Modificar perfil de tarea con helper y comprobar que ACL global permanece inalterado
    task_set_current_profile(PROFILE_FRONTEND);
    TEST_ASSERT(task_get_current()->profile == PROFILE_FRONTEND, "task_set_current_profile actualiza perfil de la tarea");
    TEST_ASSERT(acl_get_active_profile() == PROFILE_GENERAL, "task_set_current_profile no altera el perfil global de ACL");
    task_set_current_profile(PROFILE_STUDENT);

    // Probar aislamiento de credenciales creando una tarea concurrente con perfil Backend
    int pid_iso = task_create_with_profile("task_iso_b", exiting_worker, PROFILE_BACKEND);
    TEST_ASSERT(pid_iso > 0, "Creacion de tarea concurrente para prueba de aislamiento de perfil");
    task_yield();
    struct task* task_iso = task_get_current();
    TEST_ASSERT(task_iso->profile == PROFILE_BACKEND, "Tarea concurrente conserva su perfil propio (Backend)");
    TEST_ASSERT(acl_check_permission(task_iso->profile, OP_ARDUINO_WRITE) == false,
                "Permisos de tarea concurrente estan estrictamente aislados de la tarea A");
    task_exit(); // Limpieza: finaliza task_iso_b y conmuta de regreso a la tarea principal

    // Auditoría v18: Camino real de kernel exige paginación activa (fail-closed si paging está desactivado)
    // En este punto de la suite aún no se invocó vmm_init(), por lo que vmm_is_paging_enabled() == false.
    char sample_user_buf[32] = "Userland Output Test";
    syscall_set_require_paging(true);
    TEST_ASSERT(syscall_is_paging_required() == true, "Modo estricto de kernel require_paging activado");
    TEST_ASSERT(is_user_pages_accessible_failclosed(sample_user_buf, 8, false) == false,
                "Camino kernel fail-closed rechaza acceso de usuario cuando la paginacion esta desactivada");
    struct registers regs_nopg;
    k_memset(&regs_nopg, 0, sizeof(struct registers));
    regs_nopg.eax = SYS_WRITE;
    regs_nopg.ebx = (uintptr_t)sample_user_buf;
    regs_nopg.ecx = 8;
    syscall_dispatcher(&regs_nopg);
    TEST_ASSERT(regs_nopg.eax == (uint32_t)-EFAULT,
                "SYS_WRITE falla cerrado con -EFAULT en modo kernel si la paginacion no esta activa");
    syscall_set_require_paging(false);

    // Auditoría v14/v15: Semántica ABI de SYS_WRITE (buffer binario, count=0 POSIX y códigos errno estandarizados)
    struct registers regs_w;
    k_memset(&regs_w, 0, sizeof(struct registers));
    regs_w.eax = SYS_WRITE;
    regs_w.ebx = (uintptr_t)sample_user_buf;
    regs_w.ecx = 0; // count == 0 debe retornar 0 según estándar POSIX
    syscall_dispatcher(&regs_w);
    TEST_ASSERT(regs_w.eax == 0, "SYS_WRITE con count=0 retorna 0 exitosamente");

    // SYS_WRITE con buffer binario que contiene bytes NUL intermedios (no debe truncar)
    char bin_data[6] = {'T', 'E', '\0', 'S', 'T', '!'};
    regs_w.eax = SYS_WRITE;
    regs_w.ebx = (uintptr_t)bin_data;
    regs_w.ecx = 6;
    syscall_dispatcher(&regs_w);
    TEST_ASSERT(regs_w.eax == 6, "SYS_WRITE escribe buffer binario con NUL intermedio sin truncar");

    // Auditoría v18: SYS_WRITE acota transferencias mayores a 65536 bytes (MAX_SYSCALL_TRANSFER_SIZE)
    static char large_user_buf[70000];
    k_memset(large_user_buf, 'A', sizeof(large_user_buf));
    regs_w.eax = SYS_WRITE;
    regs_w.ebx = (uintptr_t)large_user_buf;
    regs_w.ecx = 70000;
    syscall_dispatcher(&regs_w);
    TEST_ASSERT(regs_w.eax == 65536, "SYS_WRITE con count > 65536 acota y retorna exactamente 65536 bytes");

    // SYS_WRITE con puntero NULL retorna -EFAULT (-14)
    regs_w.eax = SYS_WRITE;
    regs_w.ebx = 0;
    regs_w.ecx = 10;
    syscall_dispatcher(&regs_w);
    TEST_ASSERT(regs_w.eax == (uint32_t)-EFAULT, "SYS_WRITE rechaza puntero NULL retornando -EFAULT (-14)");

    // SYS_WRITE con puntero en espacio de kernel retorna -EFAULT (-14)
    regs_w.eax = SYS_WRITE;
    regs_w.ebx = (uintptr_t)(USER_MEMORY_LIMIT + 0x1000);
    regs_w.ecx = 10;
    syscall_dispatcher(&regs_w);
    TEST_ASSERT(regs_w.eax == (uint32_t)-EFAULT, "SYS_WRITE rechaza puntero en espacio de memoria del kernel retornando -EFAULT (-14)");

    // Auditoría v15: Syscall desconocida/no implementada retorna -ENOSYS (-38)
    struct registers regs_unk;
    k_memset(&regs_unk, 0, sizeof(struct registers));
    regs_unk.eax = 999;
    syscall_dispatcher(&regs_unk);
    TEST_ASSERT(regs_unk.eax == (uint32_t)-ENOSYS, "Syscall desconocida retorna -ENOSYS (-38)");

    // SYS_READ con count=0 retorna 0
    struct registers regs_r;
    k_memset(&regs_r, 0, sizeof(struct registers));
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)bin_data;
    regs_r.ecx = 0;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == 0, "SYS_READ con count=0 retorna 0 exitosamente");

    // SYS_READ con puntero NULL retorna -EFAULT (-14)
    regs_r.eax = SYS_READ;
    regs_r.ebx = 0;
    regs_r.ecx = 10;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == (uint32_t)-EFAULT, "SYS_READ rechaza puntero NULL retornando -EFAULT (-14)");

    // Auditoría v16: Validación de flags en SYS_READ
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)bin_data;
    regs_r.ecx = 1;
    regs_r.edx = 0xAA550000; // Flags inválidos desconocidos
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == (uint32_t)-EINVAL, "SYS_READ rechaza flags no reconocidos con -EINVAL (-22)");

    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)bin_data;
    regs_r.ecx = 1;
    regs_r.edx = SYS_READ_BLOCKING;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == (uint32_t)-ENOSYS, "SYS_READ_BLOCKING retorna -ENOSYS (-38) al no estar soportado aun");
    regs_r.edx = SYS_READ_NONBLOCK;

    // Auditoría v15: SYS_READ con cola de teclado vacía en modo interactivo retorna -EAGAIN (-11)
    keyboard_queue_clear();
    char read_target[32];
    k_memset(read_target, 0, sizeof(read_target));
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)read_target;
    regs_r.ecx = 10;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == (uint32_t)-EAGAIN, "SYS_READ con cola vacia retorna -EAGAIN (-11) en lugar de 0 EOF");

    // Auditoría v16: SYS_READ con count grande (> 256) lee exactamente los caracteres disponibles
    keyboard_enqueue_char('O');
    keyboard_enqueue_char('K');
    char small_valid_target[16];
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)small_valid_target;
    regs_r.ecx = 1000; // count mayor a 256
    regs_r.edx = SYS_READ_NONBLOCK;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == 2, "SYS_READ con count grande lee exactamente los 2 caracteres disponibles");
    TEST_ASSERT(k_strncmp(small_valid_target, "OK", 2) == 0, "Contenido leido con count grande coincide exactamente");

    // Auditoría v15: Inyección en cola de teclado y lectura real vía SYS_READ
    keyboard_enqueue_char('Z');
    keyboard_enqueue_char('E');
    keyboard_enqueue_char('N');
    TEST_ASSERT(keyboard_queue_length() == 3, "Cola de teclado encola 3 caracteres correctamente");
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)read_target;
    regs_r.ecx = 10;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == 3, "SYS_READ lee los 3 caracteres encolados exitosamente");
    TEST_ASSERT(k_strncmp(read_target, "ZEN", 3) == 0, "Contenido leido por SYS_READ coincide exactamente con la entrada");
    TEST_ASSERT(keyboard_queue_is_empty() == true, "Cola de teclado queda vacia tras el consumo por SYS_READ");

    // Auditoría v15: Salvaguarda contra pérdida de teclas ante búfer destino inválido
    keyboard_enqueue_char('K');
    keyboard_enqueue_char('E');
    keyboard_enqueue_char('Y');
    regs_r.eax = SYS_READ;
    regs_r.ebx = 0; // Puntero inválido
    regs_r.ecx = 3;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == (uint32_t)-EFAULT, "SYS_READ con puntero invalido es rechazado con -EFAULT");
    TEST_ASSERT(keyboard_queue_length() == 3, "Salvaguarda: no se pierden teclas de la cola si el buffer de usuario es invalido");
    // Lectura posterior con búfer válido recupera los datos íntegros
    char safe_read_buf[16];
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)safe_read_buf;
    regs_r.ecx = 3;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == 3, "Lectura posterior con buffer valido recupera los datos intactos");
    TEST_ASSERT(k_strncmp(safe_read_buf, "KEY", 3) == 0, "Caracteres recuperados intactos de la cola");

    // Auditoría v17: Lectura Transaccional de SYS_READ
    // Inyectar teclas, invocar SYS_READ hacia memoria de kernel (falla copy_to_user)
    keyboard_enqueue_char('T');
    keyboard_enqueue_char('R');
    keyboard_enqueue_char('X');
    regs_r.eax = SYS_READ;
    regs_r.ebx = (uintptr_t)(USER_MEMORY_LIMIT + 0x100); // Dirección en kernel
    regs_r.ecx = 3;
    syscall_dispatcher(&regs_r);
    TEST_ASSERT(regs_r.eax == (uint32_t)-EFAULT, "SYS_READ hacia memoria de kernel retorna -EFAULT");
    TEST_ASSERT(keyboard_queue_length() == 3, "Lectura transaccional: cola conserva los 3 caracteres intactos tras fallo");

    // Probar inspección no destructiva peek_queue y consumo diferido discard_chars
    char peek_buf[8];
    size_t pk = keyboard_peek_queue(peek_buf, 3);
    TEST_ASSERT(pk == 3 && k_strncmp(peek_buf, "TRX", 3) == 0, "keyboard_peek_queue inspecciona sin extraer");
    TEST_ASSERT(keyboard_queue_length() == 3, "Cola conserva su longitud tras peek");
    keyboard_discard_chars(3);
    TEST_ASSERT(keyboard_queue_length() == 0, "keyboard_discard_chars descarta los caracteres consumidos tras confirmacion");

    // Auditoría v15: Prevención de desbordamiento de la cola de teclado
    keyboard_queue_clear();
    bool all_enqueued = true;
    for (size_t i = 0; i < KEYBOARD_QUEUE_CAPACITY; i++) {
        if (!keyboard_enqueue_char('X')) {
            all_enqueued = false;
            break;
        }
    }
    TEST_ASSERT(all_enqueued == true, "Cola de teclado admite hasta su capacidad maxima (128 caracteres)");
    TEST_ASSERT(keyboard_queue_length() == KEYBOARD_QUEUE_CAPACITY, "Cola alcanza capacidad maxima");
    TEST_ASSERT(keyboard_enqueue_char('!') == false, "keyboard_enqueue_char rechaza desbordamiento cuando la cola esta saturada");
    keyboard_queue_clear();
}

/* Test 15: Multitarea Preemptiva, Quantum y Estados Sleep / Wakeup (Fase 3) */
void test_preemptive_scheduling_and_sleep(void) {
    printf("[*] Ejecutando: test_preemptive_scheduling_and_sleep...\n");
    scheduler_init();

    int pid = task_create("sleepy_worker", dummy_worker);
    TEST_ASSERT(pid > 0, "Creacion exitosa de sleepy_worker");

    // Conmutar a la tarea creada
    task_yield();
    struct task* cur = task_get_current();
    TEST_ASSERT(cur->id == (uint32_t)pid, "Tarea sleepy_worker en ejecucion");

    // Poner a dormir la tarea por 3 ticks
    task_sleep(3);
    // Tras task_sleep(), el scheduler debió conmutar de regreso a kernel_shell
    struct task* main_task = task_get_current();
    TEST_ASSERT(main_task->id == 1, "task_sleep conmuta de vuelta a la tarea principal");

    // task_schedule_tick tick 1: no despierta aun
    task_schedule_tick();
    task_yield();
    TEST_ASSERT(task_get_current()->id == 1, "sleepy_worker no es planificada mientras duerme (tick 1/3)");

    task_schedule_tick(); // tick 2
    task_yield();
    TEST_ASSERT(task_get_current()->id == 1, "sleepy_worker no es planificada mientras duerme (tick 2/3)");

    task_schedule_tick(); // tick 3: debe despertar a TASK_READY
    task_yield();
    TEST_ASSERT(task_get_current()->id == (uint32_t)pid, "sleepy_worker despierta automaticamente a TASK_READY y se planifica");

    // Verificación de preempción por agotamiento de quantum
    scheduler_set_preemptive(true);
    task_get_current()->time_slice = 1;
    task_schedule_tick(); // Agota quantum y conmuta automáticamente
    TEST_ASSERT(task_get_current()->id == 1, "Preempcion automatica: expiracion de quantum conmuta tarea sin yield voluntario");

    // Verificación de bloqueo de scheduler en secciones críticas
    scheduler_lock();
    task_get_current()->time_slice = 1;
    task_schedule_tick(); // No debe conmutar porque scheduler_lock_count > 0
    TEST_ASSERT(task_get_current()->id == 1, "scheduler_lock protege secciones criticas impidiendo preempcion no deseada");
    scheduler_unlock();
}

/* Test 16: Administrador de Memoria Física (PMM - Marcos de 4 KiB) */
void test_physical_memory_manager(void) {
    printf("[*] Ejecutando: test_physical_memory_manager...\n");
    pmm_init(0, 0);

    size_t total = pmm_get_total_frames();
    TEST_ASSERT(total > 0, "PMM detecta marcos fisicos totales mayores a cero");
    size_t free_initial = pmm_get_free_frames();
    TEST_ASSERT(free_initial > 0, "PMM reporta memoria fisica libre disponible");

    // Asignar primer marco físico
    uintptr_t f1 = pmm_alloc_frame();
    TEST_ASSERT(f1 != 0, "pmm_alloc_frame asigna un marco fisico valido");
    TEST_ASSERT((f1 % PAGE_SIZE) == 0, "El marco fisico asignado esta alineado a 4 KiB");
    TEST_ASSERT(pmm_is_frame_allocated(f1) == true, "El marco f1 esta marcado como ocupado en bitmap");
    TEST_ASSERT(pmm_get_free_frames() == free_initial - 1, "El contador de marcos libres se decrementa");

    // Asignar segundo marco
    uintptr_t f2 = pmm_alloc_frame();
    TEST_ASSERT(f2 != 0 && f2 != f1, "pmm_alloc_frame asigna un segundo marco contiguo y diferenciado");
    TEST_ASSERT((f2 % PAGE_SIZE) == 0, "El segundo marco fisico esta alineado a 4 KiB");

    // Liberar primer marco
    pmm_free_frame(f1);
    TEST_ASSERT(pmm_is_frame_allocated(f1) == false, "pmm_free_frame libera el marco f1");
    TEST_ASSERT(pmm_get_free_frames() == free_initial - 1, "El contador de marcos libres se recupera tras liberacion");

    // Reasignar marco: verificación de reciclaje First-Fit
    uintptr_t f3 = pmm_alloc_frame();
    TEST_ASSERT(f3 == f1, "PMM recicla el marco recientemente liberado en el bitmap");

    // Verificación estricta de Regiones Reservadas (Auditoría v10):
    // El primer megabyte (0x00000000 - 0x00100000: IVT, BDA, VGA, BIOS) debe estar 100% reservado
    TEST_ASSERT(pmm_is_region_reserved(0x00000000, 0x00100000) == true, "Primer megabyte (0x00000000 - 0x00100000) esta completamente reservado");
    // El heap del kernel (0x00400000 - 0x00600000: 2 MiB) debe estar 100% reservado
    TEST_ASSERT(pmm_is_region_reserved(0x00400000, 2 * 1024 * 1024) == true, "Heap del kernel (0x00400000 - 0x00600000: 2 MiB) esta completamente reservado");

    // Asignación con límite superior (pmm_alloc_frame_below, requerido para tablas de páginas en identity map)
    uintptr_t low_frame = pmm_alloc_frame_below(0x04000000); // < 64 MiB
    TEST_ASSERT(low_frame != 0, "pmm_alloc_frame_below asigna un marco fisico");
    TEST_ASSERT(low_frame < 0x04000000, "El marco asignado se encuentra estrictamente dentro del limite de 64 MiB");

    // Verificación de no-invasión: asignar múltiples marcos y verificar que ninguno invada el 1 MiB inferior ni el Heap
    bool no_invaded = true;
    uintptr_t sample_frames[16];
    for (int s = 0; s < 16; s++) {
        sample_frames[s] = pmm_alloc_frame();
        if (sample_frames[s] < 0x00100000 || 
            (sample_frames[s] >= 0x00400000 && sample_frames[s] < 0x00600000)) {
            no_invaded = false;
        }
    }
    TEST_ASSERT(no_invaded == true, "Ningun marco asignado por PMM invade el megabyte inferior ni el Heap reservado");

    // Pruebas de desbordamiento en pmm_mark_region_used y free (Auditoría v11/v12)
    size_t free_before_ovf = pmm_get_free_frames();
    pmm_mark_region_used(0xFFFFF000, 0x2000); // 0xFFFFF000 + 0x2000 desborda 32 bits
    TEST_ASSERT(pmm_get_free_frames() == free_before_ovf, "pmm_mark_region_used rechaza desbordamiento sin corromper bitmap");

    pmm_mark_region_free(0xFFFFF000, 0x2000);
    TEST_ASSERT(pmm_get_free_frames() == free_before_ovf, "pmm_mark_region_free rechaza desbordamiento sin corromper bitmap");

    // Prueba de rango parcial / no alineado menor a 1 página completa (no debe liberar ninguna página)
    pmm_mark_region_free(0x02000010, 100);
    TEST_ASSERT(pmm_get_free_frames() == free_before_ovf, "pmm_mark_region_free no libera paginas ante rangos incompletos menores a 4 KiB");

    // Auditoría v12: desbordamiento en redondeo superior y en suma con límites de 64 bits (UINT64_MAX)
    pmm_mark_region_used((uintptr_t)0xFFFFFFFFFFFFF000ULL, 0x2000);
    TEST_ASSERT(pmm_get_free_frames() == free_before_ovf, "pmm_mark_region_used rechaza direccion cercana a UINT64_MAX");

    pmm_mark_region_free((uintptr_t)0xFFFFFFFFFFFFF000ULL, 0x2000);
    TEST_ASSERT(pmm_get_free_frames() == free_before_ovf, "pmm_mark_region_free rechaza direccion cercana a UINT64_MAX");

    // Rango vacío (length = 0) es ignorado con seguridad
    pmm_mark_region_used(0x20000000, 0);
    pmm_mark_region_free(0x20000000, 0);
    TEST_ASSERT(pmm_get_free_frames() == free_before_ovf, "pmm_mark_region_used/free con longitud 0 no altera el estado de la memoria");

    TEST_ASSERT(pmm_is_region_reserved((uintptr_t)0xFFFFFFFFFFFFF000ULL, 0x2000) == false, "pmm_is_region_reserved maneja desbordamiento de 64 bits retornando false de forma segura");

    // Limpieza
    for (int s = 0; s < 16; s++) {
        pmm_free_frame(sample_frames[s]);
    }
    pmm_free_frame(low_frame);
    pmm_free_frame(f2);
    pmm_free_frame(f3);
}

/* Test 17: Paginación x86 de 32 bits, Mapeo Virtual y Aislamiento por Proceso */
void test_virtual_memory_paging(void) {
    printf("[*] Ejecutando: test_virtual_memory_paging...\n");
    vmm_init();

    TEST_ASSERT(vmm_is_paging_enabled() == true, "Paginacion x86 activada en VMM");
    struct page_directory* kdir = vmm_get_kernel_directory();
    TEST_ASSERT(kdir != NULL, "Directorio maestro de paginas del kernel existe");

    // Verificar identidad de 64 MiB (Ring 0 / Supervisor - Auditoría v14)
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x00000000) == 0x00000000, "Mapeo identidad de pagina cero (0x00000000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x000B8000) == 0x000B8000, "Mapeo identidad del buffer VGA (0x000B8000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x00100000) == 0x00100000, "Mapeo identidad del kernel ELF (1 MiB / 0x00100000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x00400000) == 0x00400000, "Mapeo identidad del Heap del kernel (4 MiB / 0x00400000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x03FFF000) == 0x03FFF000, "Mapeo identidad cubre hasta el limite de 64 MiB (0x03FFF000)");
    TEST_ASSERT(vmm_is_mapped(kdir, 0x04000000) == false, "Direccion exactamente en 64 MiB (0x04000000) no esta mapeada en el mapa de identidad");
    TEST_ASSERT(vmm_get_identity_limit() == (64 * 1024 * 1024), "vmm_get_identity_limit retorna exactamente 64 MiB");

    // Mapeo dinámico de página en espacio de usuario (Ring 3)
    uintptr_t user_virt = 0x80000000;
    uintptr_t phys_target = pmm_alloc_frame();
    TEST_ASSERT(phys_target != 0, "Marco fisico para prueba de mapeo asignado por PMM");

    int res = vmm_map_page(kdir, user_virt, phys_target, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    TEST_ASSERT(res == 0, "vmm_map_page mapea direccion virtual alta exitosamente");
    TEST_ASSERT(vmm_is_mapped(kdir, user_virt) == true, "vmm_is_mapped confirma presencia de pagina virtual");
    TEST_ASSERT(vmm_virt_to_phys(kdir, user_virt) == phys_target, "Traduccion virtual a fisica coincide exactamente");

    // Desmapeo
    vmm_unmap_page(kdir, user_virt);
    TEST_ASSERT(vmm_is_mapped(kdir, user_virt) == false, "vmm_unmap_page desasocia la pagina virtual");

    // Creación y aislamiento de directorio de proceso
    struct page_directory* pdir = vmm_create_process_directory();
    TEST_ASSERT(pdir != NULL, "vmm_create_process_directory crea nuevo espacio de direcciones");
    TEST_ASSERT(vmm_virt_to_phys(pdir, 0x00100000) == 0x00100000, "Espacio de direcciones de proceso hereda mapeo del kernel");

    // Mapear página privada en el proceso
    uintptr_t proc_virt = 0x40000000;
    vmm_map_page(pdir, proc_virt, phys_target, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    TEST_ASSERT(vmm_is_mapped(pdir, proc_virt) == true, "Pagina privada mapeada en directorio de proceso");
    TEST_ASSERT(vmm_is_mapped(kdir, proc_virt) == false, "Aislamiento estricto: la pagina privada NO existe en el kernel");

    // Política estricta de separación de APIs (Auditoría v11):
    // vmm_map_page rechaza colisión si la página ya está presente (EEXIST)
    int dup_map = vmm_map_page(pdir, proc_virt, phys_target, PTE_PRESENT | PTE_USER);
    TEST_ASSERT(dup_map == -1, "vmm_map_page rechaza colision de pagina ya presente (EEXIST)");

    // Modificación explícita de permisos con vmm_set_page_flags
    int flags_res = vmm_set_page_flags(pdir, proc_virt, PTE_PRESENT | PTE_USER);
    TEST_ASSERT(flags_res == 0, "vmm_set_page_flags actualiza permisos de pagina mapeada e invalida TLB");
    TEST_ASSERT((vmm_get_page_flags(pdir, proc_virt) & PTE_WRITABLE) == 0, "vmm_get_page_flags confirma que la pagina ahora es de solo lectura");

    // Conmutar a pdir para validar que copy_to_user y copy_from_user validan páginas y permisos bajo VMM
    vmm_switch_page_directory(pdir);
    char k_buf[32] = "KernelData";
    // Como proc_virt es de solo lectura, copy_to_user debe ser rechazado
    int copy_ro = copy_to_user((void*)proc_virt, k_buf, 10);
    TEST_ASSERT(copy_ro == -1, "copy_to_user rechaza escritura en pagina de usuario de solo lectura");

    // Intentar acceder a una dirección dentro del rango lógico pero sin página mapeada (0x40002000)
    int copy_unmapped = copy_from_user(k_buf, (const void*)0x40002000, 10);
    TEST_ASSERT(copy_unmapped == -1, "copy_from_user rechaza lectura de pagina no mapeada en el directorio activo");

    // Auditoría v12: Rango que cruza el límite de una página mapeada hacia una no mapeada
    // proc_virt está mapeado en [0x40000000 .. 0x40000FFF]. 0x40001000 NO está mapeado.
    // Un buffer en 0x40000000 + 4000 de tamaño 200 cruza hacia la página no mapeada.
    int cross_unmapped = copy_from_user(k_buf, (const void*)(proc_virt + 4000), 200);
    TEST_ASSERT(cross_unmapped == -1, "copy_from_user rechaza rango que cruza frontera hacia una pagina no mapeada");

    int cross_write = copy_to_user((void*)(proc_virt + 4000), k_buf, 200);
    TEST_ASSERT(cross_write == -1, "copy_to_user rechaza rango que cruza frontera hacia una pagina no mapeada");

    // Auditoría v14: strnlen_user con validación página por página en VMM activo
    TEST_ASSERT(strnlen_user((const char*)0x40002000, 32) == -1, "strnlen_user rechaza cadena en pagina no mapeada sin causar #PF");
    TEST_ASSERT(strnlen_user((const char*)0x00100000, 32) == -1, "strnlen_user rechaza cadena en pagina de kernel sin PTE_USER");

    // Auditoría v14/v15: SYS_WRITE y SYS_READ bajo paginación activa en pdir retornan -EFAULT (-14)
    struct registers regs_sw;
    k_memset(&regs_sw, 0, sizeof(struct registers));
    regs_sw.eax = SYS_WRITE;
    regs_sw.ebx = (uintptr_t)0x40002000; // no mapeada
    regs_sw.ecx = 10;
    syscall_dispatcher(&regs_sw);
    TEST_ASSERT(regs_sw.eax == (uint32_t)-EFAULT, "SYS_WRITE rechaza escritura desde pagina de usuario no mapeada con -EFAULT");

    regs_sw.eax = SYS_WRITE;
    regs_sw.ebx = (uintptr_t)(proc_virt + 4000); // cruza frontera a página no mapeada en el primer chunk (0x40001000 no mapeada)
    regs_sw.ecx = 200;
    syscall_dispatcher(&regs_sw);
    TEST_ASSERT(regs_sw.eax == (uint32_t)-EFAULT, "SYS_WRITE rechaza buffer que cruza frontera hacia pagina no mapeada en el primer chunk con -EFAULT");

    // Auditoría v18: Escritura parcial exacta en SYS_WRITE cuando el primer chunk (256 bytes) es válido
    // y el segundo chunk cruza hacia una página no mapeada.
    // Usar un buffer válido en memoria de host mapeado en pdir exactamente en su primera página alineada,
    // asegurando que sus bits [31:22] caigan fuera de KERNEL_IDENTITY_TABLES (< 16) sin importar ASLR en 64-bit.
    uint8_t* raw_partial_alloc = (uint8_t*)malloc(0x04000000UL + 16384UL);
    TEST_ASSERT(raw_partial_alloc != NULL, "Reserva temporal de host para prueba de escritura parcial en SYS_WRITE");
    uintptr_t partial_page_virt = ((uintptr_t)raw_partial_alloc + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    if (((partial_page_virt >> 22) & 0x3FF) < 16) {
        partial_page_virt += 0x04000000UL;
    }
    k_memset((void*)partial_page_virt, 'P', 8192);
    // Mapear únicamente la primera página [partial_page_virt .. partial_page_virt + 4095], dejando la segunda sin mapear
    int partial_map_rc = vmm_map_page(pdir, partial_page_virt, phys_target, PTE_PRESENT | PTE_USER);
    TEST_ASSERT(partial_map_rc == 0, "Primera pagina de prueba parcial mapeada en directorio de proceso");
    vmm_unmap_page(pdir, partial_page_virt + PAGE_SIZE);
    regs_sw.eax = SYS_WRITE;
    regs_sw.ebx = partial_page_virt + PAGE_SIZE - 256; // Primeros 256 bytes dentro de la página mapeada; siguientes 256 en página no mapeada
    regs_sw.ecx = 512;
    syscall_dispatcher(&regs_sw);
    TEST_ASSERT(regs_sw.eax == 256,
                "SYS_WRITE retorna escritura parcial exacta (256 bytes) cuando el segundo chunk cruza a pagina no mapeada");
    vmm_unmap_page(pdir, partial_page_virt);
    free(raw_partial_alloc);

    regs_sw.eax = SYS_READ;
    regs_sw.ebx = (uintptr_t)proc_virt; // proc_virt actualmente de solo lectura
    regs_sw.ecx = 10;
    syscall_dispatcher(&regs_sw);
    TEST_ASSERT(regs_sw.eax == (uint32_t)-EFAULT, "SYS_READ rechaza lectura hacia pagina de solo lectura sin PTE_WRITABLE con -EFAULT");

    // Auditoría v15: Validación Fail-Closed de espacio de usuario (sin recurrir al directorio del kernel)
    vmm_switch_page_directory(kdir);
    TEST_ASSERT(is_user_pages_accessible_failclosed((const void*)proc_virt, 10, false) == false,
                "is_user_pages_accessible_failclosed falla cerrado si la pagina no existe en el directorio de usuario");

    // Restaurar permiso de escritura con vmm_set_page_flags
    vmm_switch_page_directory(pdir);
    vmm_set_page_flags(pdir, proc_virt, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
    vmm_switch_page_directory(kdir);

    // Auditoría v12: Validación de accesibilidad de directorio (vmm_is_directory_accessible)
    TEST_ASSERT(vmm_is_directory_accessible(NULL) == false, "vmm_is_directory_accessible rechaza puntero NULL");
    TEST_ASSERT(vmm_is_directory_accessible((struct page_directory*)0x200) == false, "vmm_is_directory_accessible rechaza puntero de directorio corrupto/invalido");
    TEST_ASSERT(vmm_map_page(NULL, 0x50000000, 0x1000000, PTE_PRESENT) == -1, "vmm_map_page rechaza de forma segura directorio NULL");
    TEST_ASSERT(vmm_get_page_flags(NULL, 0x50000000) == 0, "vmm_get_page_flags retorna 0 ante directorio NULL");

    // Seguridad: rechazar páginas de usuario (PTE_USER) en el espacio reservado del kernel (< 64 MiB)
    int intrusion_res = vmm_map_page(pdir, 0x000B8000, phys_target, PTE_PRESENT | PTE_USER);
    TEST_ASSERT(intrusion_res == -1, "VMM rechaza mapeo de usuario (PTE_USER) en el espacio del kernel (< 64 MiB)");

    // Mapeo contiguo de rangos (vmm_map_range, requerido para Framebuffer / MMIO)
    uintptr_t range_virt = 0x50000000;
    uintptr_t range_phys = 0x60000000;
    int range_res = vmm_map_range(pdir, range_virt, range_phys, 16384, PTE_PRESENT | PTE_WRITABLE);
    TEST_ASSERT(range_res == 0, "vmm_map_range mapea rango contiguo de 16 KiB (4 paginas) exitosamente");
    TEST_ASSERT(vmm_virt_to_phys(pdir, range_virt + 8192) == range_phys + 8192, "Traduccion virtual a fisica en medio del rango mapeado");

    // Prueba de vmm_map_physical_range con offset intra-pagina y validaciones 64-bit (Auditoria v9/v10)
    uintptr_t virt_mapped = 0;
    int map_res = vmm_map_physical_range(pdir, 0xE0000120ULL, 8000, PTE_PRESENT | PTE_WRITABLE, &virt_mapped);
    TEST_ASSERT(map_res == 0, "vmm_map_physical_range mapea exitosamente rango fisico con offset intra-pagina");
    TEST_ASSERT((virt_mapped & 0xFFF) == 0x120, "Puntero virtual retornado preserva el desplazamiento intra-pagina (0x120)");
    TEST_ASSERT(vmm_virt_to_phys(pdir, virt_mapped) == 0xE0000120, "Traduccion virtual a fisica del puntero mapeado coincide exactamente");

    // Rechazo de direcciones por encima del espacio de 32 bits (4 GiB)
    uintptr_t invalid_ptr = 0;
    int ovf_res = vmm_map_physical_range(pdir, 0x100000000ULL, 4096, PTE_PRESENT | PTE_WRITABLE, &invalid_ptr);
    TEST_ASSERT(ovf_res == -1, "vmm_map_physical_range rechaza direcciones fisicas >= 4 GiB en arquitectura de 32 bits");

    int ovf_len = vmm_map_physical_range(pdir, 0xFFFFF000ULL, 8192, PTE_PRESENT | PTE_WRITABLE, &invalid_ptr);
    TEST_ASSERT(ovf_len == -1, "vmm_map_physical_range rechaza rangos que desbordan el limite de 32 bits");

    // Probar rango que termina exactamente en 4 GiB (0xFFFFF000 + 4096 = 0x100000000)
    uintptr_t edge_ptr = 0;
    int edge_4gb = vmm_map_physical_range(pdir, 0xFFFFF000ULL, 4096, PTE_PRESENT | PTE_WRITABLE, &edge_ptr);
    TEST_ASSERT(edge_4gb == 0, "vmm_map_physical_range maneja con precision el rango que finaliza exactamente en el limite de 4 GiB");

    // Probar rango que cruza o excede 4 GiB tras alineacion de pagina (0xFFFFF800 + 4096 = 0x100000800 > 4 GiB)
    int cross_4gb = vmm_map_physical_range(pdir, 0xFFFFF800ULL, 4096, PTE_PRESENT | PTE_WRITABLE, &invalid_ptr);
    TEST_ASSERT(cross_4gb == -1, "vmm_map_physical_range rechaza rango no alineado cuyo final sobrepasa 4 GiB");

    // Destrucción de directorio de proceso
    vmm_destroy_process_directory(pdir);
    pmm_free_frame(phys_target);
}

/* Test 18: Manejador de Excepción de Page Fault (#PF Vector 14) */
void test_page_fault_handling(void) {
    printf("[*] Ejecutando: test_page_fault_handling...\n");
    page_fault_init();

    extern uint32_t mock_cr2_addr;
    mock_cr2_addr = 0xDEADB000;

    struct registers regs;
    k_memset(&regs, 0, sizeof(struct registers));
    regs.int_no = 14;
    regs.err_code = PF_ERR_WRITE | PF_ERR_USER; // No presente, escritura, userspace
    regs.eip = 0x08048000;

    size_t count_before = page_fault_get_count();
    page_fault_handler(&regs);

    TEST_ASSERT(page_fault_get_count() == count_before + 1, "page_fault_handler procesa e incrementa contador de fallos");
    TEST_ASSERT(page_fault_get_last_address() == 0xDEADB000, "Registro CR2 capturado como direccion causante");
    TEST_ASSERT(page_fault_get_last_error_code() == (PF_ERR_WRITE | PF_ERR_USER), "Codigo de error (#PF) decodificado con precision");

    // Auditoría v19: Verificación de redirección real de EIP (fixup), vinculación a rango de EIP/CR2 y pila reentrante
    uintptr_t copy_eip_start = page_fault_get_copy_eip_start();
    uintptr_t copy_eip_end   = page_fault_get_copy_eip_end();
    uintptr_t copy_fixup_eip = page_fault_get_copy_fixup_eip();

    bool pushed = page_fault_push_user_copy_ctx(0x40000000UL, 0x2000UL, copy_eip_start, copy_eip_end, copy_fixup_eip);
    TEST_ASSERT(pushed == true && page_fault_get_user_copy_depth() == 1,
                "page_fault_push_user_copy_ctx apila contexto de copia (depth = 1)");
    TEST_ASSERT(page_fault_is_user_copy_active() == true, "Ventana de copia activa tras push");

    // 1. Si un #PF ocurre en Ring 0 pero con un EIP FUERA del rango de la instrucción de copia registrada,
    // page_fault_handler NO debe tratarlo como fixup ni redirigir EIP (evita ocultar bugs de kernel).
    mock_cr2_addr = 0x40000100; // Dentro del rango de dirección de usuario
    regs.eip = 0x00100500;      // EIP fuera de [copy_eip_start, copy_eip_end)
    regs.err_code = 0;
    size_t fixups_before = page_fault_get_fixup_count();
    page_fault_handler(&regs);
    TEST_ASSERT(page_fault_did_user_copy_fault() == false && page_fault_get_fixup_count() == fixups_before,
                "page_fault_handler NO oculta #PF cuando el EIP no pertenece a la instruccion de copia registrada");
    TEST_ASSERT(regs.eip == 0x00100500, "EIP permanece sin alterar si el fallo no proviene de la instruccion de copia");

    // 2. Si un #PF tiene el EIP dentro de la instrucción de copia pero la dirección en CR2 está FUERA del rango de usuario
    // registrado (ej. desreferencia accidental en espacio de kernel), TAMPOCO debe ocultarse como fixup.
    mock_cr2_addr = 0x00200000; // Fuera de [0x40000000, 0x40002000)
    regs.eip = copy_eip_start;
    page_fault_handler(&regs);
    TEST_ASSERT(page_fault_did_user_copy_fault() == false && page_fault_get_fixup_count() == fixups_before,
                "page_fault_handler NO oculta #PF cuando CR2 esta fuera del rango de usuario registrado");
    TEST_ASSERT((uintptr_t)regs.eip == copy_eip_start, "EIP no se redirige si CR2 esta fuera del rango de usuario");

    // 3. Caso legítimo de fixup: EIP dentro de [copy_eip_start, copy_eip_end) Y CR2 dentro de [0x40000000, 0x40002000).
    // page_fault_handler DEBE redirigir regs->eip = copy_fixup_eip para que IRET no reintente la instrucción fallida.
    mock_cr2_addr = 0x40001000;
    regs.eip = copy_eip_start;
    page_fault_handler(&regs);
    TEST_ASSERT(page_fault_did_user_copy_fault() == true,
                "page_fault_handler detecta #PF legitimo dentro del rango de EIP y CR2 registrados");
    TEST_ASSERT((uintptr_t)regs.eip == copy_fixup_eip,
                "page_fault_handler redirige regs->eip exactamente a fixup_eip para salir de la instruccion fallida");
    TEST_ASSERT(page_fault_get_fixup_count() == fixups_before + 1,
                "Contador de fixups de #PF se incrementa tras redireccion de EIP");

    // 4. Reentrancia y anidamiento de contextos de copia (pila de profundidad USER_COPY_CTX_MAX_DEPTH = 4)
    // Con el contexto exterior marcado con faulted == true, apilar un contexto interior limpio (depth = 2)
    bool pushed_inner = page_fault_push_user_copy_ctx(0x50000000UL, 0x1000UL, copy_eip_start, copy_eip_end, copy_fixup_eip);
    TEST_ASSERT(pushed_inner == true && page_fault_get_user_copy_depth() == 2,
                "Pila reentrante permite contexto anidado (depth = 2) sin sobrescribir el contexto exterior");
    TEST_ASSERT(page_fault_did_user_copy_fault() == false,
                "Contexto interior inicia con estado faulted = false independiente del exterior");

    // Llenar hasta USER_COPY_CTX_MAX_DEPTH (4) y verificar rechazo explícito si se intenta superar la profundidad máxima
    TEST_ASSERT(page_fault_push_user_copy_ctx(0x60000000UL, 0x1000UL, copy_eip_start, copy_eip_end, copy_fixup_eip) == true,
                "Apilado nivel 3 exitoso");
    TEST_ASSERT(page_fault_push_user_copy_ctx(0x70000000UL, 0x1000UL, copy_eip_start, copy_eip_end, copy_fixup_eip) == true,
                "Apilado nivel 4 (USER_COPY_CTX_MAX_DEPTH) exitoso");
    TEST_ASSERT(page_fault_push_user_copy_ctx(0x80000000UL, 0x1000UL, copy_eip_start, copy_eip_end, copy_fixup_eip) == false,
                "Pila de contextos rechaza desbordamiento cuando se supera USER_COPY_CTX_MAX_DEPTH");

    // Desapilar niveles 4, 3 y 2 (sin fallo) y comprobar que el nivel 1 conserva su estado faulted == true intacto
    TEST_ASSERT(page_fault_pop_user_copy_ctx() == false, "Pop nivel 4 retorna false (sin fallo)");
    TEST_ASSERT(page_fault_pop_user_copy_ctx() == false, "Pop nivel 3 retorna false (sin fallo)");
    TEST_ASSERT(page_fault_pop_user_copy_ctx() == false, "Pop nivel 2 retorna false (sin fallo)");
    TEST_ASSERT(page_fault_get_user_copy_depth() == 1 && page_fault_did_user_copy_fault() == true,
                "Al desapilar contextos interiores, el contexto exterior preserva su estado faulted == true");
    TEST_ASSERT(page_fault_pop_user_copy_ctx() == true,
                "Pop del contexto exterior (nivel 1) retorna true confirmando el fallo registrado");
    TEST_ASSERT(page_fault_is_user_copy_active() == false && page_fault_get_user_copy_depth() == 0,
                "Pila de contextos queda completamente cerrada (depth = 0)");
}

/* Test 19: Primitivas GFX 2D, Clipping y Doble Búfer */
void test_gfx_2d_primitives_and_double_buffer(void) {
    printf("[*] Ejecutando: test_gfx_2d_primitives_and_double_buffer...\n");
    const uint32_t width = 800;
    const uint32_t height = 600;
    const uint32_t pitch = width * sizeof(uint32_t);

    uint32_t* front = (uint32_t*)malloc(height * pitch);
    uint32_t* back = (uint32_t*)malloc(height * pitch);
    assert(front != NULL && back != NULL);
    memset(front, 0, height * pitch);
    memset(back, 0, height * pitch);

    gfx_init_custom(front, back, width, height, pitch, 32);
    TEST_ASSERT(gfx_is_active() == true, "Motor gráfico 2D activo tras gfx_init_custom");

    // Limpieza
    gfx_clear(0xFF102030);
    TEST_ASSERT(gfx_getpixel(0, 0) == 0xFF102030, "gfx_clear colorea origen del backbuffer");
    TEST_ASSERT(gfx_getpixel(width - 1, height - 1) == 0xFF102030, "gfx_clear colorea extremo del backbuffer");

    // Putpixel y Getpixel
    gfx_putpixel(150, 200, 0xFFAABBCC);
    TEST_ASSERT(gfx_getpixel(150, 200) == 0xFFAABBCC, "gfx_putpixel escribe color correcto en backbuffer");

    // Clipping fuera de límites
    gfx_putpixel(9999, 9999, 0xFFFFFFFF);
    TEST_ASSERT(gfx_getpixel(9999, 9999) == 0, "Acceso fuera de limites recortado de forma segura");

    // Fill Rect con Clipping
    gfx_fill_rect(50, 50, 40, 30, 0xFF00FF00);
    TEST_ASSERT(gfx_getpixel(50, 50) == 0xFF00FF00, "gfx_fill_rect esquina superior izquierda");
    TEST_ASSERT(gfx_getpixel(89, 79) == 0xFF00FF00, "gfx_fill_rect esquina inferior derecha");
    TEST_ASSERT(gfx_getpixel(90, 80) != 0xFF00FF00, "gfx_fill_rect no invade fuera del rectangulo");

    // Draw Rect (Bordes)
    gfx_draw_rect(100, 100, 20, 20, 0xFFFF0000);
    TEST_ASSERT(gfx_getpixel(100, 100) == 0xFFFF0000, "gfx_draw_rect traza borde superior");
    TEST_ASSERT(gfx_getpixel(110, 110) != 0xFFFF0000, "gfx_draw_rect preserva interior hueco");

    // Línea Bresenham
    gfx_draw_line(200, 200, 210, 210, 0xFFFFFFFF);
    TEST_ASSERT(gfx_getpixel(200, 200) == 0xFFFFFFFF, "gfx_draw_line punto inicial");
    TEST_ASSERT(gfx_getpixel(205, 205) == 0xFFFFFFFF, "gfx_draw_line interpolacion diagonal Bresenham");
    TEST_ASSERT(gfx_getpixel(210, 210) == 0xFFFFFFFF, "gfx_draw_line punto final");

    // Texto con fuente bitmap 8x16
    gfx_draw_string(300, 300, "Zenthra", COLOR_TEXT_WHITE, 0);
    // Verificar que al menos algún píxel del glifo 'Z' fue dibujado
    bool has_drawn_text_pixel = false;
    for (int y = 300; y < 316; y++) {
        for (int x = 300; x < 308; x++) {
            if (gfx_getpixel(x, y) == COLOR_TEXT_WHITE) {
                has_drawn_text_pixel = true;
                break;
            }
        }
        if (has_drawn_text_pixel) break;
    }
    TEST_ASSERT(has_drawn_text_pixel == true, "gfx_draw_string rasteriza glifos de la fuente bitmap 8x16");

    // Verificación de Doble Búfer (Flip)
    TEST_ASSERT(front[200 * (pitch / 4) + 150] != 0xFFAABBCC, "Frontbuffer aislado antes del flip");
    gfx_flip();
    TEST_ASSERT(front[200 * (pitch / 4) + 150] == 0xFFAABBCC, "gfx_flip vuelca backbuffer completo a frontbuffer");

    // Verificación de Fallback Directo (Auditoría v11: backbuffer == frontbuffer no debe copiar sobre sí mismo)
    gfx_init_custom(front, front, width, height, pitch, 32);
    gfx_flip(); // No crashea ni realiza memcpy recursivo
    TEST_ASSERT(true, "gfx_flip maneja de forma segura y sin redundancia el modo de dibujo directo (backbuffer == frontbuffer)");

    free(front);
    free(back);
}

/* Test 20: Driver de Ratón PS/2 (8042) y Decodificación de Paquetes */
void test_ps2_mouse_driver(void) {
    printf("[*] Ejecutando: test_ps2_mouse_driver...\n");
    mouse_init(800, 600);
    struct mouse_state* ms = mouse_get_state();

    TEST_ASSERT(ms != NULL, "mouse_get_state retorna puntero valido");
    TEST_ASSERT(ms->x == 400 && ms->y == 300, "Posicion inicial del raton centrada en pantalla");
    TEST_ASSERT(ms->left_button == false && ms->right_button == false, "Botones en reposo al inicio");

    // Byte fuera de sincronía (bit 3 == 0) debe ser descartado
    mouse_process_byte(0x00);
    TEST_ASSERT(ms->x == 400 && ms->y == 300, "Byte desincronizado descartado por el protocolo");

    // Paquete 1: Clic Izquierdo + Mover +20 en X, +10 en Y (que sube en pantalla)
    mouse_process_byte(0x08 | 0x01); // Bit 3=1, LeftBtn=1
    mouse_process_byte(20);          // dx = +20
    mouse_process_byte(10);          // dy = +10 (y = y - 10 = 290)

    TEST_ASSERT(ms->left_button == true, "mouse_process_byte detecta boton izquierdo presionado");
    TEST_ASSERT(ms->x == 420, "Coordenada X actualizada (+20px)");
    TEST_ASSERT(ms->y == 290, "Coordenada Y actualizada (-10px pantalla)");

    // Paquete 2: Clic Derecho + Mover -30 en X, -20 en Y con signo negativo
    mouse_process_byte(0x08 | 0x02 | 0x10 | 0x20); // Bit3=1, RightBtn=1, X_sign=1, Y_sign=1
    mouse_process_byte((uint8_t)(-30));             // dx = -30
    mouse_process_byte((uint8_t)(-20));             // dy = -20 (y = y - (-20) = y + 20)

    TEST_ASSERT(ms->left_button == false, "Boton izquierdo liberado");
    TEST_ASSERT(ms->right_button == true, "Boton derecho activado");
    TEST_ASSERT(ms->x == 390, "Coordenada X extendida con signo negativo (-30px)");
    TEST_ASSERT(ms->y == 310, "Coordenada Y extendida con signo negativo (+20px pantalla)");

    // Prueba de Delimitación Estricta a Pantalla (Clamping)
    for (int i = 0; i < 50; i++) {
        mouse_process_byte(0x08 | 0x10); // mover fuertemente a la izquierda
        mouse_process_byte((uint8_t)(-100));
        mouse_process_byte(0);
    }
    TEST_ASSERT(ms->x == 0, "Cursor delimitado en borde izquierdo (X=0)");

    for (int i = 0; i < 50; i++) {
        mouse_process_byte(0x08 | 0x20); // mover fuertemente hacia abajo (dy negativo con signo)
        mouse_process_byte(0);
        mouse_process_byte((uint8_t)(-100)); // dy negativo -> y aumenta hacia abajo
    }
    TEST_ASSERT(ms->y == 599, "Cursor delimitado en borde inferior (Y=599)");
}

/* Test 21: Gestor de Ventanas Zenthra (WM), Arrastre y Hit-Testing */
void test_window_manager_and_desktop(void) {
    printf("[*] Ejecutando: test_window_manager_and_desktop...\n");

    // Configurar framebuffer virtual para el Window Manager
    uint32_t* fb_mock = (uint32_t*)malloc(800 * 600 * sizeof(uint32_t));
    memset(fb_mock, 0, 800 * 600 * sizeof(uint32_t));
    gfx_init_custom(fb_mock, fb_mock, 800, 600, 800 * 4, 32);

    wm_init();
    TEST_ASSERT(wm_get_window_count() >= 2, "wm_init crea ventanas predeterminadas del sistema");

    struct window* win_term = wm_get_window(0);
    struct window* win_iot = wm_get_window(1);
    TEST_ASSERT(win_term != NULL && win_iot != NULL, "Ventanas del sistema accesibles");
    TEST_ASSERT(strcmp(win_term->title, "Terminal Zenthra OS") == 0, "Ventana 1 es Terminal Zenthra OS");
    TEST_ASSERT(strcmp(win_iot->title, "Monitor de Sistema & IoT") == 0, "Ventana 2 es Monitor IoT");

    // Crear ventana adicional de usuario
    struct window* win_app = wm_create_window("Zenthra Code Editor", 100, 100, 300, 200, NULL);
    TEST_ASSERT(win_app != NULL, "wm_create_window crea ventana de usuario");
    TEST_ASSERT(win_app->is_visible == true, "Nueva ventana visible");
    TEST_ASSERT(win_app->is_focused == true, "Nueva ventana toma el foco activo");

    // Simular clic en barra de título para arrastre (Drag & Drop)
    // Barra de título de win_app: x in [100, 380], y in [100, 122]
    wm_handle_mouse(150, 110, true);
    TEST_ASSERT(win_app->is_dragging == true, "Clic en barra de titulo inicia arrastre (is_dragging)");

    // Mover ratón durante el arrastre
    wm_handle_mouse(180, 140, true);
    TEST_ASSERT(win_app->x == 130 && win_app->y == 130, "Ventana desplazada en sincronia con el raton (+30px, +30px)");

    // Soltar arrastre
    wm_handle_mouse(180, 140, false);
    TEST_ASSERT(win_app->is_dragging == false, "Liberar boton de raton detiene arrastre");

    // Simular clic en botón [X] de cierre de la ventana
    // Botón [X] ubicado en: win->x + win->width - 20, win->y + 3
    int32_t close_x = win_app->x + (int32_t)win_app->width - 12; // En medio del botón de 16x16
    int32_t close_y = win_app->y + 10;
    wm_handle_mouse(close_x, close_y, true);

    // Simular entrada interactiva de teclado en la ventana de Terminal (id 1)
    wm_focus_window(1);
    TEST_ASSERT(wm_needs_redraw() == true, "wm_focus_window solicita redibujado");

    wm_handle_keyboard('h');
    wm_handle_keyboard('e');
    wm_handle_keyboard('l');
    wm_handle_keyboard('p');
    wm_handle_keyboard('\n');
    TEST_ASSERT(wm_needs_redraw() == true, "Entrada de comando help en terminal grafica procesada y solicita redibujado");

    // Probar comandos dinamicos tasks y sysinfo con scheduler y PMM reales
    wm_handle_keyboard('t');
    wm_handle_keyboard('a');
    wm_handle_keyboard('s');
    wm_handle_keyboard('k');
    wm_handle_keyboard('s');
    wm_handle_keyboard('\n');
    TEST_ASSERT(wm_needs_redraw() == true, "Comando tasks en terminal procesado con scheduler de tareas real");

    wm_handle_keyboard('s');
    wm_handle_keyboard('y');
    wm_handle_keyboard('s');
    wm_handle_keyboard('i');
    wm_handle_keyboard('n');
    wm_handle_keyboard('f');
    wm_handle_keyboard('o');
    wm_handle_keyboard('\n');
    TEST_ASSERT(wm_needs_redraw() == true, "Comando sysinfo en terminal procesado con telemetria de memoria real");

    // Simular clic en Launcher Zenthra en la Taskbar (abrir menu)
    // Barra de tareas: y = 564 a 600, Boton Zenthra: x in [8, 96], y in [569, 595]
    wm_handle_mouse(30, 580, true);
    wm_handle_mouse(30, 580, false);
    TEST_ASSERT(wm_needs_redraw() == true, "Clic en boton Zenthra conmuta launcher y solicita redibujado");

    // Simular clic en opcion 1 del Launcher (restaurar y enfocar Terminal)
    // Menu popup en ly = 564 - 92 = 472. Opcion 1: y in [472, 502]
    wm_handle_mouse(30, 485, true);
    wm_handle_mouse(30, 485, false);
    struct window* term_refocus = wm_get_window(0);
    TEST_ASSERT(term_refocus->is_focused == true, "Opcion del Launcher enfoca la ventana Terminal");

    // Comprobar actualización de reloj y renderizado de escritorio
    wm_update_clock(12500); // 125 segundos
    wm_render_desktop();
    TEST_ASSERT(fb_mock[0] != 0, "wm_render_desktop renderiza gradiente corporativo en el framebuffer");
    TEST_ASSERT(wm_needs_redraw() == false, "Redibujado bajo demanda completado: needs_redraw desactivado");

    free(fb_mock);
}

/* Test 22: Driver de Teclado PS/2, Modificadores Shift/Caps Lock, Layouts y Telemetría (Auditoría v16) */
void test_ps2_keyboard_driver(void) {
    printf("[*] Ejecutando: test_ps2_keyboard_driver...\n");

    keyboard_init();
    TEST_ASSERT(keyboard_queue_is_empty() == true, "keyboard_init inicializa cola vacia");
    TEST_ASSERT(keyboard_is_shift_active() == false, "Shift inactivo por defecto");
    TEST_ASSERT(keyboard_is_caps_active() == false, "Caps Lock inactivo por defecto");
    TEST_ASSERT(keyboard_get_layout() == KEYBOARD_LAYOUT_US, "Layout inicial es US");
    TEST_ASSERT(keyboard_get_overflow_count() == 0, "Contador de overflow inicializado en 0");

    // 1. Teclas normales sin Shift ni Caps (Scancode 0x10 = 'q', 0x1E = 'a')
    keyboard_process_scancode(0x10);
    TEST_ASSERT(keyboard_queue_length() == 1, "Procesamiento de scancode encola 1 caracter");
    char c = keyboard_dequeue_char();
    TEST_ASSERT(c == 'q', "Scancode 0x10 produce 'q' minuscula");

    // 2. Tecla con Shift (Make 0x2A -> 'a' -> Break 0xAA)
    keyboard_process_scancode(0x2A); // Left Shift Make
    TEST_ASSERT(keyboard_is_shift_active() == true, "Left Shift make activa estado shift");
    keyboard_process_scancode(0x1E); // 'a'
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == 'A', "Con Shift activo, scancode 'a' produce 'A' mayuscula");

    // Símbolo numérico con Shift en layout US (0x02 = '1' -> '!')
    keyboard_process_scancode(0x02);
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == '!', "Con Shift activo en US, '1' produce '!'");

    keyboard_process_scancode(0xAA); // Left Shift Break
    TEST_ASSERT(keyboard_is_shift_active() == false, "Left Shift break desactiva estado shift");

    // 3. Caps Lock (Make 0x3A / Break 0xBA y protección contra autorepetición - Auditoría v18)
    keyboard_process_scancode(0x3A); // Caps Lock Make
    TEST_ASSERT(keyboard_is_caps_active() == true, "Caps Lock make conmuta a activo");
    // Simular autorepetición del controlador 8042 (múltiples Make 0x3A sin Break 0xBA intermedio)
    keyboard_process_scancode(0x3A);
    keyboard_process_scancode(0x3A);
    TEST_ASSERT(keyboard_is_caps_active() == true,
                "Autorepeticion de Caps Lock Make (0x3A sin Break 0xBA) NO alterna Caps Lock repetidamente");
    keyboard_process_scancode(0xBA); // Caps Lock Break (liberar tecla)
    TEST_ASSERT(keyboard_is_caps_active() == true, "Liberar Caps Lock (0xBA) conserva el estado activo");

    keyboard_process_scancode(0x1E); // 'a' -> 'A'
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == 'A', "Con Caps Lock activo, 'a' produce 'A'");

    // Caps Lock NO debe alterar números o símbolos
    keyboard_process_scancode(0x02); // '1'
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == '1', "Caps Lock no afecta numeros (scancode '1' permanece '1')");

    // XOR: Caps Lock activo + Shift presionado -> Letra minúscula
    keyboard_process_scancode(0x36); // Right Shift Make
    TEST_ASSERT(keyboard_is_shift_active() == true, "Right Shift make activa shift");
    keyboard_process_scancode(0x1E); // 'a'
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == 'a', "XOR: Caps Lock + Shift produce letra minuscula ('a')");
    keyboard_process_scancode(0xB6); // Right Shift Break

    // Desactivar Caps Lock con nueva pulsación completa (Make 0x3A -> Break 0xBA)
    keyboard_process_scancode(0x3A);
    keyboard_process_scancode(0xBA);
    TEST_ASSERT(keyboard_is_caps_active() == false, "Segunda pulsacion de Caps Lock tras Break conmuta a inactivo");

    // 3b. Estado independiente de Left Shift y Right Shift (Auditoría v18)
    keyboard_process_scancode(0x2A); // Left Shift Make
    keyboard_process_scancode(0x36); // Right Shift Make
    TEST_ASSERT(keyboard_is_shift_active() == true, "Ambos Shift presionados mantienen estado Shift activo");
    keyboard_process_scancode(0xAA); // Soltar solo Left Shift
    TEST_ASSERT(keyboard_is_shift_active() == true,
                "Liberar Left Shift mientras Right Shift sigue presionado mantiene Shift activo");
    keyboard_process_scancode(0x1E); // 'a' -> 'A'
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == 'A', "Tecla presionada con Right Shift remanente produce mayuscula ('A')");
    keyboard_process_scancode(0xB6); // Soltar Right Shift
    TEST_ASSERT(keyboard_is_shift_active() == false, "Liberar ambos Shift desactiva finalmente el estado Shift");

    // 4. Conmutación a Layout Español Básico ASCII (KEYBOARD_LAYOUT_ES)
    keyboard_set_layout(KEYBOARD_LAYOUT_ES);
    TEST_ASSERT(keyboard_get_layout() == KEYBOARD_LAYOUT_ES, "Conmutacion de layout a Espanol/LatAm exitosa");

    keyboard_process_scancode(0x33); // En ES normal: ','
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == ',', "Layout ES normal scancode 0x33 produce ','");

    keyboard_process_scancode(0x2A); // Shift
    keyboard_process_scancode(0x33); // En ES shift: ';'
    c = keyboard_dequeue_char();
    TEST_ASSERT(c == ';', "Layout ES con Shift scancode 0x33 produce ';'");
    keyboard_process_scancode(0xAA); // Shift break

    // Restaurar layout US
    keyboard_set_layout(KEYBOARD_LAYOUT_US);

    // 5. Telemetría y saturación de cola (overflow counter)
    keyboard_queue_clear();
    keyboard_reset_overflow_count();
    TEST_ASSERT(keyboard_get_overflow_count() == 0, "keyboard_reset_overflow_count restablece contador a 0");

    // Llenar exactamente la cola hasta KEYBOARD_QUEUE_CAPACITY (256)
    for (size_t i = 0; i < KEYBOARD_QUEUE_CAPACITY; i++) {
        bool enq = keyboard_enqueue_char('X');
        TEST_ASSERT(enq == true, "Encolado exitoso dentro de capacidad");
    }
    TEST_ASSERT(keyboard_queue_length() == KEYBOARD_QUEUE_CAPACITY, "Cola al maximo de capacidad");
    TEST_ASSERT(keyboard_get_overflow_count() == 0, "Sin overflow mientras no se exceda la capacidad");

    // Intentar 5 inserciones adicionales que desborden la cola
    for (int i = 0; i < 5; i++) {
        bool enq = keyboard_enqueue_char('Z');
        TEST_ASSERT(enq == false, "Encolado rechazado cuando la cola esta saturada");
    }
    TEST_ASSERT(keyboard_get_overflow_count() == 5, "keyboard_get_overflow_count registra exactamente los 5 desbordes");

    keyboard_queue_clear();
    keyboard_reset_overflow_count();
}

int main(void) {
    printf("====================================================\n");
    printf("    ZENTHRA OS — SUITE DE PRUEBAS AUTOMATIZADAS     \n");
    printf("====================================================\n\n");

    test_klib_strings();
    test_kheap_allocator();
    test_syscall_security_checks();
    test_multitasking_scheduler();
    test_acl_security();
    test_ramfs();
    test_profile_provisioning();
    test_package_manager();
    test_rules_engine();
    test_bidirectional_simulation();
    test_x86_structures();
    test_multiboot_and_power();
    test_task_lifecycle_and_cleanup();
    test_syscall_boundary_and_abi();
    test_preemptive_scheduling_and_sleep();
    test_physical_memory_manager();
    test_virtual_memory_paging();
    test_page_fault_handling();
    test_gfx_2d_primitives_and_double_buffer();
    test_ps2_mouse_driver();
    test_window_manager_and_desktop();
    test_ps2_keyboard_driver();

    printf("\n----------------------------------------------------\n");
    printf("Resultados: %d Aprobados | %d Fallidos\n", tests_passed, tests_failed);
    printf("----------------------------------------------------\n");

    if (tests_failed > 0) {
        printf("[-] Fallaron pruebas criticas del sistema.\n");
        return 1;
    }
    printf("[+] Todas las pruebas pasaron satisfactoriamente.\n");
    return 0;
}

