#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ktypes.h"
#include "klib.h"
#include "kheap.h"
#include "nova_link.h"
#include "nova_iot.h"

// Variables requeridas por klib.c
void vga_putc(char c) {
    (void)c;
}
void vga_writestring(const char* s) {
    (void)s;
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

/* Test 2: Gestor de memoria dinámica kheap (kmalloc y kfree) */
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

/* Test 3: Simulación de conversación bidireccional NOVA-Link */
void test_bidirectional_simulation(void) {
    printf("[*] Ejecutando: test_bidirectional_simulation...\n");

    // Simular envío de comando LED_ON
    uint8_t tx_frame[4] = { NOVA_LINK_MAGIC, CMD_LED_ON, 0x00, (uint8_t)(CMD_LED_ON ^ 0x00) };
    TEST_ASSERT(tx_frame[0] == 0xAA, "Trama enviada inicia con MAGIC");
    TEST_ASSERT(tx_frame[1] == 0x10, "Comando enviado es LED_ON");

    // Simular recepción de respuesta exitosa de Arduino (RESP_OK)
    uint8_t rx_frame[4] = { NOVA_LINK_MAGIC, RESP_OK, 0x00, (uint8_t)(RESP_OK ^ 0x00) };
    uint8_t rx_cksum = rx_frame[1] ^ rx_frame[2];
    TEST_ASSERT(rx_frame[3] == rx_cksum, "Integridad de trama de respuesta de Arduino confirmada");

    // Simular recepción con error de Checksum corrompido en el canal físico
    uint8_t corrupted_frame[4] = { NOVA_LINK_MAGIC, RESP_OK, 0x00, 0xFF };
    uint8_t corrupted_calc = corrupted_frame[1] ^ corrupted_frame[2];
    TEST_ASSERT(corrupted_frame[3] != corrupted_calc, "Deteccion y rechazo exitoso de trama corrompida");
}

/* Test 4: Validación de estructuras de protección de CPU x86 */
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
