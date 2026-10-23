# Guía de Ejecución y Arranque en VirtualBox — Zenthra OS

## 1. Introducción
Zenthra OS puede compilarse y empaquetarse en una imagen de disco ISO arrancable (`zenthra_os.iso`) compatible con **VirtualBox**, **QEMU** y **VMware**, utilizando el gestor de arranque **GRUB** bajo la especificación **Multiboot** (i686 32-bit Protected Mode).

---

## 2. Requisitos de Construcción de la ISO
Para generar la imagen ISO (`zenthra_os.iso`) se requieren las siguientes herramientas en el sistema:
- `grub-mkrescue` (paquete `grub-common` + `grub-pc-bin`)
- `xorriso`
- `mtools` y `dosfstools` (requeridos internamente por `grub-mkrescue` para dar formato de disquete/arranque El Torito)
- Compilador x86 (`gcc-multilib` o `i686-elf-gcc`)

### Instalación de dependencias:
- **Ubuntu / Debian / Linux (o GitHub Actions CI)**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y gcc-multilib build-essential grub-pc-bin grub-common xorriso qemu-system-x86 clang mtools dosfstools
  ```

---

## 3. Generación Estricta de la ISO

Desde la raíz del proyecto Zenthra:
```bash
make clean
make iso
```

Este comando:
1. Compila todos los módulos del kernel y los stubs de excepciones/IRQ en ensamblador.
2. Enlaza el binario ELF `build/zenthra_kernel.elf`.
3. Valida estrictamente la cabecera Multiboot 1 con `grub-file --is-x86-multiboot`.
4. Prepara la estructura de directorios ISO (`build/isodir/boot/grub/grub.cfg`).
5. Invoca `grub-mkrescue -o build/zenthra_os.iso build/isodir`.
6. Si alguna herramienta falta o la ISO no se genera con tamaño válido (>0 bytes), el comando **falla explícitamente con código de error** (no produce falsos positivos).
7. Calcula e imprime el hash SHA-256 de la imagen generada.

---

## 4. Configuración de la Máquina Virtual en VirtualBox

Para arrancar Zenthra OS en Oracle VM VirtualBox:

1. **Crear Máquina Virtual**:
   - **Name**: `Zenthra OS`
   - **Type**: `Other`
   - **Version**: `Other/Unknown (32-bit)`
2. **Memoria Base (RAM)**:
   - Configurar entre `128 MB` y `512 MB` (Zenthra OS requiere un mínimo de 16 MB para kernel + heap + RAMFS).
3. **Procesador (CPU)**:
   - 1 vCPU (Arquitectura x86 32-bit).
4. **Almacenamiento**:
   - En el controlador IDE / Óptico: Seleccionar **Choose a disk file...** y montar `build/zenthra_os.iso` (o la ISO descargada desde los artefactos del GitHub Actions CI).
5. **Puerto Serie (Opcional pero muy recomendado para diagnóstico)**:
   - Ir a **Settings -> Serial Ports -> Port 1**:
     - Marcar **Enable Serial Port**.
     - **Port Number**: `COM1` (I/O Port `0x3F8`, IRQ `4`).
     - **Port Mode**: `Raw File` y asignar ruta a un archivo `zenthra_serial.log` en el host para capturar los diagnósticos del kernel, ticks de PIT y alternancia de tareas en tiempo real.
6. **Iniciar la Máquina**:
   - Presionar **Start**.
   - Aparecerá el menú de GRUB con la entrada:
     `Zenthra OS (Experimental Academic Edition - 2026)`.
   - Presionar Enter. Verás la inicialización de GDT, IDT (con 32 excepciones y 16 IRQs instaladas), Timer PIT (100 Hz), Scheduler con tareas alternantes, memoria detectada por Multiboot y la consola:
     `zenthra [General]# `

---

## 5. Validación y Verificación de Integridad

Registra el hash de la versión para el anexo de tesis:
```bash
sha256sum build/zenthra_os.iso
```
