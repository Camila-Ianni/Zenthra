# Zenthra OS (Academic Project 2026)

> **Sistema Operativo Experimental Bare-Metal x86 (32 bits) para PC**  
> Proyecto de grado académico en desarrollo por Camila Ianni.

---

## 1. Visión y Alcance

Zenthra OS es un kernel monolítico de propósito general para **computadoras personales (laptops, PC de escritorio y estaciones de trabajo)**, diseñado bare-metal para arquitectura **x86 en modo protegido de 32 bits (i686 / BIOS)**. La meta de bajo consumo de recursos es concebida como una virtud de eficiencia ingenieril y no como una limitación a hardware modesto. El proyecto aborda los cimientos de la ingeniería de sistemas operativos: arranque Multiboot 1, inicialización de estructuras de hardware (GDT, IDT, PIC, PIT, UART), memoria física (PMM) y virtual (VMM) de 2 niveles, multitarea preemptiva Round-Robin (UP), seguridad ACL por proceso, interfaz gráfica de escritorio con framebuffer VESA LFB y enlace IoT serie bidireccional.

---

## 2. Estado de Implementación Técnica por Módulo

Para máxima transparencia técnica, el estado del código se clasifica rigurosamente en cuatro niveles:
- **REAL**: Implementado directamente a nivel de hardware/arquitectura bare-metal y validado con tests.
- **PARCIAL**: Operativo funcionalmente pero con simplificaciones temporales en curso de ampliación.
- **SIMULADA**: Lógica emulada o catálogos demostrativos en memoria RAM para fines de diseño de arquitectura.
- **PENDIENTE**: Planificado en el Roadmap para próximas fases.

| Subsistema / Característica | Estado Actual | Descripción Técnica |
| :--- | :---: | :--- |
| **Arranque y Bootloader** | **REAL** | Contrato Multiboot 1 oficial (`0x1BADB002`, `FLAGS = 0x7` con `ALIGN \| MEMINFO \| VIDEO_MODE`) solicitando modo gráfico lineal VESA `1024x768x32bpp` en `boot/boot.s` y validado con `grub-file --is-x86-multiboot` en CI. |
| **Segmentación de Memoria (GDT)** | **REAL** | 6 descriptores (Kernel Code/Data, User Code/Data, TSS de 32 bits disponible con DPL 0 `0x89` en selector `0x28`). Task Register cargado con `ltr 0x28` (RPL 0) y aserción de tamaño (104 bytes). |
| **Memoria Física (PMM)** | **REAL** | Asignador físico de marcos de 4 KiB basado en bitmap. Lectura y parseo del mapa de memoria Multiboot 1 con soporte de 64 bits (`addr_high`/`len_high`), validación estricta de límites de entrada (`entry->size`), protección de overflow mediante aritmética de 64 bits en `pmm_mark_region_used()` y `pmm_mark_region_free()`, y recorte a 1 GiB. Reserva explícita y verificada de regiones críticas: 1 MiB inferior, imagen ELF, Heap de 6 MiB (`0x00400000 - 0x00A00000`), estructura MBI, página de `mbi->cmdline`, módulos y MMIO de framebuffer. |
| **Paginación x86 y Memoria Virtual (VMM)** | **REAL** | Paginación de 32 bits por hardware (`CR0.PG`, `CR0.WP`, `CR3`). Directorio de páginas y tablas de 4 KiB. Mapeo identidad de 64 MiB (`KERNEL_IDENTITY_TABLES = 16`) en Ring 0. Separación estricta de funciones (`vmm_map_page`, `vmm_set_page_flags`, `vmm_unmap_page`) y `vmm_map_physical_range` con soporte de 64 bits a prueba de overflow para Framebuffer VESA LFB. |
| **Manejador de Page Faults (#PF) y Fixup EIP** | **REAL** | Interrupción Vector 14 en IDT (`page_fault.c`), lectura de `CR2`, verificación de copia activa (`syscall_is_user_copy_active`), comprobación de `regs->eip` dentro de `[__user_copy_start, __user_copy_end)` (`arch_user_copy_bytes` en `syscall_entry.s`) y redirección real de `regs->eip = __user_copy_fault_fixup` que aborta la copia y retorna `-EFAULT` sin bucle infinito ni Kernel Panic. Verificado en QEMU (`[VMM_FIXUP_QEMU_PASS]`). |
| **Interrupciones y PIC (IDT)** | **REAL** | 256 descriptores. Stubs assembly para 32 excepciones y 16 IRQs. Máscara PIC granular (IRQ0 de PIT, IRQ1 de teclado PS/2 e IRQ12 de ratón PS/2). |
| **Temporizador del Sistema (PIT)** | **REAL** | Intel 8253/8254 programado a 100 Hz (IRQ 0 / Vector 32) con ticks periódicos y sleep por `hlt`. |
| **Puerto Serie (COM1 UART 16550A)** | **REAL** | Inicialización con divisor de baudios (38400), FIFOs y salida serie para diagnóstico de depuración. |
| **Planificador y Multitarea** | **REAL (Preemptiva & UP)** | Scheduler Round-Robin con quantum (`TASK_DEFAULT_QUANTUM 5`), preempción forzada por temporizador PIT IRQ0 (`task_schedule_tick`), soporte de sueño/despertar (`task_sleep`), protección de sección crítica segura frente a IRQ (`irq_save`/`irq_restore`), tarea CPU-bound (`worker_cpubound`) preemptada exitosamente y cola de pilas zombies de 32 elementos con purga segura post-switch. |
| **Gestión de Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques con cabeceras de tamaño, flag de ocupado y coalescencia de fragmentos contiguos (6 MiB iniciales en `0x00400000`, alojando el `backbuffer` VESA de 3 MiB en RAM). |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil y Seguro)** | Sistema de archivos en RAM con inodos, operaciones de creación, lectura, escritura y borrado. `ramfs_write` rechaza escrituras que superen `RAMFS_MAX_SIZE` (1024 bytes) con código `-1` explícito. Almacenamiento persistente en disco pendiente. |
| **Llamadas al Sistema (Syscalls)** | **REAL (Núcleo + Fixup)** | Despachador `int 0x80` (`isr128`). Validación estricta fail-closed página por página en `copy_from_user()` y `copy_to_user()` más tabla de excepción hardware (`arch_user_copy_bytes`), lectura transaccional en `SYS_READ` (`keyboard_peek_queue`/`keyboard_discard_chars`), escritura parcial exacta en `SYS_WRITE`, y prevención de escalada de privilegios en `SYS_SET_PROFILE`. |
| **Seguridad y Perfiles por Proceso** | **REAL** | Credenciales de seguridad `profile` aisladas en el PCB de cada proceso (`struct task`), sin mutar el perfil global desde helpers de tarea. Registro de auditoría en `security.log`. |
| **Gestor de Paquetes / Store** | **SIMULADA (Metadata Demo)** | Catálogo demostrativo de aplicaciones en memoria para validar la arquitectura de perfiles de usuario (no ejecutables comerciales reales). Verificación de integridad por CRC32 (IEEE 802.3). |
| **Protocolo de Enlace Arduino (IoT)** | **REAL + SIMULADO** | Trama binaria estructurada con delimitador MAGIC (`0xAA55`), comandos (`PING`, `TEMP`, `LED_ON`, `LED_OFF`) y checksum XOR, más telemetría demostrativa etiquetada `[SIMULADO IoT]`. |
| **Escritorio Gráfico Nativo (VESA LFB + Glassmorphism)** | **REAL + DEMO LOCAL** | Inicializado en `1024x768x32bpp` desde GRUB 2 (`VIDEO_MODE = 1<<2`), doble búfer en RAM (`3 MiB`), Alpha Blending ARGB por píxel (`gfx_blend_argb`, `gfx_fill_rect_alpha`) para Glassmorphism real, conmutador de **Modo Eco** (opaco sin alpha) y **Tema Oscuro/Claro**, Dock flotante inferior, y **4 ventanas nativas**: 1) `Terminal Zenthra OS` (`[REAL]`), 2) `Monitor de Sistema & IoT` (`[REAL + SIMULADO IoT]`), 3) `Jarvis AI [DEMO LOCAL]` (`[SIMULADO - SIN RED]`), y 4) `Centro de 6 Agentes [SIM]` (`[SIMULADO]`). Verificado en QEMU (`[GUI_DESKTOP_QEMU_PASS]`). |

---

## 3. Requisitos de Construcción

### Entorno Linux (Ubuntu / Debian):
```bash
sudo apt-get update
sudo apt-get install -y gcc-multilib build-essential grub-pc-bin grub-common xorriso qemu-system-x86 clang mtools dosfstools
```

### Entorno macOS:
Para compilar y ejecutar la suite unitaria localmente en el host:
```bash
brew install clang-format
make test
```

---

## 4. Comandos de Compilación y Verificación

```bash
# Limpiar binarios y objetos anteriores
make clean

# Ejecutar suite de pruebas unitarias automatizadas (505 tests aprobados, 0 fallidos)
make test

# Compilar kernel ELF y generar la imagen ISO arrancable
make iso

# Validar el arranque automatizado en QEMU (test-complete + boot-smoke con VESA LFB 1024x768x32bpp)
make verify-qemu
```

---

## 5. Integración Continua (CI)

El proyecto cuenta con un pipeline automatizado en **GitHub Actions** ([`.github/workflows/ci.yml`](file:///Users/cami/Desktop/NOVA/.github/workflows/ci.yml)) que ante cada commit en `main`:
1. Ejecuta la suite de **505 pruebas unitarias** al 100%.
2. Compila el kernel bare-metal x86 (`build/zenthra_kernel.elf`).
3. Valida estrictamente la cabecera Multiboot 1 (`grub-file --is-x86-multiboot`).
4. Genera la imagen `build/zenthra_os.iso` con GRUB y calcula su hash SHA-256.
5. Ejecuta `make verify-qemu-complete` (verificando `[VMM_FIXUP_QEMU_PASS]` y `[TEST_COMPLETE] ZENTHRA_QEMU_TEST_COMPLETE` con apagado limpio ACPI) y `make verify-qemu-smoke` (verificando el arranque desde ISO con GRUB, `Framebuffer VESA LFB inicializado: 1024x768 a 32 bpp` y `[GUI_DESKTOP_QEMU_PASS]`).
6. Publica la ISO lista para su descarga en la sección de Artefactos de GitHub.

---

## 6. Documentación Adicional

- [**Documentación Integral de Ingeniería de Software y Sistemas Operativos (20 Secciones)**](docs/DOCUMENTACION_INTEGRAL_PROYECTO.md) *(Propuesta, Mobile, Web, Repo, Video Demo, Usuario Prueba, Manual, Casos de Uso, Casos de Prueba, RF/RNF, DER, Diagrama de Clases, Arquitectura C4, Componentes, Gantt, FODA, Mapa de Empatía, Buyer Persona, Benchmarking, SCAMPER)*
- [Guía de Arranque en VirtualBox](docs/virtualbox_guia.md)
- [Especificación Técnica del MVP Gráfico](docs/DESKTOP_MVP.md)
- [Estado Detallado y Matriz de Auditoría](docs/STATUS.md)
- [Hoja de Ruta del Proyecto (Roadmap)](docs/ROADMAP.md)

