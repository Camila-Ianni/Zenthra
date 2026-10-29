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
    /* Limpiar flags de CPU (asegurar dirección hacia adelante e interrupciones apagadas) */
    cld
    cli

    /* Preservar parámetros Multiboot en registros no modificados por rep stosb */
    mov %eax, %esi  /* %esi = Magic Multiboot (0x2BADB002) */
    mov %ebx, %edx  /* %edx = Puntero a Multiboot Info */

    /* Purgado explícito de la sección .bss a ceros (Bare-Metal Startup CRT0) */
    mov $_bss_start, %edi
    mov $_bss_end, %ecx
    sub %edi, %ecx
    xor %eax, %eax
    rep stosb

    /* Inicializamos el puntero de pila una vez sanitizada la memoria .bss */
    mov $stack_top, %esp

    /* Emitir código de diagnóstico POST 0x01 al puerto I/O 0x80 (BSS purgada y CRT0 listo) */
    mov $0x01, %al
    out %al, $0x80

    /* Pasamos los parámetros Multiboot a kernel_main(magic, multiboot_info_ptr) */
    push %edx  /* mbi_addr */
    push %esi  /* magic */

    /* Llamar a la función principal en C */
    call kernel_main

    /* Si kernel_main retorna, entramos en un bucle infinito seguro */
    cli
.Lhang:
    hlt
    jmp .Lhang
.size _start, . - _start
