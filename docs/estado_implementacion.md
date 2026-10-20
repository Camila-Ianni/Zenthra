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
- **Syscalls (Int 0x80)**: Despachador en [`kernel/arch/x86/syscall.c`](file:///Users/cami/Desktop/NOVA/kernel/arch/x86/syscall.c) para invocación segura de servicios desde espacio de usuario (`SYS_WRITE`, `SYS_ARDUINO_CMD`).
- **Kernel Heap (`kmalloc`/`kfree`)**: En [`kernel/lib/kheap.c`](file:///Users/cami/Desktop/NOVA/kernel/lib/kheap.c), asignador dinámico de memoria *First-Fit* con coalescencia de bloques liberados contiguos, ubicado a partir de los 4MB de memoria física.

### 2.2 Almacenamiento y Sistema de Archivos
- **RAMFS (In-Memory Filesystem)**: En [`kernel/drivers/ramfs.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/ramfs.c), estructura para almacenar y consultar archivos en memoria RAM (`ramfs_create`, `ramfs_write`, `ramfs_read`, `ramfs_delete`, `ramfs_list`). Permite el registro y persistencia en sesión de telemetría de Arduino (`telemetria.log`), alertas del sistema (`alertas.log`) y binarios/metadatos de aplicaciones instaladas (`app_<id>`).

### 2.3 Centro de Aplicaciones y Gestor de Paquetes
- **NOVA Store ([`pkg_manager.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/pkg_manager.c) / [`pkg_manager.h`](file:///Users/cami/Desktop/NOVA/kernel/include/pkg_manager.h))**: Catálogo modular de aplicaciones con soporte para perfiles:
  - Catálogo con nombre, versión, tamaño, descripción y perfil objetivo (`Estudiante`, `Desarrollador`, `Hardware/IoT`, `General`).
  - Motor de recomendación de paquetes contextual según el perfil activo del usuario.
  - Flujo demostrable de instalación y desinstalación que crea y retira los archivos correspondientes en RAMFS.

### 2.4 Automatización y Motor de Reglas
- **Rules Engine (Motor SI-ENTONCES)**: En [`kernel/drivers/rules_engine.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/rules_engine.c) y [`kernel/include/rules_engine.h`](file:///Users/cami/Desktop/NOVA/kernel/include/rules_engine.h). Permite evaluar en tiempo real condiciones de umbral de sensores y disparar acciones automáticas sobre actuadores (como encendido/apagado de LED/ventilador) y registro de incidencias, cubriendo el requerimiento de automatización de la propuesta.

### 2.5 Controladores y Entrada/Salida
- **VGA Text Mode Driver**: En [`kernel/drivers/vga.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/vga.c), gestión directa del buffer en `0xB8000`, scrolling vertical automático y actualización del cursor físico mediante puertos `0x3D4` y `0x3D5`.
- **Serial Driver (UART 16550A)**: En [`kernel/drivers/serial.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/serial.c), configuración en `COM1 (0x3F8)` a 38400 baudios, 8N1, con verificación previa por loopback.
- **Keyboard Driver (PS/2)**: En [`kernel/drivers/keyboard.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/keyboard.c), decodificación de scan codes a ASCII con modo no bloqueante.

### 2.6 Interfaz de Usuario y Perfiles
- **NOVA Shell**: En [`kernel/drivers/shell.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/shell.c), intérprete de comandos con soporte para:
  - `help`: Lista de comandos disponibles.
  - `info`: Información de arquitectura y versión.
  - `clear`: Limpieza de la pantalla VGA.
  - `mem`: Monitoreo en vivo de memoria usada y libre en el Heap.
  - `ls`: Listado de archivos en RAMFS.
  - `cat <archivo>`: Visualización de archivos en consola.
  - `store search`: Explorador del catálogo completo de NOVA Store.
  - `store list`: Lista de aplicaciones instaladas en el sistema.
  - `store rec`: Muestra recomendaciones de aplicaciones según el perfil del usuario.
  - `store install <id>`: Instala un paquete y registra su entrada en RAMFS.
  - `store remove <id>`: Desinstala un paquete del sistema.
  - `rules`: Listado de reglas de automatización y su contador de activaciones.
  - `profile <0..3>`: Conmutación en tiempo real entre perfiles (`General`, `Estudiante`, `Desarrollador`, `Hardware/IoT`) con recomendaciones automáticas.
  - `arduino ping | led on | led off | temp | auto | log | status`: Interacción, control, automatización y telemetría con la placa.

### 2.7 Integración Física con Arduino
- **Protocolo NOVA-Link**: En [`kernel/include/nova_link.h`](file:///Users/cami/Desktop/NOVA/kernel/include/nova_link.h). Formato de paquete: `[MAGIC (0xAA)] [CMD] [LEN] [PAYLOAD...] [CHECKSUM XOR]`.
- **Servicio `nova_iot`**: En [`kernel/drivers/nova_iot.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/nova_iot.c), centraliza la validación, empaquetado, recepción con timeout y conteo de errores de integridad.
- **Firmware Receptor**: En [`arduino/nova_link/nova_firmware.ino`](file:///Users/cami/Desktop/NOVA/arduino/nova_link/nova_firmware.ino), compatible con Arduino Uno/Nano/Mega para actuar sobre el pin digital 13 y leer sensores analógicos en `A0`.

---

## 3. Pruebas Automatizadas y Validación

Se ejecutó la suite de pruebas unitarias y de simulación mediante `make test`:

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
[*] Ejecutando: test_ramfs...
  [PASS] Archivo bienvenida.txt existe y tiene contenido
  [PASS] Creacion de nuevo archivo sensor_test.dat
  [PASS] Escritura completa de datos de sensor
  [PASS] Lectura de contenido de archivo coincide con lo escrito
  [PASS] Integridad de los datos leidos comprobada
  [PASS] Borrado de archivo exitoso
  [PASS] Archivo borrado ya no puede ser leido
[*] Ejecutando: test_package_manager...
  [PASS] Instalacion exitosa de paquete 'telemetry'
  [PASS] Paquete instalado crea entrada binaria en RAMFS (app_telemetry)
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
Resultados: 35 Aprobados | 0 Fallidos
----------------------------------------------------
[+] Todas las pruebas pasaron satisfactoriamente.
```

---

## 4. Pipeline de CI/CD
Se configuró un flujo en [`.github/workflows/ci.yml`](file:///Users/cami/Desktop/NOVA/.github/workflows/ci.yml) para ejecutar automáticamente las pruebas y compilaciones en cada push o pull request.
