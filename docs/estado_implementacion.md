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

### 2.2 Controladores y Entrada/Salida
- **VGA Text Mode Driver**: En [`kernel/drivers/vga.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/vga.c), gestión directa del buffer en `0xB8000`, scrolling vertical automático y actualización del cursor físico mediante puertos `0x3D4` y `0x3D5`.
- **Serial Driver (UART 16550A)**: En [`kernel/drivers/serial.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/serial.c), configuración en `COM1 (0x3F8)` a 38400 baudios, 8N1, con verificación previa por loopback.
- **Keyboard Driver (PS/2)**: En [`kernel/drivers/keyboard.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/keyboard.c), decodificación de scan codes a ASCII con modo no bloqueante.

### 2.3 Interfaz de Usuario y Perfiles
- **NOVA Shell**: En [`kernel/drivers/shell.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/shell.c), intérprete de comandos con soporte para:
  - `help`: Lista de comandos disponibles.
  - `info`: Información de arquitectura y versión.
  - `clear`: Limpieza de la pantalla VGA.
  - `profile <0..3>`: Conmutación en tiempo real entre perfiles (`General`, `Estudiante`, `Desarrollador`, `Hardware/IoT`).
  - `arduino ping | led on | led off | temp | status`: Interacción y diagnóstico con la placa conectada.

### 2.4 Integración Física con Arduino
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
[*] Ejecutando: test_bidirectional_simulation...
  [PASS] Trama enviada inicia con MAGIC
  [PASS] Comando enviado es LED_ON
  [PASS] Integridad de trama de respuesta de Arduino confirmada
  [PASS] Deteccion y rechazo exitoso de trama corrompida
[*] Ejecutando: test_x86_structures...
  [PASS] Descriptor GDT tiene tamano estandar x86 (8 bytes)
  [PASS] Descriptor IDT tiene tamano estandar x86 (8 bytes)

----------------------------------------------------
Resultados: 14 Aprobados | 0 Fallidos
----------------------------------------------------
[+] Todas las pruebas pasaron satisfactoriamente.
```

---

## 4. Pipeline de CI/CD
Se configuró un flujo en [`.github/workflows/ci.yml`](file:///Users/cami/Desktop/NOVA/.github/workflows/ci.yml) para ejecutar automáticamente las pruebas y compilaciones en cada push o pull request.
