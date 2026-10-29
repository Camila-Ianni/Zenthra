/* Linker script para NOVA OS
 * Define la estructura física de memoria del kernel a partir de 1MB (0x100000)
 */

ENTRY(_start)

SECTIONS
{
    /* Cargar el kernel en la dirección estándar superior a la memoria convencional (1MB) */
    . = 1M;

    /* Primero va la cabecera Multiboot para que sea detectada por GRUB/QEMU */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Secciones de datos de solo lectura (strings constantes, etc.) */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Secciones de datos leídos y modificados (inicializados) */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* Datos sin inicializar y la pila del kernel */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        _bss_end = .;
    }

    /* Símbolo para calcular el final de la memoria del kernel */
    _kernel_end = .;
}
