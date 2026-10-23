# Zenthra OS — Estado Técnico y Matriz de Auditoría

> **Fecha de Actualización**: 22 de octubre de 2026  
> **Versión**: 5.0 (Post-Auditoría Técnica v5)

---

## 1. Criterios de Clasificación y Niveles de Evidencia

Para garantizar rigor académico y de ingeniería de software en el proyecto de grado, se establecen categorías estrictas e independientes:

| Nivel de Verificación | Estado | Evidencia Concreta |
| :--- | :---: | :--- |
| **UNIT TESTS** | **PASS** | 92 pruebas automatizadas ejecutadas con `make test` (0 fallos). |
| **BUILD ELF** | **PASS** | Compilación y enlace limpio de `build/zenthra_kernel.elf` (i686). |
| **MULTIBOOT 1 VALIDATION** | **PASS** | Binario ELF validado estrictamente por `grub-file --is-x86-multiboot`. |
| **ISO GENERATION** | **PASS** | Imagen `build/zenthra_os.iso` construida con `grub-mkrescue` y hash SHA-256. |
| **QEMU HEADLESS BOOT** | **PASS** | Verificado con `make verify-qemu`: captura de `serial.log` con banner del kernel, ticks periódicos IRQ0 de PIT y alternancia de tareas concurrentes. |
| **VIRTUALBOX** | **PROCEDIMIENTO VALIDADO** | Procedimiento de arranque documentado en [docs/virtualbox_guia.md](virtualbox_guia.md) utilizando la ISO oficial. |
| **SYSCALL RING 3** | **PROTOTIPO / PENDIENTE** | Stub de bajo nivel `isr128` en assembly con marco `struct registers` e `iret`. Las pruebas actuales validan límites lógicos (`is_user_pointer`). La frontera con aislamiento total requiere Paging (CR0.PG) y TSS, clasificados como **PENDIENTE**. |
| **DESKTOP GUI** | **PENDIENTE** | Diseñado en [docs/DESKTOP_MVP.md](DESKTOP_MVP.md). Código actual implementa consola de texto VGA 80x25 como modo de rescate. |

---

## 2. Matriz de Estado por Subsistema

| Subsistema / Módulo | Estado | Componentes Reales Verificados | Limitaciones / Siguiente Hito |
| :--- | :---: | :--- | :--- |
| **Bootloader & Multiboot** | **REAL** | Encabezado Multiboot 1 y 2, verificación estricta por `grub-file`, lectura de memoria baja y extendida. | Paging y allocator de frames físicos planificados para Fase 3. |
| **Segmentación (GDT)** | **REAL** | Selectores de 32 bits para Código/Datos Kernel (0x08, 0x10) y Usuario (0x18, 0x20). | Requiere entrada TSS (Task State Segment) con `ESP0` para transiciones de privilegio Ring 3->Ring 0. |
| **IDT & Interrupciones** | **REAL** | Tabla de 256 descriptores, stubs en assembly para 32 excepciones CPU y 16 IRQs PIC (32..47). | Volcado de diagnóstico y registros en COM1 implementado. |
| **Temporizador (PIT 8254)** | **REAL** | Configurado a 100 Hz (10 ms por tick), interrupción IRQ0 con callback, `pit_sleep` con instrucción `hlt`. | Salida serie periódica de verificación de ticks activa. |
| **Puerto Serie (UART 16550A)** | **REAL** | COM1 (`0x3F8`), divisor de baudios (38400), registro FIFO y salida serie para telemetría y diagnóstico. | Recepción por interrupción IRQ 4 sujeta a búfer circular. |
| **Multitarea y Conmutación** | **REAL** | Scheduler Round-Robin, `switch_context` en assembly, pilas independientes, ciclo de vida (`TASK_READY`, `TASK_RUNNING`, `TASK_TERMINATED`). | Liberación diferida de stack en `task_exit` sin use-after-free probada en suite unitaria y QEMU. |
| **Llamadas al Sistema (Syscalls)** | **PROTOTIPO** | Interrupción `int 0x80` despachada mediante stub assembly `isr128` que arma el marco `struct registers` y retorna con `iret`. Funciones `copy_from_user` y `copy_to_user` con validación estricta sin desbordamiento. | Aislamiento forzado por hardware y manejo de Page Faults en userspace pendiente de paginación. |
| **Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques contiguos con cabeceras de tamaño, flag de ocupación y coalescencia de fragmentos contiguos (2 MiB iniciales en `0x00400000`). | Expansión dinámica mediante páginas virtuales en Fase 3. |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil)** | Inodos en memoria RAM, creación, lectura, escritura y borrado de archivos temporales. | Almacenamiento persistente en disco en Fase 7. |
| **Matriz de Seguridad (ACL)** | **REAL** | 11 perfiles de usuario con permisos por operación y auditoría en `security.log`. | Permisos validados a nivel de kernel/shell; forzamiento por MMU en Fase 3. |
| **Catálogo de Paquetes** | **SIMULADA** | Catálogo en memoria con verificación de integridad de datos por CRC32 IEEE 802.3. | Paquetes `.zpkg` con binarios ELF y firma criptográfica en Fase 6. |
| **Protocolo Arduino (IoT)** | **REAL** | Trama binaria estructurada con delimitador `0xAA55`, comandos PING, TEMP, LED y checksum XOR. | Probado en simulación y soporte serie COM1 en hardware real. |
| **Control de Energía (ACPI / 8042)** | **REAL** | Puertos de apagado ACPI en emulador (`0x604`, `0x0804`, `0x4004`) y reinicio por teclado 8042. | ACPI DSDT/SSDT avanzado para placas modernas en Fase 8. |
| **Interfaz VGA Modo Texto** | **REAL** | Búfer `0xB8000` 80x25 con shell interactiva (modo de rescate y diagnóstico). | Se conservará permanentemente como consola de recuperación. |
| **Escritorio Gráfico (GUI)** | **PENDIENTE** | Especificación completa en [docs/DESKTOP_MVP.md](DESKTOP_MVP.md). | Framebuffer VESA 1024x768x32bpp, doble búfer, mouse PS/2 y gestor de ventanas en Fase 5. |
