# Guía de Ejecución y Arranque en VirtualBox — NOVA OS

## 1. Introducción
NOVA OS puede compilarse y empaquetarse en una imagen de disco ISO arrancable compatible con **VirtualBox**, **VMware** y **QEMU**, utilizando el gestor de arranque **GRUB** bajo la especificación **Multiboot**.

---

## 2. Requisitos de Construcción de la ISO
Para generar la imagen ISO (`nova_os.iso`) en tu entorno local se requieren las siguientes herramientas:
- `grub-mkrescue` (o `grub2-mkrescue`)
- `xorriso`
- Compilador cruzado x86 (e.g., `i686-elf-gcc` o `clang -target i386-unknown-none-elf`)

### Instalación de dependencias:
- **macOS (Homebrew)**:
  ```bash
  brew install xorriso qemu
  # Si utilizas Linux en contenedor o máquina virtual:
  ```
- **Ubuntu / Debian / Linux**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y gcc-multilib build-essential grub-pc-bin grub-common xorriso qemu-system-x86
  ```

---

## 3. Generación de la ISO

Desde la raíz del proyecto NOVA:
```bash
make iso
```

Este comando:
1. Compila todos los módulos del kernel en el binario ELF `build/nova_kernel.elf`.
2. Valida la cabecera Multiboot integrada.
3. Prepara la estructura de directorios ISO (`isodir/boot/grub/grub.cfg`).
4. Genera la imagen arrancable `build/nova_os.iso` y calcula su hash SHA-256 para verificación de integridad.

---

## 4. Configuración de la Máquina Virtual en VirtualBox

Para arrancar NOVA OS en VirtualBox, crea una máquina virtual con la siguiente configuración:

1. **Tipo de Sistema Operativo**:
   - **Type**: `Other`
   - **Version**: `Other/Unknown (32-bit)`
2. **Memoria Base (RAM)**:
   - Configura entre `64 MB` y `512 MB` (NOVA OS requiere un mínimo de 16 MB).
3. **Procesador (CPU)**:
   - 1 vCPU (Suficiente para el scheduler Round-Robin y tareas cooperativas).
4. **Almacenamiento**:
   - No requiere disco duro obligatorio para el arranque inicial (corre desde RAM con RAMFS).
   - En el controlador IDE / Óptico: Selecciona **Choose a disk file...** y monta el archivo `build/nova_os.iso`.
5. **Configuración de Red / Puertos Serie (Opcional para IoT)**:
   - Ve a **Settings -> Serial Ports -> Port 1**:
     - Marcar **Enable Serial Port**.
     - **Port Mode**: `Disconnected` o `Raw File` / `Host Pipe` si deseas redirigir la comunicación con una placa Arduino conectada por USB a través de un puente serie COM.
6. **Iniciar la Máquina**:
   - Presiona **Start**.
   - Aparecerá el menú de GRUB con la entrada:
     `NOVA OS (Experimental Academic Edition - 2026)`.
   - Presiona Enter y verás el banner VGA, la inicialización de la GDT/IDT/Syscalls/Heap/RAMFS y el prompt interactivo:
     `nova [General]# `

---

## 5. Validación y Verificación de Integridad

Al generar la ISO, puedes registrar el hash de la versión para el anexo de tesis:
```bash
shasum -a 256 build/nova_os.iso
```
