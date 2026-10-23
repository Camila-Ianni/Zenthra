# Zenthra OS (Academic Project 2026)

> **Sistema Operativo Experimental Bare-Metal x86 (32 bits) para PC**  
> Proyecto de grado académico en desarrollo por Camila Ianni.

---

## 1. Visión y Alcance

Zenthra OS es un kernel experimental monolítico diseñado para arquitectura **x86 en modo protegido de 32 bits (i686)**. El proyecto aborda los cimientos de la ingeniería de sistemas operativos para computadoras personales: desde el arranque mediante especificación Multiboot y la carga de estructuras de hardware (GDT, IDT, PIC, PIT, UART), hasta la gestión de memoria dinámica, seguridad de acceso (ACL), despacho de llamadas al sistema, interfaz de comandos y comunicación serie/IoT bidireccional.

---

## 2. Estado de Implementación Técnica por Módulo

Para máxima transparencia técnica, el estado del código se clasifica rigurosamente en cuatro niveles:
- **REAL**: Implementado directamente a nivel de hardware/arquitectura bare-metal y validado con tests.
- **PARCIAL**: Operativo funcionalmente pero con simplificaciones temporales en curso de ampliación.
- **SIMULADA**: Lógica emulada o catálogos demostrativos en memoria RAM para fines de diseño.
- **PENDIENTE**: Planificado en el Roadmap para próximas fases.

| Subsistema / Característica | Estado Actual | Descripción Técnica |
| :--- | :---: | :--- |
| **Arranque y Bootloader** | **REAL** | Contrato Multiboot 1 oficial (`0x1BADB002`) con validación estricta por `grub-file --is-x86-multiboot`. |
| **Segmentación de Memoria (GDT)** | **REAL** | 6 descriptores (Kernel Code/Data, User Code/Data, TSS de 32 bits disponible con DPL 0 `0x89` en selector 0x28). Task Register cargado con `ltr 0x28` (RPL 0) y aserción de tamaño (104 bytes). |
| **Memoria Física (PMM)** | **REAL** | Asignador físico de marcos de 4 KiB basado en bitmap. Lectura y parseo del mapa de memoria Multiboot 1 con soporte de 64 bits (`addr_high`/`len_high`), validación estricta de límites de entrada (`entry->size`), protección de overflow mediante aritmética de 64 bits en `pmm_mark_region_used()` y `pmm_mark_region_free()`, liberación exclusiva de páginas completas y recorte estricto a 1 GiB. Reserva explícita y verificada de regiones críticas: 1 MiB inferior, imagen ELF, heap 2 MiB (0x00400000), MBI, módulos y MMIO de framebuffer si cae dentro de la RAM. Asignación de marcos bajos para tablas (`pmm_alloc_frame_below`). |
| **Paginación x86 y Memoria Virtual (VMM)** | **REAL** | Paginación de 32 bits por hardware (`CR0.PG`, `CR0.WP`, `CR3`). Directorio de páginas y tablas de 4 KiB. Mapeo identidad de 64 MiB (`KERNEL_IDENTITY_TABLES = 16`) en Ring 0. Tablas y directorios asignados estrictamente en memoria baja directamente accesible. Separación estricta de funciones: `vmm_map_page()` solo mapea direcciones no presentes (rechaza colisiones con `EEXIST`), `vmm_set_page_flags()` modifica permisos con invalidación de TLB (`invlpg`), y `vmm_unmap_page()` desmapea páginas. Función `vmm_map_physical_range` con soporte completo de 64 bits a prueba de overflow en bordes de 4 GiB, preservación de offset intra-página para MMIO/Framebuffer y liberación limpia de marcos de usuario. |
| **Manejador de Page Faults (#PF)** | **REAL** | Interrupción Vector 14 en IDT, captura de dirección causante desde `CR2`, decodificación de bits de fallo (presente, lectura/escritura, kernel/userspace). |
| **Interrupciones y PIC (IDT)** | **REAL** | 256 descriptores. Stubs assembly para 32 excepciones y 16 IRQs. Máscara PIC granular (IRQ0 de PIT y IRQ12 de ratón PS/2 habilitados selectivamente con validación de ACK/timeout). |
| **Temporizador del Sistema (PIT)** | **REAL** | Intel 8253/8254 programado a 100 Hz (IRQ 0 / Vector 32) con ticks periódicos y sleep por `hlt`. |
| **Puerto Serie (COM1 UART 16550A)** | **REAL** | Inicialización con divisor de baudios (38400), FIFOs y salida serie para diagnóstico de depuración. |
| **Planificador y Multitarea** | **REAL (Preemptiva & Segura)** | Scheduler Round-Robin con quantum (`TASK_DEFAULT_QUANTUM 5`), preempción forzada por temporizador PIT IRQ0 (`task_schedule_tick`), soporte de sueño/despertar (`task_sleep`), protección de sección crítica segura frente a IRQ (`irq_save()`/`irq_restore()`, `cli`/`popfl`), tarea CPU-bound (`worker_cpubound`) preemptada exitosamente. Cambio de contexto en ensamblador (`switch_context`), pilas dedicadas por tarea. Cola de pilas zombies ampliada a 32 elementos con drenado seguro post-switch y purga automática al crear tareas o ante saturación de cola. |
| **Gestión de Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques con cabeceras de tamaño, flag de ocupado y coalescencia de fragmentos contiguos (2 MiB iniciales en 0x00400000). |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil)** | Sistema de archivos en RAM con inodos, operaciones de creación, lectura, escritura y borrado. Almacenamiento persistente en disco pendiente. |
| **Llamadas al Sistema (Syscalls)** | **PROTOTIPO SEGURO** | Despachador `int 0x80` mediante stub assembly `isr128` con marco `struct registers` e `iret`. Validación estricta en `copy_from_user()` y `copy_to_user()` página por página en el directorio activo (`vmm_get_current_directory()`), exigiendo `PTE_PRESENT`, `PTE_USER` y `PTE_WRITABLE` para evitar fallos en Ring 0. Autorización ACL obligatoria asociada al proceso en `SYS_ARDUINO_CMD` (-13 EACCES en accesos no autorizados) y gestión de perfiles (`SYS_GET_PROFILE`, `SYS_SET_PROFILE`). |
| **Seguridad y Perfiles por Proceso** | **REAL** | Credenciales de seguridad `profile` en el bloque de control de cada proceso (`struct task`), impidiendo escalada global de permisos. Registro de auditoría en `security.log`. |
| **Gestor de Paquetes / Store** | **SIMULADA** | Catálogo demostrativo de aplicaciones con verificación de integridad por CRC32 (IEEE 802.3). Formato `.zpkg` firmado planificado para Fase 6. |
| **Protocolo de Enlace Arduino (IoT)** | **REAL** | Trama binaria estructurada con delimitador MAGIC (`0xAA55`), comandos (PING, TEMP, LED_ON, LED_OFF) y checksum XOR. |
| **Interfaz Gráfica de Escritorio (GUI)** | **REAL (MVP Funcional)** | Linear Framebuffer VESA / LFB (1024x768x32bpp) mapeado en VMM con `vmm_map_physical_range` con validación de overflow contra 4 GiB y dirección física válida, doble búfer anti-parpadeo (`gfx.c`) con fallback directo seguro (`backbuffer == frontbuffer`), fuente bitmap 8x16 (`font8x16.h`), driver de ratón PS/2 con ACK y timeout opcional (`mouse.c`), y Gestor de Ventanas Zenthra (`window_manager.c`) con gradiente corporativo, barra de tareas, menú launcher popup, terminal gráfica interactiva con comandos ejecutables (`help`, `sysinfo`, `mem`, `tasks`) conectados a telemetría del sistema y scheduler de tareas real (`task_get_info_at`), monitor de memoria y política de redibujado bajo demanda (`needs_redraw`). Fallback suave a consola VGA. |

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

# Ejecutar suite de pruebas unitarias automatizadas (209 tests)
make test

# Compilar kernel ELF y generar la imagen ISO arrancable
make iso

# Validar el arranque automatizado en QEMU y alternancia de tareas
make verify-qemu
```

---

## 5. Integración Continua (CI)

El proyecto cuenta con un pipeline automatizado en **GitHub Actions** (`.github/workflows/ci.yml`) que ante cada commit en `main`:
1. Ejecuta la suite de pruebas unitarias al 100%.
2. Compila el kernel bare-metal x86.
3. Valida estrictamente la cabecera Multiboot 1.
4. Genera la imagen `zenthra_os.iso` con GRUB y calcula su hash SHA-256.
5. Inicia QEMU en modo headless y valida el log serial (`serial.log`), verificando la respuesta de las interrupciones del PIT y la alternancia de tareas concurrentes.
6. Publica la ISO lista para su descarga en la sección de Artefactos de GitHub.

---

## 6. Documentación Adicional

- [Guía de Arranque en VirtualBox](docs/virtualbox_guia.md)
- [Especificación Técnica del MVP Gráfico](docs/DESKTOP_MVP.md)
- [Estado Detallado y Matriz de Auditoría](docs/STATUS.md)
- [Hoja de Ruta del Proyecto (Roadmap)](docs/ROADMAP.md)
