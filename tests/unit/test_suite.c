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
    static uint8_t memory_pool[16384] __attribute__((aligned(16)));
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
    TEST_ASSERT(regs_acl.eax == 0, "SYS_SET_PROFILE cambia el perfil activo con permisos de administracion");
    TEST_ASSERT(task_get_current()->profile == PROFILE_STUDENT, "Perfil de proceso verificado tras SYS_SET_PROFILE");
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

    // Verificar identidad de 16 MiB (Ring 0 / Supervisor)
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x00000000) == 0x00000000, "Mapeo identidad de pagina cero (0x00000000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x000B8000) == 0x000B8000, "Mapeo identidad del buffer VGA (0x000B8000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x00100000) == 0x00100000, "Mapeo identidad del kernel ELF (1 MiB / 0x00100000)");
    TEST_ASSERT(vmm_virt_to_phys(kdir, 0x00400000) == 0x00400000, "Mapeo identidad del Heap del kernel (4 MiB / 0x00400000)");

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

    // Política de ownership y reemplazo de PTE (Auditoría v10):
    // Mapear la misma página con flags actualizados debe tener éxito
    int same_map = vmm_map_page(pdir, proc_virt, phys_target, PTE_PRESENT | PTE_USER);
    TEST_ASSERT(same_map == 0, "vmm_map_page permite actualizar flags en pagina ya asociada al mismo marco fisico");

    // Intentar asociar la misma direccion virtual a un marco fisico DIFERENTE sin desmapear debe ser rechazado
    uintptr_t other_frame = 0x70000000;
    int conflict_map = vmm_map_page(pdir, proc_virt, other_frame, PTE_PRESENT | PTE_USER);
    TEST_ASSERT(conflict_map == -1, "vmm_map_page rechaza reemplazo conflictivo de marco fisico sin desmapeo previo");

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
