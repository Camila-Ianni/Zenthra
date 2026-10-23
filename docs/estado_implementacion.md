# Documentación Técnica y Estado de Implementación — Zenthra OS

## 1. Resumen de Ejecución
Este documento consolida el estado técnico del sistema operativo experimental **Zenthra OS**, implementado bajo la arquitectura x86 en modo protegido de 32 bits, con integración física mediante el protocolo serie **ZIoT / Zenthra-Link** hacia microcontroladores Arduino.

---

## 2. Componentes Implementados

### 2.1 Núcleo y Arquitectura x86
- **Multiboot Specification (Dual 1 y 2)**: Soporte completo en [`kernel/include/multiboot.h`](file:///Users/cami/Desktop/NOVA/kernel/include/multiboot.h) y [`boot/boot.s`](file:///Users/cami/Desktop/NOVA/boot/boot.s) para arrancar bajo Multiboot 1 (`0x2BADB002`) y Multiboot 2 (`0x36D76289`). Incluye salvaguarda crítica contra bucles infinitos por etiquetas malformadas (`tag->size < 8` y alineación a 8 bytes `~7`).
- **Control de Energía y Apagado ACPI**: En [`kernel/include/power.h`](file:///Users/cami/Desktop/NOVA/kernel/include/power.h), rutinas `qemu_poweroff()` para salida limpia en QEMU/VirtualBox mediante puertos ACPI (`0x604`, `0x0804`, `0x4004`, `0x501`) y `cpu_reboot()` mediante el controlador de teclado 8042 (puerto `0x64`).
- **Linker Script (`linker.ld`)**: Carga el binario a partir de la dirección `0x100000` (1MB), alineando bloques en páginas de 4KB.
- **GDT (Global Descriptor Table)**: Configuración en [`kernel/arch/x86/gdt.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/gdt.c) con 5 descriptores: Nulo, Código Kernel (Ring 0), Datos Kernel (Ring 0), Código Usuario (Ring 3) y Datos Usuario (Ring 3).
- **IDT (Interrupt Descriptor Table)**: Tabla de 256 descriptores en [`kernel/arch/x86/idt.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/idt.c) con remapeo del controlador PIC 8259 (IRQs 0-15 a los vectores 32-47).
- **PIT 8254 (Programmable Interval Timer)**: En [`kernel/drivers/pit.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/pit.c), generador de onda cuadrada a 100 Hz conectado a IRQ0 (Vector 32) para conteo de ticks y función de retardo `pit_sleep(ms)`.
- **Planificador Multitarea (Scheduler & PCB)**: En [`kernel/drivers/task.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/task.c) y [`kernel/include/task.h`](file:///Users/cami/Desktop/NOVA/kernel/include/task.h). Soporte para Bloque de Control de Proceso (PCB), creación dinámica de tareas con pila independiente (`task_create`), alternancia de contexto Round-Robin (`task_yield`), ciclo de vida (`TASK_READY`, `TASK_RUNNING`, `TASK_SLEEPING`, `TASK_TERMINATED`) y tarea de fondo demonio (`iot_daemon`).
- **Syscalls Seguras (Int 0x80)**: Despachador en [`kernel/arch/x86/syscall.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/syscall.c) con protección estricta contra ataques de inyección y lecturas fuera de límites desde Ring 3:
  - `is_user_pointer(ptr, size)`: Verifica punteros no nulos, previene desbordamiento aritmético y bloquea el acceso al espacio reservado del kernel (`USER_MEMORY_LIMIT`).
  - `copy_from_user(k_dest, u_src, size)`: Copia segura hacia buffers acotados en kernel.
  - `copy_to_user(u_dest, k_src, size)`: Copia segura de resultados hacia userland.
  - `strnlen_user(u_str, max_len)`: Lectura acotada de cadenas que valida la presencia obligatoria de terminador `NUL` sin cruzar páginas inválidas.
- **Kernel Heap (`kmalloc`/`kfree`)**: En [`kernel/lib/kheap.c`](file:///Users/cami/Desktop/NOVA/kernel/lib/kheap.c), asignador dinámico de memoria *First-Fit* con coalescencia de bloques liberados contiguos, ubicado a partir de los 4MB de memoria física.

### 2.2 Seguridad y Control de Acceso Granular (ACL)
- **Principio de Mínimo Privilegio**: En [`kernel/drivers/acl_security.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/acl_security.c) y [`kernel/include/acl_security.h`](file:///Users/cami/Desktop/NOVA/kernel/include/acl_security.h).
- Valida en tiempo real que cada perfil solo ejecute las operaciones autorizadas:
  - `OP_FS_READ`: Lectura de archivos.
  - `OP_FS_WRITE`: Creación/modificación de archivos.
  - `OP_ARDUINO_READ`: Lectura de sensores físicos.
  - `OP_ARDUINO_WRITE`: Activación de actuadores físicos (LED, relés).
  - `OP_SYSTEM_ADMIN`: Instalación de paquetes y administración.
- **Auditoría Activa**: Cualquier intento de acceso denegado se bloquea, incrementa el contador de infracciones y se registra automáticamente en `security.log` dentro de RAMFS.

### 2.3 Almacenamiento y Sistema de Archivos
- **RAMFS (In-Memory Filesystem)**: En [`kernel/drivers/ramfs.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/ramfs.c), estructura para almacenar y consultar archivos en memoria RAM (`ramfs_create`, `ramfs_write`, `ramfs_read`, `ramfs_delete`, `ramfs_list`). Permite el registro y persistencia en sesión de telemetría de Arduino (`telemetria.log`), alertas del sistema (`alertas.log`), registros de violaciones de seguridad (`security.log`) y binarios/metadatos de aplicaciones instaladas (`app_<id>`).

### 2.4 Perfiles de Usuario y Auto-Aprovisionamiento (11 Perfiles)
Zenthra OS implementa **11 perfiles de usuario** especializados. Cada perfil cuenta con su propio lote de aplicaciones que se auto-descargan y registran como metadatos demostrativos en el sistema de archivos RAMFS al seleccionarlo:

| # | Perfil | Herramientas Auto-Aprovisionadas | Propósito |
|---|---|---|---|
| **0** | **General** | `calc`, `media-player` | Uso básico diario, explorador y multimedia |
| **1** | **Estudiante** | `office` (Suite Office), `pdf-reader` (Lector PDF), `flashcards` (Anki Cards) | Estudio, apuntes, lecturas y preparación de exámenes |
| **2** | **Frontend Developer** | `figma` (UI/UX), `vscode` (Editor de Código), `nodejs` (Runtime JS), `dev-browser` | Desarrollo web cliente, prototipado e inspección de interfaces |
| **3** | **Backend Developer** | `docker` (Contenedores), `postgres` (Cliente DB), `api-postman` (APIs REST), `git-cli` | Microservicios, bases de datos y control de versiones |
| **4** | **Fullstack Developer** | `fullstack-sdk` (SDKs C/Rust/Node), `terminal-pro` (Terminal avanzada) | Entorno integral de desarrollo frontend y backend |
| **5** | **Diseñador / Creativo** | `vector-draw` (Ilustración), `photo-editor` (Retoque), `font-manager` (Tipografías) | Edición visual, recursos gráficos y paletas |
| **6** | **Hardware / IoT** | `arduino-ide` (Studio Arduino), `serial-mon` (Analizador UART), `relay-ctl` (Reles) | Conexión, flasheo y experimentación física con microcontroladores |
| **7** | **Data Scientist / IA** | `jupyter-nb` (Notebooks interactivos), `stats-plot` (Gráficos estadísticos) | Análisis exploratorio, ciencia de datos y visualización |
| **8** | **Ciberseguridad** | `net-scanner` (Auditor de puertos), `pkt-analyzer` (Sniffer de red) | Análisis de tráfico de red y auditoría de seguridad |
| **9** | **Ofimática / Oficina** | `calc-pro` (Planillas avanzadas), `invoice-mgr` (Facturación y contabilidad) | Administración comercial y gestión de documentos |
| **10**| **Gamer / Streamer** | `game-launcher` (GameHub optimizado), `stream-obs` (Captura y streaming OBS) | Lanzador de videojuegos y transmisión de pantalla |

> **Nota de Auditoría Técnica**: Las herramientas listadas corresponden a una *demostración arquitectural* de aprovisionamiento por perfiles. Registran metadatos y payloads verificados con CRC32 en RAMFS, modelando el subsistema que integrará binarios ELF nativos en la Fase 6.

### 2.5 Centro de Aplicaciones (Zenthra Store) e Integridad Criptográfica
- **Catálogo de Aplicaciones**: Más de 25 herramientas catalogadas con firmas de verificación (simulación funcional de metadatos en memoria).
- **Verificación Criptográfica de Integridad (Punto 3 de Auditoría)**:
  - Implementación del algoritmo estándar **CRC32 (IEEE 802.3 polinomial `0xEDB88320`)** en [`kernel/drivers/pkg_manager.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/pkg_manager.c) y [`kernel/include/pkg_manager.h`](file:///Users/cami/Desktop/NOVA/kernel/include/pkg_manager.h).
  - Cada paquete contiene su firma esperada (`expected_crc32`). Al invocarse `pkg_install()`, se ejecuta obligatoriamente `pkg_verify_integrity()`. Si los datos fueron alterados o están corrompidos, la instalación es rechazada y se bloquea la escritura en RAMFS.
  - Validación contra el vector canónico estándar IEEE 802.3 (`"123456789"` -> `0xCBF43926`).
  - Validación de escritura en RAMFS: si `ramfs_create()` o `ramfs_write()` fallan (ej. por saturación de espacio), `pkg_install()` revierte y devuelve `-1`.
- **Auto-Aprovisionamiento**: Función `pkg_provision_profile(profile)` que instala en masa las herramientas verificadas al elegir o cambiar de perfil.

### 2.6 Automatización y Motor de Reglas
- **Rules Engine (Motor SI-ENTONCES)**: En [`kernel/drivers/rules_engine.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/rules_engine.c). Evaluación de umbrales térmicos y disparo de actuadores físicos en Arduino.

### 2.7 Controladores y Entrada/Salida
- **VGA Text Mode Driver**: En [`kernel/drivers/vga.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/vga.c), gestión en `0xB8000` con scroll vertical y cursor físico.
- **Serial Driver (UART 16550A)**: En [`kernel/drivers/serial.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/serial.c), configuración en `COM1 (0x3F8)` a 38400 baudios, 8N1.
- **Keyboard Driver (PS/2)**: En [`kernel/drivers/keyboard.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/keyboard.c), decodificación de scan codes a ASCII.

### 2.8 Interfaz de Usuario y Comandos
- `help`: Lista completa de utilidades.
- `perms`: Muestra la matriz de permisos de seguridad asignados al perfil activo.
- `ps`: Muestra la tabla de procesos y tareas en ejecución (PID, nombre, estado, ticks consumidos).
- `profiles`: Presenta los 11 perfiles detallando su propósito.
- `profile <0..10>`: Cambia de entorno y auto-descarga todas las aplicaciones correspondientes.
- `store search | list | rec | install <id> | remove <id>`: Operaciones de tienda protegidas por ACL e integridad CRC32.
- `rules`: Listado de reglas activas de automatización.
- `arduino ping | led on | led off | temp | auto | log | status`: Telemetría e integración Arduino protegidas por permisos.
- `poweroff` / `shutdown`: Apagado limpio de la máquina virtual o emulador mediante puertos ACPI estándar.
- `reboot`: Reinicio en caliente del procesador mediante el controlador de teclado 8042 (puerto `0x64`).

---

### 3. Pruebas Automatizadas y Validación (Auditoría v15)

Se ejecutó la suite de pruebas mediante `make test` (**251 pruebas automatizadas, 0 fallos**):

```
====================================================
     ZENTHRA OS — SUITE DE PRUEBAS AUTOMATIZADAS      
====================================================

[*] Ejecutando: test_klib_strings... (8 pruebas)
[*] Ejecutando: test_kheap_allocator... (6 pruebas)
[*] Ejecutando: test_syscall_security_checks... (16 pruebas: strnlen_user seguro con validación de páginas, SYS_WRITE binario POSIX, SYS_READ con PTE_WRITABLE, buffers multi-página y límites de memoria)
[*] Ejecutando: test_multitasking_scheduler... (10 pruebas: Round-Robin, yield, estrés 40 tareas)
[*] Ejecutando: test_acl_security... (20 pruebas: permisos granulares por proceso, SYS_READ con cola circular de teclado, retorno -EAGAIN interactivo, preservación de teclas ante buffers inválidos, códigos de error POSIX errno.h y prevención de desbordamiento de cola)
[*] Ejecutando: test_ramfs... (9 pruebas: CRUD, rechazo de sobreflujo >1024)
[*] Ejecutando: test_profile_provisioning... (10 pruebas: 11 perfiles, auto-aprovisionamiento)
[*] Ejecutando: test_package_manager... (12 pruebas: CRC32 IEEE 802.3, verificación, rollback)
[*] Ejecutando: test_rules_engine... (3 pruebas: disparadores y logging)
[*] Ejecutando: test_bidirectional_simulation... (4 pruebas: protocolo ZIoT serie)
[*] Ejecutando: test_x86_structures... (3 pruebas: GDT, IDT, TSS 104 bytes)
[*] Ejecutando: test_multiboot_and_power... (11 pruebas: firmas, tags, salvaguardas ACPI)
[*] Ejecutando: test_pmm_physical_allocator... (23 pruebas: 64-bit, alineación, límites, salvaguarda de desbordamiento)
[*] Ejecutando: test_vmm_hardware_paging... (39 pruebas: Identity Mapping exacto a 64 MiB, CR0, CR3, accesibilidad, direct map, aislamiento de proceso, validación fail-closed de memoria de usuario, TLB invalidation)
[*] Ejecutando: test_page_fault_handler... (8 pruebas: decodificación #PF CR2)
[*] Ejecutando: test_pit_and_irq_masks... (10 pruebas: 100 Hz, máscaras PIC)
[*] Ejecutando: test_gfx_and_lfb... (25 pruebas: 1024x768x32bpp, doble buffer, clipping)
[*] Ejecutando: test_mouse_ps2... (17 pruebas: paquetes, botones, límites de pantalla)
[*] Ejecutando: test_window_manager_and_desktop... (18 pruebas: drag, focus, launcher, terminal)

----------------------------------------------------
Resultados: 251 Aprobados | 0 Fallidos
----------------------------------------------------
[+] Todas las pruebas pasaron satisfactoriamente.
```

---

## 4. Pipeline de CI/CD y Validación de Arranque

- **Flujo GitHub Actions (`.github/workflows/ci.yml`)**:
  - `make check-env`: Diagnóstico y reporte de herramientas del entorno.
  - `make test`: Ejecución de las 251 pruebas unitarias sin dependencias externas.
  - `make kernel`: Compilación y enlace del ELF x86 de 32 bits (`i686`).
  - `grub-file`: Verificación estricta del encabezado Multiboot 1.
  - `make iso`: Creación de la imagen arrancable `build/zenthra_os.iso` con GRUB y generación de `build/BUILD_INFO.txt` con SHA-256.
  - `make verify-qemu`: Ejecución de doble verificación automatizada:
    1. `verify-qemu-complete`: Ejecución directa del kernel con bandera `test-mode`, exigiendo la emisión de `[TEST_COMPLETE] ZENTHRA_QEMU_TEST_COMPLETE`, apagado limpio ACPI y finalización con **exit code 0** (no acepta timeouts).
    2. `verify-qemu-smoke`: Arranque de la imagen ISO con GRUB, inicialización gráfica VESA LFB, ratón PS/2, gestor de ventanas y estabilidad del escritorio sin pánicos de CPU.
- **Portabilidad Host macOS / Darwin**:
  - Detección explícita de SO en `Makefile`.
  - En macOS emite mensajes informativos `[BLOCKED]` si faltan herramientas de cross-compilación (`i686-linux-gnu-gcc` o `grub-file`), permitiendo que el host compile y ejecute la suite de pruebas local (`make test`) limpiamente sin fallar por rutas inexistentes.
