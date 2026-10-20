# Documentación Técnica y Estado de Implementación — NOVA OS

## 1. Resumen de Ejecución
Este documento consolida el estado técnico del sistema operativo experimental **NOVA OS**, implementado bajo la arquitectura x86 en modo protegido de 32 bits, con integración física mediante el protocolo serie **NOVA-Link** hacia microcontroladores Arduino.

---

## 2. Componentes Implementados

### 2.1 Núcleo y Arquitectura x86
- **Multiboot Specification**: Cabecera Multiboot 1 integrada en [`boot/boot.s`](file:///Users/cami/Desktop/NOVA/boot/boot.s) con flags de alineación y mapa de memoria.
- **Linker Script (`linker.ld`)**: Carga el binario a partir de la dirección `0x100000` (1MB), alineando bloques en páginas de 4KB.
- **GDT (Global Descriptor Table)**: Configuración en [`kernel/arch/x86/gdt.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/gdt.c) con 5 descriptores: Nulo, Código Kernel (Ring 0), Datos Kernel (Ring 0), Código Usuario (Ring 3) y Datos Usuario (Ring 3).
- **IDT (Interrupt Descriptor Table)**: Tabla de 256 descriptores en [`kernel/arch/x86/idt.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/idt.c) con remapeo del controlador PIC 8259 (IRQs 0-15 a los vectores 32-47).
- **PIT 8254 (Programmable Interval Timer)**: En [`kernel/drivers/pit.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/pit.c), generador de onda cuadrada a 100 Hz conectado a IRQ0 (Vector 32) para conteo de ticks y función de retardo `pit_sleep(ms)`.
- **Planificador Multitarea (Scheduler & PCB)**: En [`kernel/drivers/task.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/task.c) y [`kernel/include/task.h`](file:///Users/cami/Desktop/NOVA/kernel/include/task.h). Soporte para Bloque de Control de Proceso (PCB), creación dinámica de tareas con pila independiente (`task_create`), alternancia de contexto Round-Robin (`task_yield`), ciclo de vida (`TASK_READY`, `TASK_RUNNING`, `TASK_SLEEPING`, `TASK_TERMINATED`) y tarea de fondo demonio (`iot_daemon`).
- **Syscalls (Int 0x80)**: Despachador en [`kernel/arch/x86/syscall.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/syscall.c) para invocación segura de servicios desde espacio de usuario (`SYS_WRITE`, `SYS_ARDUINO_CMD`).
- **Kernel Heap (`kmalloc`/`kfree`)**: En [`kernel/lib/kheap.c`](file:///Users/cami/Desktop/NOVA/kernel/lib/kheap.c), asignador dinámico de memoria *First-Fit* con coalescencia de bloques liberados contiguos, ubicado a partir de los 4MB de memoria física.

### 2.2 Almacenamiento y Sistema de Archivos
- **RAMFS (In-Memory Filesystem)**: En [`kernel/drivers/ramfs.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/ramfs.c), estructura para almacenar y consultar archivos en memoria RAM (`ramfs_create`, `ramfs_write`, `ramfs_read`, `ramfs_delete`, `ramfs_list`). Permite el registro y persistencia en sesión de telemetría de Arduino (`telemetria.log`), alertas del sistema (`alertas.log`) y binarios/metadatos de aplicaciones instaladas (`app_<id>`).

### 2.3 Perfiles de Usuario y Auto-Aprovisionamiento (11 Perfiles)
NOVA OS implementa **11 perfiles de usuario** especializados. Cada perfil cuenta con su propio lote de aplicaciones que se auto-descargan e instalan automáticamente en el sistema de archivos al seleccionarlo:

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

### 2.4 Centro de Aplicaciones (NOVA Store)
- Catálogo ampliado a más de 25 herramientas nativas.
- Función `pkg_provision_profile(profile)` que instala en masa las herramientas al elegir o cambiar de perfil.

### 2.5 Automatización y Motor de Reglas
- **Rules Engine (Motor SI-ENTONCES)**: En [`kernel/drivers/rules_engine.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/rules_engine.c). Evaluación de umbrales térmicos y disparo de actuadores físicos en Arduino.

### 2.6 Controladores y Entrada/Salida
- **VGA Text Mode Driver**: En [`kernel/drivers/vga.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/vga.c), gestión en `0xB8000` con scroll vertical y cursor físico.
- **Serial Driver (UART 16550A)**: En [`kernel/drivers/serial.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/serial.c), configuración en `COM1 (0x3F8)` a 38400 baudios, 8N1.
- **Keyboard Driver (PS/2)**: En [`kernel/drivers/keyboard.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/keyboard.c), decodificación de scan codes a ASCII.

### 2.7 Interfaz de Usuario y Comandos
- `help`: Lista completa de utilidades.
- `ps`: Muestra la tabla de procesos y tareas en ejecución (PID, nombre, estado, ticks consumidos).
- `profiles`: Presenta los 11 perfiles detallando su propósito.
- `profile <0..10>`: Cambia de entorno y auto-descarga todas las aplicaciones correspondientes.
- `store search | list | rec | install <id> | remove <id>`: Operaciones de tienda.
- `rules`: Listado de reglas activas de automatización.
- `arduino ping | led on | led off | temp | auto | log | status`: Telemetría e integración Arduino.

---

## 3. Pruebas Automatizadas y Validación

Se ejecutó la suite de pruebas mediante `make test`:

```
====================================================
      NOVA OS — SUITE DE PRUEBAS AUTOMATIZADAS      
====================================================

[*] Ejecutando: test_klib_strings...
  [PASS] k_strlen calcula longitud correcta
  [PASS] k_strcmp igualdad
  [PASS] k_strcmp desigualdad
  [PASS] k_strncmp prefijo
  [PASS] k_memset rellena buffer
  [PASS] k_memcpy copia memoria
  [PASS] uitoa base 10 correcta
  [PASS] uitoa base 16 hexadecimal correcta
[*] Ejecutando: test_kheap_allocator...
  [PASS] Heap inicializado con memoria libre disponible
  [PASS] kmalloc asigna 64 bytes correctamente
  [PASS] El contador de memoria ocupada se incrementa
  [PASS] kmalloc asigna un segundo bloque contiguo y diferenciado
  [PASS] kfree libera y decrementa memoria ocupada
  [PASS] Coalescencia de bloques contiguos restaura el espacio original
[*] Ejecutando: test_multitasking_scheduler...
  [PASS] Obtencion de la tarea actual en ejecucion
  [PASS] La tarea principal del kernel esta en estado RUNNING
  [PASS] Nombre de la tarea 0 es kernel_shell
  [PASS] Creacion exitosa de tarea worker_task con PID valido
  [PASS] task_yield conmuta al worker_task
  [PASS] Nueva tarea conmutada a estado RUNNING
  [PASS] task_yield regresa a la tarea principal kernel_shell (Round-Robin)
[*] Ejecutando: test_ramfs...
  [PASS] Archivo bienvenida.txt existe y tiene contenido
  [PASS] Creacion de nuevo archivo sensor_test.dat
  [PASS] Escritura completa de datos de sensor
  [PASS] Lectura de contenido de archivo coincide con lo escrito
  [PASS] Integridad de los datos leidos comprobada
  [PASS] Borrado de archivo exitoso
  [PASS] Archivo borrado ya no puede ser leido
[*] Ejecutando: test_profile_provisioning...
  [PASS] Existen al menos 11 perfiles de usuario definidos
  [PASS] Perfil Estudiante aprovisiona 3 apps de estudio
  [PASS] App 'office' presente en RAMFS tras seleccion de perfil Estudiante
  [PASS] Perfil Frontend aprovisiona 4 herramientas (figma, vscode, nodejs, browser)
  [PASS] App 'figma' presente en RAMFS
  [PASS] App 'vscode' presente en RAMFS
  [PASS] Perfil Backend aprovisiona 4 herramientas (docker, postgres, api-postman, git)
  [PASS] App 'docker' presente en RAMFS
  [PASS] Perfil Gamer aprovisiona suite de juegos y OBS
  [PASS] App 'stream-obs' presente en RAMFS
[*] Ejecutando: test_package_manager...
  [PASS] Instalacion exitosa de paquete 'arduino-ide'
  [PASS] Paquete instalado crea entrada binaria en RAMFS (app_arduino-ide)
  [PASS] Deteccion de aplicacion ya instalada
  [PASS] Desinstalacion exitosa de paquete
  [PASS] Entrada de app retirada de RAMFS tras desinstalacion
[*] Ejecutando: test_rules_engine...
  [PASS] Temperatura normal (25.0 C) no activa alarma
  [PASS] Temperatura critica (32.5 C) activa actuador LED por automatizacion
  [PASS] Registro de alerta guardado en RAMFS (alertas.log)
[*] Ejecutando: test_bidirectional_simulation...
  [PASS] Trama enviada inicia con MAGIC
  [PASS] Comando enviado es LED_ON
  [PASS] Integridad de trama de respuesta de Arduino confirmada
  [PASS] Deteccion y rechazo exitoso de trama corrompida
[*] Ejecutando: test_x86_structures...
  [PASS] Descriptor GDT tiene tamano estandar x86 (8 bytes)
  [PASS] Descriptor IDT tiene tamano estandar x86 (8 bytes)

----------------------------------------------------
Resultados: 52 Aprobados | 0 Fallidos
----------------------------------------------------
[+] Todas las pruebas pasaron satisfactoriamente.
```
