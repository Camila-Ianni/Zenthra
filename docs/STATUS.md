# Zenthra OS — Estado Técnico y Matriz de Auditoría

> **Fecha de Actualización**: 22 de octubre de 2026  
> **Versión**: 7.0 (Post-Auditoría Técnica v7)

---

## 1. Criterios de Clasificación y Niveles de Evidencia

Para garantizar rigor académico y de ingeniería de software en el proyecto de grado, se establecen categorías estrictas e independientes:

| Nivel de Verificación | Estado | Evidencia Concreta |
| :--- | :---: | :--- |
| **UNIT TESTS** | **PASS** | 97 pruebas automatizadas ejecutadas con `make test` (0 fallos). |
| **BUILD ELF** | **PASS** | Compilación y enlace limpio de `build/zenthra_kernel.elf` (i686). |
| **MULTIBOOT 1 VALIDATION** | **PASS** | Contrato oficial de arranque Multiboot 1 validado estrictamente por `grub-file --is-x86-multiboot`. |
| **ISO GENERATION** | **PASS** | Imagen `build/zenthra_os.iso` construida con `grub-mkrescue` y hash SHA-256. |
| **QEMU HEADLESS BOOT** | **PASS** | Verificado con `make verify-qemu`: captura de `serial.log` con banner del kernel, ticks periódicos IRQ0 de PIT y alternancia de tareas concurrentes. |
| **VIRTUALBOX** | **PROCEDIMIENTO VALIDADO** | Procedimiento de arranque documentado en [docs/virtualbox_guia.md](virtualbox_guia.md) utilizando la ISO oficial. |
| **SYSCALL RING 3** | **PROTOTIPO / PENDIENTE** | Stub assembly `isr128` con marco `struct registers` e `iret`, TSS de hardware configurado (DPL 0 `0x89`, selector `0x28`, `esp0 = kernel_stack_top`). Syscalls validan rango lógico y credencial ACL asociada al proceso. Paging y procesos de usuario aislados clasificados como **PENDIENTE** (Fase 4). |
| **DESKTOP GUI** | **PENDIENTE** | Diseñado en [docs/DESKTOP_MVP.md](DESKTOP_MVP.md). Código actual implementa consola de texto VGA 80x25 como modo de rescate. |

---

## 2. Matriz de Estado por Subsistema

| Subsistema / Módulo | Estado | Componentes Reales Verificados | Limitaciones / Siguiente Hito |
| :--- | :---: | :--- | :--- |
| **Bootloader & Multiboot** | **REAL** | Contrato oficial Multiboot 1 (`0x1BADB002`), verificación estricta obligatoria por `grub-file`, lectura de memoria física. | Parser soporta tags Multiboot 2 para futura migración; contrato oficial actual es Multiboot 1. Paging en Fase 4. |
| **Segmentación (GDT & TSS)** | **REAL** | 6 descriptores (Kernel Code/Data, User Code/Data, TSS x86 de 32 bits disponible con DPL 0 `0x89` en descriptor 0x28). Task Register cargado mediante `ltr %%ax` con selector `0x28` (RPL 0). Verificación estática de tamaño de TSS (104 bytes). | Preparado para transición Ring 3 -> Ring 0 en syscalls e interrupciones. |
| **IDT & PIC Granular** | **REAL** | Tabla de 256 descriptores, stubs en assembly para 32 excepciones CPU y 16 IRQs PIC (32..47). Máscaras PIC inicializadas en `0xFB` (Master) y `0xFF` (Slave) enmascarando IRQs no usadas. Desmascarado selectivo con `pic_clear_mask(0)` para PIT. | Habilitación de IRQ1 (teclado) si se migra a interrupciones en lugar de polling. |
| **Temporizador (PIT 8254)** | **REAL** | Configurado a 100 Hz (10 ms por tick), interrupción IRQ0 con callback, `pic_clear_mask(0)`, `pit_sleep` con instrucción `hlt`. | Salida serie periódica de verificación de ticks activa. |
| **Puerto Serie (UART 16550A)** | **REAL** | COM1 (`0x3F8`), divisor de baudios (38400), registro FIFO y salida serie para telemetría y diagnóstico. | Recepción por interrupción IRQ 4 sujeta a búfer circular. |
| **Multitarea y Conmutación** | **REAL (Cooperativa)** | Scheduler Round-Robin, `switch_context` en assembly, pilas dedicadas por tarea (incluyendo pila propia para tarea 0 principal). Separación estricta entre `saved_esp` y `kernel_stack_top` cargado en `tss.esp0`. Marco inicial de pila alineado: `[EFLAGS, EDI, ESI, EBX, EBP, entry_point, task_exit]`. | Preempción automática por interrupción PIT IRQ0 planificada en Fase 3. |
| **Llamadas al Sistema (Syscalls)** | **PROTOTIPO** | Interrupción `int 0x80` despachada mediante stub assembly `isr128` que arma el marco `struct registers` e `iret`. Autorización ACL obligatoria en `SYS_ARDUINO_CMD` (-13 EACCES si el perfil del proceso carece de permiso), `SYS_GET_PROFILE` y `SYS_SET_PROFILE`. | Aislamiento forzado por hardware y manejo de Page Faults en userspace pendiente de paginación. |
| **Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques contiguos con cabeceras de tamaño, flag de ocupación y coalescencia de fragmentos contiguos (2 MiB iniciales en `0x00400000`). | Expansión dinámica mediante páginas virtuales en Fase 4. |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil)** | Inodos en memoria RAM, creación, lectura, escritura y borrado de archivos temporales. | Almacenamiento persistente en disco en Fase 7. |
| **Matriz de Seguridad (ACL)** | **REAL (Por Proceso)** | Cada tarea/proceso posee su credencial `profile` propia en `struct task`. Soporte para 11 perfiles de usuario. Validado directamente sobre el proceso llamador en la syscall, eliminando escalada global. | MMU paging en Fase 4. |
| **Catálogo de Paquetes** | **SIMULADA** | Catálogo en memoria con verificación de integridad de datos por CRC32 IEEE 802.3. | Paquetes `.zpkg` con binarios ELF y firma criptográfica en Fase 6. |
| **Protocolo Arduino (IoT)** | **REAL** | Trama binaria estructurada con delimitador `0xAA`, comandos PING, TEMP, LED y checksum XOR. Validado con control de acceso por proceso en syscall. | Probado en simulación y soporte serie COM1 en hardware real. |
| **Control de Energía (ACPI / 8042)** | **REAL** | Puertos de apagado ACPI en emulador (`0x604`, `0x0804`, `0x4004`) y reinicio por teclado 8042. | ACPI DSDT/SSDT avanzado para placas modernas en Fase 8. |
| **Interfaz VGA Modo Texto** | **REAL** | Búfer `0xB8000` 80x25 con shell interactiva (modo de rescate y diagnóstico). | Se conservará permanentemente como consola de recuperación. |
| **Escritorio Gráfico (GUI)** | **PENDIENTE** | Especificación completa en [docs/DESKTOP_MVP.md](DESKTOP_MVP.md). | Framebuffer VESA 1024x768x32bpp, doble búfer, mouse PS/2 y gestor de ventanas en Fase 5. |
