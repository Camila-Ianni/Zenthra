/*
 * interrupts.s — Stubs de bajo nivel para excepciones CPU, IRQs y cambio de contexto
 * Arquitectura: x86 (i386 / 32-bit Protected Mode)
 * Zenthra OS (Academic Project)
 */

.section .text

.macro ISR_NOERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
    pushl $0            /* Error code ficticio para mantener marco simétrico */
    pushl $\num         /* Número de interrupción */
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
    /* La CPU empuja automáticamente el código de error */
    pushl $\num         /* Número de interrupción */
    jmp isr_common_stub
.endm

.macro IRQ_STUB irq_num, int_num
.global irq\irq_num
.type irq\irq_num, @function
irq\irq_num:
    pushl $0            /* Error code ficticio */
    pushl $\int_num     /* Vector mapeado en PIC (32..47) */
    jmp irq_common_stub
.endm

/* Excepciones de CPU (0 a 31) */
ISR_NOERRCODE 0   /* #DE Divide Error */
ISR_NOERRCODE 1   /* #DB Debug */
ISR_NOERRCODE 2   /* NMI Interrupt */
ISR_NOERRCODE 3   /* #BP Breakpoint */
ISR_NOERRCODE 4   /* #OF Overflow */
ISR_NOERRCODE 5   /* #BR Bound Range Exceeded */
ISR_NOERRCODE 6   /* #UD Invalid Opcode */
ISR_NOERRCODE 7   /* #NM Device Not Available */
ISR_ERRCODE   8   /* #DF Double Fault (con error code) */
ISR_NOERRCODE 9   /* Coprocessor Segment Overrun */
ISR_ERRCODE   10  /* #TS Invalid TSS (con error code) */
ISR_ERRCODE   11  /* #NP Segment Not Present (con error code) */
ISR_ERRCODE   12  /* #SS Stack-Segment Fault (con error code) */
ISR_ERRCODE   13  /* #GP General Protection Fault (con error code) */
ISR_ERRCODE   14  /* #PF Page Fault (con error code) */
ISR_NOERRCODE 15  /* Reservado */
ISR_NOERRCODE 16  /* #MF x87 FPU Floating-Point Error */
ISR_ERRCODE   17  /* #AC Alignment Check (con error code) */
ISR_NOERRCODE 18  /* #MC Machine Check */
ISR_NOERRCODE 19  /* #XM SIMD Floating-Point Exception */
ISR_NOERRCODE 20  /* #VE Virtualization Exception */
ISR_ERRCODE   21  /* #CP Control Protection Exception */
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30  /* #SX Security Exception */
ISR_NOERRCODE 31

/* Interrupciones de Hardware PIC (IRQ 0..15 -> Vectores 32..47) */
IRQ_STUB 0,  32
IRQ_STUB 1,  33
IRQ_STUB 2,  34
IRQ_STUB 3,  35
IRQ_STUB 4,  36
IRQ_STUB 5,  37
IRQ_STUB 6,  38
IRQ_STUB 7,  39
IRQ_STUB 8,  40
IRQ_STUB 9,  41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

/* Despachador común para Excepciones CPU */
.extern isr_handler
isr_common_stub:
    pusha               /* Guarda EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX */
    pushl %ds           /* Guarda segmento de datos actual */

    movw $0x10, %ax     /* Carga selector de segmento de datos del kernel */
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %esp          /* Puntero a struct registers */
    call isr_handler
    addl $4, %esp       /* Desapilar puntero */

    popl %ds            /* Restaurar selector de segmento de datos */
    popa                /* Restaurar registros de propósito general */
    addl $8, %esp       /* Desapilar código de error y número de interrupción */
    iret

/* Despachador común para Interrupciones de Hardware (IRQs) */
.extern irq_handler
irq_common_stub:
    pusha
    pushl %ds

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %esp
    call irq_handler
    addl $4, %esp

    popl %ds
    popa
    addl $8, %esp
    iret

/*
 * switch_context:
 *   Cambio real de contexto de tarea x86.
 *   Prototipo C: void switch_context(uint32_t* old_esp_ptr, uint32_t new_esp);
 */
.global switch_context
.type switch_context, @function
switch_context:
    /* Empujar registros preservados por la convención cdecl (callee-saved) y flags */
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    pushfl

    /* 
     * Pila actual tras los 5 push (20 bytes):
     *   [esp +  0] = EFLAGS
     *   [esp +  4] = EDI
     *   [esp +  8] = ESI
     *   [esp + 12] = EBX
     *   [esp + 16] = EBP
     *   [esp + 20] = Retorno de llamada (EIP)
     *   [esp + 24] = old_esp_ptr
     *   [esp + 28] = new_esp
     */
    movl 24(%esp), %eax
    testl %eax, %eax
    jz 1f
    movl %esp, (%eax)   /* Guardar el ESP actual en *old_esp_ptr */
1:
    movl 28(%esp), %esp /* Cambiar el registro de pila de la CPU al nuevo ESP */

    /* Restaurar registros de la tarea entrante */
    popfl
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret                 /* Salta a la dirección de retorno de la nueva tarea */
.size switch_context, . - switch_context
