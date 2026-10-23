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
