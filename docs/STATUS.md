# Zenthra OS — Estado Técnico y Matriz de Auditoría

> **Fecha de Actualización**: 23 de octubre de 2026  
> **Versión**: 8.0 (Post-Auditoría Técnica v8 — Correcciones Críticas de Seguridad y Estabilidad)

---

## 1. Criterios de Clasificación y Niveles de Evidencia

Para garantizar rigor académico y de ingeniería de software en el proyecto de grado, se establecen categorías estrictas e independientes:

| Nivel de Verificación | Estado | Evidencia Concreta |
| :--- | :---: | :--- |
| **UNIT TESTS** | **PASS** | 182 pruebas automatizadas ejecutadas con `make test` (0 fallos). |
| **BUILD ELF** | **PASS** | Compilación y enlace limpio de `build/zenthra_kernel.elf` (i686). |
| **MULTIBOOT 1 VALIDATION** | **PASS** | Contrato oficial de arranque Multiboot 1 validado estrictamente por `grub-file --is-x86-multiboot`. |
| **ISO GENERATION** | **PASS** | Imagen `build/zenthra_os.iso` construida en pipeline de CI con `grub-mkrescue` y hash SHA-256 verificado. |
| **QEMU HEADLESS BOOT** | **PASS** | Verificado en CI con `make verify-qemu`: banner serial, PMM activo, Paginación x86 VMM activa en CR0/CR3, GFX/Mouse/WM listos, ticks PIT IRQ0 y alternancia de tareas concurrentes con preempción de tarea CPU-bound. |
| **VIRTUALBOX** | **PROCEDIMIENTO VALIDADO** | Procedimiento de arranque documentado en [docs/virtualbox_guia.md](virtualbox_guia.md) utilizando la ISO oficial. |
| **SCHEDULER & STACKS** | **REAL (SEGURO)** | Pila de tarea terminada liberada estrictamente después de conmutar la CPU con `switch_context`, eliminando cualquier use-after-free. |
| **PMM & VMM PAGING** | **REAL (SEGURO)** | PMM procesa mapas de 64 bits (`addr_high`/`len_high`) con protección de overflow y recorte a 1 GiB. VMM rechaza mapeos `PTE_USER` en los 16 MiB del kernel, provee `vmm_map_range` para Framebuffer MMIO y libera marcos de usuario en `vmm_destroy_process_directory`. |
| **DESKTOP GUI & INPUT** | **REAL (MVP FUNCIONAL)** | VESA LFB mapeado con `vmm_map_range`, exclusivamente 32 bpp validado contra desbordamiento. Driver de ratón PS/2 con ACK y timeout opcional. Terminal gráfica editable conectada al teclado con comandos ejecutables (`help`, `sysinfo`, `mem`, `tasks`), launcher popup en botón Zenthra y redibujado bajo demanda (`needs_redraw`). |

---

## 2. Matriz de Estado por Subsistema

| Subsistema / Módulo | Estado | Componentes Reales Verificados | Limitaciones / Siguiente Hito |
| :--- | :---: | :--- | :--- |
| **Bootloader & Multiboot** | **REAL** | Contrato oficial Multiboot 1 (`0x1BADB002`), verificación estricta obligatoria por `grub-file`, lectura de mapa físico de memoria (`mmap`). | Parser soporta tags Multiboot 2 para futura migración; contrato oficial actual es Multiboot 1. |
| **Segmentación (GDT & TSS)** | **REAL** | 6 descriptores (Kernel Code/Data, User Code/Data, TSS x86 de 32 bits disponible con DPL 0 `0x89` en descriptor 0x28). Task Register cargado mediante `ltr %%ax` con selector `0x28` (RPL 0). Verificación estática de tamaño de TSS (104 bytes). | Preparado para transición Ring 3 -> Ring 0 en syscalls e interrupciones. |
| **Memoria Física (PMM)** | **REAL** | Gestor de marcos de 4 KiB basado en bitmap, parseo de mapas Multiboot 1 de 64 bits (`addr_high`/`len_high`), protección contra overflow, recorte a 1 GiB, reserva estricta de 1 MiB inferior, kernel ELF, estructuras bootloader y heap. Reciclaje First-Fit. | PMM soporta hasta 1 GiB de memoria física bare-metal. |
| **Paginación y Memoria Virtual (VMM)** | **REAL** | Directorio y tablas de páginas x86 de 32 bits (4 KiB por página). Mapeo de identidad de 16 MiB para Ring 0 (código kernel, heap 4-6 MB, VGA 0xB8000). Activación de `CR0.PG` (bit 31) y `CR0.WP` (bit 16). Mapeo de rangos con `vmm_map_range`, protección de espacio kernel contra páginas `PTE_USER`, mapeo/desmapeo dinámico y liberación de marcos de usuario al destruir directorios privados. | Expansión a PAE / 64-bit planificada en Fase 8. |
| **Manejador de Page Faults (#PF)** | **REAL** | Vector 14 en IDT (`isr14`), lectura de dirección virtual causante en `CR2`, decodificación precisa de bits de código de error (presente, escritura/lectura, modo usuario/kernel, fetch de instrucción). | Manejo de Copy-On-Write (COW) y swap en fases avanzadas. |
| **IDT & PIC Granular** | **REAL** | Tabla de 256 descriptores, stubs en assembly para 32 excepciones CPU y 16 IRQs PIC (32..47). Máscaras PIC granular con desmascarado selectivo de IRQ0 (PIT) e IRQ12 (ratón si está presente). | Habilitación de interrupciones adicionales según periféricos detectados. |
| **Temporizador (PIT 8254)** | **REAL** | Configurado a 100 Hz (10 ms por tick), interrupción IRQ0 con callback, `pic_clear_mask(0)`, preempción por tick y `pit_sleep` con instrucción `hlt`. | Sincronización de reloj en GUI a 1 Hz bajo demanda. |
| **Puerto Serie (UART 16550A)** | **REAL** | COM1 (`0x3F8`), divisor de baudios (38400), registro FIFO y salida serie para telemetría y diagnóstico. | Recepción por interrupción IRQ 4 sujeta a búfer circular. |
| **Multitarea y Conmutación** | **REAL (Preemptiva & Segura)** | Scheduler Round-Robin con quantum (`TASK_DEFAULT_QUANTUM 5`), preempción forzada por temporizador PIT IRQ0 (`task_schedule_tick`), soporte de sueño/despertar (`task_sleep`), protección de sección crítica (`scheduler_lock`/`scheduler_unlock`), tarea CPU-bound (`worker_cpubound`) preemptada exitosamente. `switch_context` en assembly, pilas dedicadas por tarea. Pila zombie liberada estrictamente después de conmutar a la nueva tarea activa, eliminando use-after-free. | Conmutación de directorio de páginas por tarea en Ring 3. |
| **Llamadas al Sistema (Syscalls)** | **PROTOTIPO** | Interrupción `int 0x80` despachada mediante stub assembly `isr128` que arma el marco `struct registers` e `iret`. Autorización ACL obligatoria en `SYS_ARDUINO_CMD` (-13 EACCES si el perfil del proceso carece de permiso), `SYS_GET_PROFILE` y `SYS_SET_PROFILE`. | Enlace directo con loader de binarios de usuario en Fase 6. |
| **Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques contiguos con cabeceras de tamaño, flag de ocupación y coalescencia de fragmentos contiguos (2 MiB iniciales en `0x00400000`). | Ubicado dentro del rango mapeado de 16 MiB. |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil)** | Inodos en memoria RAM, creación, lectura, escritura y borrado de archivos temporales. | Almacenamiento persistente en disco en Fase 7. |
| **Matriz de Seguridad (ACL)** | **REAL (Por Proceso)** | Cada tarea/proceso posee su credencial `profile` propia en `struct task`. Soporte para 11 perfiles de usuario. Validado directamente sobre el proceso llamador en la syscall, eliminando escalada global. | Combinado con aislamiento de páginas VMM. |
| **Catálogo de Paquetes** | **SIMULADA** | Catálogo en memoria con verificación de integridad de datos por CRC32 IEEE 802.3. | Paquetes `.zpkg` con binarios ELF y firma criptográfica en Fase 6. |
| **Protocolo Arduino (IoT)** | **REAL** | Trama binaria estructurada con delimitador `0xAA`, comandos PING, TEMP, LED y checksum XOR. Telemetría integrada en ventana de Monitor (marcada como SIMULADA/HARDWARE). | Soporte serie COM1 en hardware real. |
| **Control de Energía (ACPI / 8042)** | **REAL** | Puertos de apagado ACPI en emulador (`0x604`, `0x0804`, `0x4004`) y reinicio por teclado 8042. | ACPI DSDT/SSDT avanzado para placas modernas en Fase 8. |
| **Interfaz VGA Modo Texto** | **REAL** | Búfer `0xB8000` 80x25 con shell interactiva (modo de rescate y diagnóstico). | Se conservará permanentemente como consola de recuperación. |
| **Escritorio Gráfico (GUI)** | **REAL (MVP Funcional)** | VESA Linear Framebuffer (LFB) 1024x768x32bpp mapeado en VMM con `vmm_map_range`, doble búfer en RAM (`backbuffer`), primitivas 2D con recorte (`gfx.c`), fuente bitmap 8x16 (`font8x16.h`), driver PS/2 de ratón con verificación de ACK y timeout (`mouse.c`), y gestor de ventanas (`window_manager.c`) con fondo degradado corporativo, barra de tareas, menú launcher popup, terminal interactiva con historial y comandos ejecutables, monitor de memoria en vivo y política de redibujado bajo demanda (`needs_redraw`). Fallback automático a modo texto VGA. | Driver de aceleración de vídeo VirtIO-GPU en Fase 8. |
