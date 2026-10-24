/* Multiboot Header Specification Constants */
.set ALIGN,      1<<0                       /* Alineación de módulos en límites de página (4KB) */
.set MEMINFO,    1<<1                       /* Proporcionar mapa de memoria */
.set VIDEO_MODE, 1<<2                       /* Solicitar modo gráfico VESA Linear Framebuffer (LFB) */
.set FLAGS,      ALIGN | MEMINFO | VIDEO_MODE /* Flags multiboot (0x00000007) */
.set MAGIC,      0x1BADB002                 /* Número mágico Multiboot 1 */
.set CHECKSUM,   -(MAGIC + FLAGS)           /* Checksum requerido por la especificación */

/* Sección del encabezado Multiboot (debe estar en los primeros 8KB del ejecutable) */
.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0          /* header_addr (no usado cuando bit 16 = 0) */
.long 0          /* load_addr */
.long 0          /* load_end_addr */
.long 0          /* bss_end_addr */
.long 0          /* entry_addr */
.long 0          /* mode_type: 0 = Linear Graphics Framebuffer (LFB) */
.long 1024       /* width: 1024 px */
.long 768        /* height: 768 px */
.long 32         /* depth: 32 bpp (ARGB 8888) */

/* Reserva de espacio para la pila (Stack) del Kernel: 16 KiB */
.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16 KiB */
stack_top:

/* Punto de entrada del kernel */
.section .text
.global _start
.type _start, @function
_start:
    /* Inicializamos el puntero de pila */
    mov $stack_top, %esp

    /* Limpiar flags de CPU (asegurar dirección de cadenas e interrupciones apagadas) */
    cld
    cli

    /* Pasamos los parámetros Multiboot a kernel_main(magic, multiboot_info_ptr) */
    push %ebx  /* Puntero a la estructura Multiboot Info provista por el bootloader */
    push %eax  /* Valor mágico (debe ser 0x2BADB002) */

    /* Llamar a la función principal en C */
    call kernel_main

    /* Si kernel_main retorna, entramos en un bucle infinito seguro */
    cli
.Lhang:
    hlt
    jmp .Lhang
.size _start, . - _start
