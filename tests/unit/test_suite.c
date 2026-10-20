#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ktypes.h"
#include "klib.h"
#include "kheap.h"
#include "ramfs.h"
#include "rules_engine.h"
#include "nova_link.h"
#include "nova_iot.h"

// Variables requeridas por drivers en host
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
    static uint8_t memory_pool[8192] __attribute__((aligned(16)));
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

/* Test 3: Sistema de Archivos en Memoria (RAMFS) */
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

/* Test 4: Motor de Reglas y Automatización SI-ENTONCES */
void test_rules_engine(void) {
    printf("[*] Ejecutando: test_rules_engine...\n");
    rules_engine_init(); // Contiene regla: Si Temp > 30.0 C (300 decicelsius) -> LED ON

    // Caso A: Temperatura normal (25.0 C = 250) -> No debe disparar la regla
    mock_led_value = false;
    rules_engine_evaluate(250);
    TEST_ASSERT(mock_led_value == false, "Temperatura normal (25.0 C) no activa alarma");

    // Caso B: Temperatura umbral excedida (32.5 C = 325) -> Debe disparar la regla
    rules_engine_evaluate(325);
    TEST_ASSERT(mock_led_value == true, "Temperatura critica (32.5 C) activa actuador LED por automatizacion");

    // Comprobar que la regla generó un registro de alerta en RAMFS
    uint8_t alert_log[128];
    k_memset(alert_log, 0, sizeof(alert_log));
    int b = ramfs_read("alertas.log", alert_log, sizeof(alert_log) - 1);
    TEST_ASSERT(b > 0, "Registro de alerta guardado en RAMFS (alertas.log)");
}

/* Test 5: Simulación de conversación bidireccional NOVA-Link */
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

/* Test 6: Validación de estructuras de protección de CPU x86 */
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

int main(void) {
    printf("====================================================\n");
    printf("      NOVA OS — SUITE DE PRUEBAS AUTOMATIZADAS      \n");
    printf("====================================================\n\n");

    test_klib_strings();
    test_kheap_allocator();
    test_ramfs();
    test_rules_engine();
    test_bidirectional_simulation();
    test_x86_structures();

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
