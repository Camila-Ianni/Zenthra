# Makefile para Zenthra OS
# Arquitectura: i686 (x86 32-bit Protected Mode ELF)

# Detección flexible de Toolchain para host Linux y entornos cruzados
ifeq ($(shell uname -s),Darwin)
    ifneq ($(shell which i686-elf-gcc 2>/dev/null),)
        CC = i686-elf-gcc
        AS = i686-elf-as
        LD = i686-elf-ld
    else
        CC = false
        AS = false
        LD = false
    endif
else
    ifneq ($(shell which i686-elf-gcc 2>/dev/null),)
        CC = i686-elf-gcc
        AS = i686-elf-as
        LD = i686-elf-ld
    else ifneq ($(shell which gcc 2>/dev/null),)
        CC = gcc -m32
        AS = gcc -m32 -c
        LD = ld -m elf_i386
    else ifneq ($(shell which clang 2>/dev/null),)
        CC = clang -target i386-unknown-none-elf -m32
        AS = clang -target i386-unknown-none-elf -m32 -c
        ifneq ($(shell which ld.lld 2>/dev/null),)
            LD = ld.lld -m elf_i386
        else
            LD = false
        endif
    else
        CC = false
        AS = false
        LD = false
    endif
endif

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-builtin -fno-stack-protector -fno-pie -fno-pic -nostdlib -D__KERNEL__ \
         -Ikernel/include

LDFLAGS = -T boot/linker.ld -nostdlib

BUILD_DIR = build
BIN_NAME  = $(BUILD_DIR)/zenthra_kernel.elf
ISO_NAME  = $(BUILD_DIR)/zenthra_os.iso
ISO_DIR   = $(BUILD_DIR)/isodir

SRCS_C = kernel/kernel.c \
         kernel/arch/x86/gdt.c \
         kernel/arch/x86/idt.c \
         kernel/arch/x86/syscall.c \
         kernel/drivers/vga.c \
         kernel/drivers/serial.c \
         kernel/drivers/pit.c \
         kernel/drivers/task.c \
         kernel/drivers/acl_security.c \
         kernel/drivers/ramfs.c \
         kernel/drivers/pkg_manager.c \
         kernel/drivers/rules_engine.c \
         kernel/drivers/keyboard.c \
         kernel/drivers/nova_iot.c \
         kernel/drivers/shell.c \
         kernel/drivers/pmm.c \
         kernel/drivers/vmm.c \
         kernel/drivers/page_fault.c \
         kernel/drivers/gfx.c \
         kernel/drivers/mouse.c \
         kernel/drivers/window_manager.c \
         kernel/drivers/vfs.c \
         kernel/lib/klib.c \
         kernel/lib/kheap.c \
         kernel/lib/slab.c

SRCS_S = boot/boot.s \
         kernel/arch/x86/interrupts.s

OBJS = $(SRCS_S:%.s=$(BUILD_DIR)/%.o) \
       $(SRCS_C:%.c=$(BUILD_DIR)/%.o)

all: $(BIN_NAME)

kernel: $(BIN_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@if [ "$(AS)" = "false" ]; then \
		echo "[BLOCKED] Ensamblador x86 bare-metal no disponible en este host (macOS Darwin)."; \
		echo "          La compilacion del kernel ELF se ejecuta de forma reproducible en Ubuntu CI."; \
		exit 1; \
	fi
	$(AS) $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@if [ "$(CC)" = "false" ]; then \
		echo "[BLOCKED] Compilador C x86 bare-metal no disponible en este host (macOS Darwin)."; \
		echo "          La compilacion del kernel ELF se ejecuta de forma reproducible en Ubuntu CI."; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_NAME): $(OBJS)
	@if [ "$(LD)" = "false" ]; then \
		echo "[BLOCKED] Cross-linker x86 (ld -m elf_i386 o i686-elf-ld) no disponible en este host (macOS Darwin)."; \
		echo "          El enlace del kernel ELF se ejecuta de forma reproducible en Ubuntu CI."; \
		exit 1; \
	fi
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "========================================================"
	@echo " [OK] Zenthra Kernel ELF enlazado exitosamente: $@"
	@echo "========================================================"

check-env:
	@echo "=== Diagnostico de Herramientas de Construccion Zenthra OS ==="
	@printf "  Compilador C Host (%s): " "$(CC_HOST)"
	@command -v $(CC_HOST) >/dev/null 2>&1 && echo "[OK]" || echo "[FALTA]"
	@printf "  Compilador C Kernel (%s): " "$(CC)"
	@if [ "$(CC)" != "false" ]; then echo "[OK]"; else echo "[BLOCKED en host Darwin - Requiere i686-elf-gcc o Linux CI]"; fi
	@printf "  Ensamblador Kernel (%s): " "$(AS)"
	@if [ "$(AS)" != "false" ]; then echo "[OK]"; else echo "[BLOCKED en host Darwin - Requiere i686-elf-as o Linux CI]"; fi
	@printf "  Enlazador Kernel (%s): " "$(LD)"
	@if [ "$(LD)" != "false" ]; then echo "[OK]"; else echo "[BLOCKED en host Darwin - Requiere i686-elf-ld o Linux CI]"; fi
	@printf "  Validador Multiboot 1 (grub-file): "
	@command -v grub-file >/dev/null 2>&1 && echo "[OK]" || echo "[BLOCKED en host local - Se valida en CI]"
	@printf "  Generador ISO (grub-mkrescue): "
	@command -v grub-mkrescue >/dev/null 2>&1 && echo "[OK]" || echo "[BLOCKED en host local - Se valida en CI]"
	@printf "  Herramienta ISO (xorriso): "
	@command -v xorriso >/dev/null 2>&1 && echo "[OK]" || echo "[FALTA]"
	@printf "  Emulador de Sistema (qemu-system-i386): "
	@command -v qemu-system-i386 >/dev/null 2>&1 && echo "[OK]" || echo "[BLOCKED en host local - Se valida en CI]"
	@echo "=============================================================="

check-multiboot: $(BIN_NAME)
	@if command -v grub-file >/dev/null 2>&1; then \
		grub-file --is-x86-multiboot $(BIN_NAME) || (echo "[-] Error fatal: El binario no cumple con la especificacion Multiboot 1." && exit 1); \
		echo "[OK] Cabecera Multiboot 1 validada estrictamente por grub-file."; \
	else \
		echo "[BLOCKED] 'grub-file' no esta instalado en este host (entorno macOS / auditoria)."; \
		echo "          La validacion Multiboot 1 y generacion de ISO se ejecutan en Ubuntu / CI."; \
		echo "          Para instalar localmente en Linux: sudo apt-get install grub-common"; \
		exit 1; \
	fi

# Creación de imagen ISO arrancable con GRUB (para VirtualBox / QEMU)
$(ISO_NAME): $(BIN_NAME) check-multiboot
	@if ! command -v grub-mkrescue >/dev/null 2>&1; then \
		echo "[BLOCKED] 'grub-mkrescue' no disponible en este host para generar la ISO."; \
		echo "          La ISO se genera de forma determinista y reproducible en el pipeline CI."; \
		exit 1; \
	fi
	@echo "=== Preparando estructura ISO de arranque (VirtualBox / GRUB) ==="
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(BIN_NAME) $(ISO_DIR)/boot/zenthra_kernel.elf
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_NAME) $(ISO_DIR)
	@if [ ! -s $(ISO_NAME) ]; then \
		echo "[-] Error: La imagen ISO no fue generada o tiene tamano 0 bytes."; \
		exit 1; \
	fi
	@echo "========================================================"
	@echo " [OK] Imagen ISO creada exitosamente: $(ISO_NAME)"
	@sha256sum $(ISO_NAME) 2>/dev/null || shasum -a 256 $(ISO_NAME)
	@echo "========================================================"
	@echo "Generando manifiesto de verificacion build/BUILD_INFO.txt..."
	@echo "Zenthra OS Build Information" > $(BUILD_DIR)/BUILD_INFO.txt
	@echo "Date: $$(date -u)" >> $(BUILD_DIR)/BUILD_INFO.txt
	@echo "Commit: $$(git rev-parse HEAD 2>/dev/null || echo 'unknown')" >> $(BUILD_DIR)/BUILD_INFO.txt
	@echo "ELF Binary: $(BIN_NAME)" >> $(BUILD_DIR)/BUILD_INFO.txt
	@echo -n "ELF SHA-256: " >> $(BUILD_DIR)/BUILD_INFO.txt
	@sha256sum $(BIN_NAME) 2>/dev/null | awk '{print $$1}' >> $(BUILD_DIR)/BUILD_INFO.txt || shasum -a 256 $(BIN_NAME) | awk '{print $$1}' >> $(BUILD_DIR)/BUILD_INFO.txt
	@echo "ISO Image: $(ISO_NAME)" >> $(BUILD_DIR)/BUILD_INFO.txt
	@echo -n "ISO SHA-256: " >> $(BUILD_DIR)/BUILD_INFO.txt
	@sha256sum $(ISO_NAME) 2>/dev/null | awk '{print $$1}' >> $(BUILD_DIR)/BUILD_INFO.txt || shasum -a 256 $(ISO_NAME) | awk '{print $$1}' >> $(BUILD_DIR)/BUILD_INFO.txt

iso: $(ISO_NAME)

clean:
	rm -rf $(BUILD_DIR) bin_test

# Detección de compilador host para la suite de tests
ifneq ($(shell which clang 2>/dev/null),)
    CC_HOST = clang
else
    CC_HOST = gcc
endif

# Ejecutar suite de pruebas unitarias y de subsistemas
test:
	@echo "=== Compilando y ejecutando Test Suite de Zenthra OS ==="
	$(CC_HOST) -Wall -Wextra -Ikernel/include tests/mocks/mock_hw.c kernel/lib/klib.c kernel/lib/kheap.c kernel/lib/slab.c kernel/arch/x86/syscall.c kernel/drivers/keyboard.c kernel/drivers/task.c kernel/drivers/acl_security.c kernel/drivers/ramfs.c kernel/drivers/rules_engine.c kernel/drivers/pkg_manager.c kernel/drivers/pmm.c kernel/drivers/vmm.c kernel/drivers/page_fault.c kernel/drivers/gfx.c kernel/drivers/mouse.c kernel/drivers/window_manager.c kernel/drivers/vfs.c tests/unit/test_suite.c -o bin_test
	./bin_test

# Verificación 1: Prueba de finalización estricta (test-complete)
# Ejecuta en modo test-mode con kernel ELF directo, exige finalización de workers,
# emisión del marcador final y apagado limpio ACPI (exit code 0). NO acepta timeouts.
verify-qemu-complete: $(BIN_NAME)
	@echo "=== Verificando prueba de finalizacion estricta (test-complete) en QEMU ==="
	@if ! command -v qemu-system-i386 >/dev/null 2>&1; then \
		echo "[BLOCKED] 'qemu-system-i386' no esta disponible en este host."; \
		echo "          El arranque en QEMU se valida de forma automatica en GitHub Actions CI."; \
		exit 1; \
	fi
	@rm -f $(BUILD_DIR)/serial.log
	@qemu_status=0; timeout 15s qemu-system-i386 -no-reboot -kernel $(BIN_NAME) -append "test-mode" -serial file:$(BUILD_DIR)/serial.log -display none || qemu_status=$$?; \
	if [ -s $(BUILD_DIR)/serial.log ]; then \
		echo "--- Log Serial de Finalizacion Estricta (test-complete) ---"; \
		cat $(BUILD_DIR)/serial.log; \
		echo "------------------------------------------------------------"; \
	fi; \
	if [ $$qemu_status -eq 124 ]; then \
		echo "[-] Error fatal: QEMU expiro por timeout (15s) sin completar la prueba ni apagarse limpiamente."; \
		exit 1; \
	elif [ $$qemu_status -ne 0 ]; then \
		echo "[-] Error fatal: QEMU aborto con codigo anomalo (exit code $$qemu_status)"; \
		exit 1; \
	fi
	@if [ ! -s $(BUILD_DIR)/serial.log ]; then \
		echo "[-] Error: QEMU no genero salida serial o el archivo serial.log esta vacio"; \
		exit 1; \
	fi
	@if grep -q "CPU PANIC" $(BUILD_DIR)/serial.log; then \
		echo "[-] Error: Se detecto un Panic de CPU no controlado en serial.log"; \
		exit 1; \
	fi
	@grep -q "ZENTHRA OS SERIAL LOG" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro el banner serial del kernel" && exit 1)
	@grep -q "PMM" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro inicializacion de PMM en serial.log" && exit 1)
	@grep -q "VMM" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro activacion de Paginacion x86 en serial.log" && exit 1)
	@grep -q "VMM_FIXUP_QEMU_PASS" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se supero la prueba de #PF real con redireccion de EIP (fixup) en QEMU" && exit 1)
	@grep -q "Worker A" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registro la ejecucion del Worker A" && exit 1)
	@grep -q "Worker B" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registro la ejecucion del Worker B" && exit 1)
	@grep -q "Worker C (CPU-bound)" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registro la ejecucion de Worker C CPU-bound" && exit 1)
	@grep -q "Interrupcion IRQ0" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registraron ticks de interrupcion de PIT IRQ0" && exit 1)
	@grep -q "ZENTHRA_QEMU_TEST_COMPLETE" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro el marcador final ZENTHRA_QEMU_TEST_COMPLETE" && exit 1)
	@grep -q "Apagado limpio ACPI" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registro el apagado ACPI limpio en serial.log" && exit 1)
	@echo "[+] [PASS] Prueba test-complete superada: Kernel completado, #PF fixup real verificado y apagado limpiamente con exit code 0 (sin timeouts)."

# Verificación 2: Prueba de humo de arranque ISO y GUI viva (boot-smoke)
# Comprueba que la imagen ISO arranque con GRUB, cargue el framebuffer VESA, mouse,
# gestor de ventanas y mantenga la ejecución viva de forma estable sin crasheos.
verify-qemu-smoke: $(ISO_NAME)
	@echo "=== Verificando prueba de humo de arranque ISO y GUI (boot-smoke) en QEMU ==="
	@if ! command -v qemu-system-i386 >/dev/null 2>&1; then \
		echo "[BLOCKED] 'qemu-system-i386' no esta disponible en este host."; \
		exit 1; \
	fi
	@rm -f $(BUILD_DIR)/serial.log
	@qemu_status=0; timeout 8s qemu-system-i386 -vga std -cdrom $(ISO_NAME) -serial file:$(BUILD_DIR)/serial.log -display none || qemu_status=$$?; \
	if [ $$qemu_status -ne 0 ] && [ $$qemu_status -ne 124 ]; then \
		echo "[-] Error fatal: QEMU aborto con fallo anomalo (exit code $$qemu_status)"; \
		exit 1; \
	fi
	@if [ ! -s $(BUILD_DIR)/serial.log ]; then \
		echo "[-] Error: QEMU no genero salida serial o el archivo serial.log esta vacio"; \
		exit 1; \
	fi
	@echo "--- Log Serial de Prueba de Humo ISO / GUI (boot-smoke) ---"
	@cat $(BUILD_DIR)/serial.log
	@echo "------------------------------------------------------------"
	@if grep -q "CPU PANIC" $(BUILD_DIR)/serial.log; then \
		echo "[-] Error: Se detecto un Panic de CPU no controlado en serial.log"; \
		exit 1; \
	fi
	@grep -q "ZENTHRA OS SERIAL LOG" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro el banner serial del kernel" && exit 1)
	@grep -q "Framebuffer VESA LFB inicializado" $(BUILD_DIR)/serial.log || (echo "[-] Error: GRUB no inicializo el Framebuffer VESA LFB en serial.log" && exit 1)
	@grep -q "MOUSE" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro inicializacion de MOUSE en serial.log" && exit 1)
	@grep -q "WM" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro inicializacion de Window Manager en serial.log" && exit 1)
	@grep -q "GUI_DESKTOP_QEMU_PASS" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se verifico el escritorio grafico VESA LFB (GUI_DESKTOP_QEMU_PASS) en serial.log" && exit 1)
	@echo "[+] [PASS] Prueba boot-smoke superada: ISO arrancable con GRUB, Framebuffer VESA LFB 1024x768x32bpp, raton y escritorio nativo activos."

# Verificación integral: combina test-complete estricto y boot-smoke
verify-qemu: verify-qemu-complete verify-qemu-smoke
	@echo "=========================================================================="
	@echo " [+] [PASS] Todas las verificaciones QEMU (test-complete y boot-smoke) OK."
	@echo "=========================================================================="

verify-qemu-direct: verify-qemu-complete

qemu: $(BIN_NAME)
	qemu-system-i386 -kernel $(BIN_NAME) -serial stdio

qemu-iso: iso
	qemu-system-i386 -cdrom $(ISO_NAME) -serial stdio

.PHONY: all kernel check-env check-multiboot clean qemu qemu-iso iso test verify-qemu verify-qemu-complete verify-qemu-smoke verify-qemu-direct
