# Zenthra OS — Estado Técnico y Matriz de Auditoría

> **Fecha de Actualización**: 22 de octubre de 2026  
> **Versión**: 4.1 (Post-Auditoría Técnica v4)

---

## 1. Criterios de Clasificación

Para garantizar rigor académico y de ingeniería de software en el proyecto de grado, cada subsistema se evalúa según su implementación técnica real:

* **REAL**: Código bare-metal x86 operativo, probado en hardware/emulador y respaldado por tests unitarios automatizados.
* **PARCIAL**: Arquitectura implementada con funcionalidades básicas operativas, sujeta a ampliación (ej. segmentación funcional sin paginación por proceso).
* **SIMULADA**: Lógica en memoria utilizada como maqueta demostrativa para perfiles o emulaciones sin persistencia en disco ni formato binario ejecutable final.
* **PENDIENTE**: Subsistema no iniciado, especificado en la hoja de ruta (*Roadmap*).

---

## 2. Matriz de Estado por Subsistema

| Subsistema / Módulo | Estado | Componentes Reales | Limitaciones Actuales / Siguiente Paso |
| :--- | :---: | :--- | :--- |
| **Bootloader & Multiboot** | **REAL** | Encabezado Multiboot 1 y 2, `grub-file --is-x86-multiboot`, lectura de memoria baja y alta. | Paginación y mapas de frames físicos pendientes. |
| **GDT (Segmentación)** | **REAL** | Segmentos de Código y Datos para Kernel (0x08, 0x10) y Usuario (0x18, 0x20). | Falta TSS (Task State Segment) con ESP0 para cambios de privilegio Ring 3->Ring 0. |
| **IDT & Manejo de Interrupciones** | **REAL** | Tabla de 256 descriptores, 32 stubs de excepciones CPU (con/sin código de error), 16 stubs de IRQs PIC remapeadas (32..47) y handler Int 0x80. | Diagnóstico con volcado de registros en COM1 implementado. |
| **Temporizador (PIT 8254)** | **REAL** | 100 Hz (10 ms por tick), interrupción IRQ0 con callback, función `pit_sleep` con instrucción `hlt`. | Sincronización de tareas periódicas y temporizadores dinámicos por software. |
| **Puerto Serie (UART 16550A)** | **REAL** | COM1 (I/O 0x3F8), inicialización con divisor de baudios (38400), registro FIFO, transmisión de cadenas. | Recepción por interrupciones (IRQ 4) sujeta a integración en buffer circular. |
| **Multitarea y Conmutación de Contexto** | **REAL / PARCIAL** | Scheduler Round-Robin, PCB (`struct task`), cambio real de contexto en assembly (`switch_context`), pilas separadas por tarea. | Pruebas de alternancia con dos workers en QEMU completadas. Paginación por proceso pendiente. |
| **Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques contiguos con cabeceras de tamaño, flag de ocupación y fusión (*coalescing*). | Tamaño fijo inicial de 2 MiB (0x00400000). Expansión dinámica mediante páginas virtuales pendiente. |
| **Llamadas al Sistema (Syscalls)** | **PARCIAL** | Despachador `int 0x80`, validación no desbordante de punteros (`is_user_pointer`, `copy_from_user`, `copy_to_user`). | Requiere paginación de hardware (CR0.PG) para aislamiento criptográfico/espacio de direcciones Ring 3. |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil)** | 32 inodos, nombres de hasta 32 caracteres, operaciones de creación, lectura, escritura y borrado en RAM. | No persiste tras reiniciar. El driver de bloque IDE/AHCI está planificado para Fase 7. |
| **Seguridad y Control de Acceso (ACL)** | **REAL** | Matriz de permisos para 11 perfiles, funciones `acl_check_permission`, registro de auditoría en `security.log`. | Permisos aplicados en llamadas del kernel y shell; falta aislamiento forzado por MMU. |
| **Catálogo y Gestor de Paquetes** | **SIMULADA** | Catálogo en memoria de herramientas por perfil, verificación de integridad accidental por CRC32 IEEE 802.3. | Los paquetes actuales son marcadores de prueba. Formato binario `.zpkg` firmado con Ed25519 en Fase 6. |
| **Protocolo de Enlace Arduino (IoT)** | **REAL** | Trama binaria MAGIC `0xAA55`, comandos PING, TEMP, LED_ON, LED_OFF y validación XOR. | Mock funcional en suite y soporte serie físico por COM1 en hardware real. |
| **Gestión de Energía (ACPI / 8042)** | **REAL** | Puertos de apagado ACPI en QEMU/VirtualBox (`0x604`, `0x0804`, `0x4004`) y reinicio por microcontrolador de teclado 8042. | ACPI DSDT/SSDT completo para PCs modernas en Fase 8. |
| **Interfaz VGA Modo Texto** | **REAL** | Búfer VGA `0xB8000`, 80x25 caracteres, cursor hardware, scrolling y shell interactiva con historial. | Diseñada para mantenerse como consola de rescate y modo de depuración. |
| **Interfaz Gráfica (GUI Desktop)** | **PENDIENTE** | Especificación técnica completa en `docs/DESKTOP_MVP.md`. | Framebuffer VESA 1024x768, primitivas 2D, mouse PS/2 y gestor de ventanas en Fase 5. |

---

## 3. Estado de la Suite de Pruebas

- **Total de Pruebas Automatizadas**: 82 ejecutadas en cada build (`make test`).
- **Tasa de Aprobación**: 100% (82 Aprobados, 0 Fallidos).
- **Entorno de Verificación**:
  - Pruebas unitarias de lógica: Ejecutadas nativamente con Clang / GCC.
  - Arranque bare-metal: Verificado automáticamente en QEMU headless con captura de log serie en CI (`make verify-qemu`).
