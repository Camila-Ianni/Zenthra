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
| **Arranque y Bootloader** | **REAL** | Soporte Multiboot 1 y 2 con detección de memoria física y validación estricta por `grub-file`. |
| **Segmentación de Memoria (GDT)** | **REAL** | Descriptores de 32 bits de Código y Datos para Kernel (Ring 0) y Userland (Ring 3). |
| **Interrupciones y Excepciones (IDT)** | **REAL** | IDT con 256 descriptores. Stubs en assembly para 32 excepciones de CPU y 16 IRQs PIC 8259 remapeadas (32..47). |
| **Temporizador del Sistema (PIT)** | **REAL** | Intel 8253/8254 programado a 100 Hz (IRQ 0 / Vector 32) con ticks periódicos y sleep por `hlt`. |
| **Puerto Serie (COM1 UART 16550A)** | **REAL** | Inicialización con divisor de baudios (38400), FIFOs y salida serie para diagnóstico de depuración. |
| **Planificador y Multitarea** | **REAL / PARCIAL** | Scheduler Round-Robin con cambio real de contexto en ensamblador (`switch_context`), pilas dedicadas por tarea y tareas alternantes. Paginación por proceso pendiente. |
| **Gestión de Memoria Dinámica (Heap)** | **REAL** | Asignador `kmalloc`/`kfree` de bloques con cabeceras de tamaño, flag de ocupado y coalescencia de fragmentos contiguos (2 MiB iniciales en 0x00400000). |
| **Sistema de Archivos (RAMFS)** | **REAL (Volátil)** | Sistema de archivos en RAM con inodos, operaciones de creación, lectura, escritura y borrado. Almacenamiento persistente en disco pendiente. |
| **Llamadas al Sistema (Syscalls)** | **PROTOTIPO** | Despachador `int 0x80` mediante stub assembly `isr128` con marco `struct registers` e `iret`. Comprobaciones seguras de punteros (`is_user_pointer`, `copy_from_user`, `copy_to_user`). Requiere paging para aislamiento total. |
| **Seguridad y Perfiles de Usuario** | **REAL** | Matriz de control de acceso (ACL) con 11 perfiles de usuario, operaciones diferenciadas y registro en `security.log`. |
| **Gestor de Paquetes / Store** | **SIMULADA** | Catálogo demostrativo de aplicaciones con verificación de integridad por CRC32 (IEEE 802.3). Formato `.zpkg` firmado planificado para Fase 6. |
| **Protocolo de Enlace Arduino (IoT)** | **REAL** | Trama binaria estructurada con delimitador MAGIC (`0xAA55`), comandos (PING, TEMP, LED_ON, LED_OFF) y checksum XOR. |
| **Control de Energía (ACPI / 8042)** | **REAL** | Puertos de apagado ACPI en emulador (`0x604`, `0x0804`, `0x4004`) y reinicio por teclado 8042. |
| **Interfaz de Usuario Actual** | **REAL** | Consola de texto VGA color de 80x25 con prompt interactivo y comandos integrados (modo de rescate). |
| **Interfaz Gráfica de Escritorio (GUI)** | **PENDIENTE** | Diseñada en arquitectura (Fase 5): Framebuffer VESA 1024x768, primitivas 2D, mouse PS/2 y gestor de ventanas. |

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

# Ejecutar suite de pruebas unitarias automatizadas (92 tests)
make test

# Compilar kernel ELF y generar la imagen ISO arrancable
make iso

# Validar el arranque automatizado en QEMU y alternancia de tareas
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
