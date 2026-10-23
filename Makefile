# Makefile para Zenthra OS
# Arquitectura: i686 (x86 32-bit Protected Mode ELF)

# Detección flexible de Toolchain para host Linux y entornos cruzados
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

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-builtin -fno-stack-protector -nostdlib -D__KERNEL__ \
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
         kernel/lib/klib.c \
         kernel/lib/kheap.c

SRCS_S = boot/boot.s \
         kernel/arch/x86/interrupts.s

OBJS = $(SRCS_S:%.s=$(BUILD_DIR)/%.o) \
       $(SRCS_C:%.c=$(BUILD_DIR)/%.o)

all: $(BIN_NAME)

kernel: $(BIN_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(AS) $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_NAME): $(OBJS)
	@if [ "$(LD)" = "false" ]; then \
		echo "[-] Error: Cross-linker x86 (ld -m elf_i386 o i686-elf-ld) no disponible en este host."; \
		exit 1; \
	fi
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "========================================================"
	@echo " [OK] Zenthra Kernel ELF enlazado exitosamente: $@"
	@echo "========================================================"

check-multiboot: $(BIN_NAME)
	@if command -v grub-file >/dev/null 2>&1; then \
		grub-file --is-x86-multiboot $(BIN_NAME) || (echo "[-] Error: El binario no cumple con la especificacion Multiboot 1." && exit 1); \
		echo "[OK] Cabecera Multiboot 1 validada estrictamente por grub-file."; \
	else \
		echo "[!] Advertencia: grub-file no instalado; omitiendo chequeo de cabecera."; \
	fi

# Creación de imagen ISO arrancable con GRUB (para VirtualBox / QEMU)
$(ISO_NAME): $(BIN_NAME) check-multiboot
	@if ! command -v grub-mkrescue >/dev/null 2>&1; then \
		echo "[-] Error fatal: 'grub-mkrescue' no disponible para generar la ISO."; \
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
	$(CC_HOST) -Wall -Wextra -Ikernel/include tests/mocks/mock_hw.c kernel/lib/klib.c kernel/lib/kheap.c kernel/arch/x86/syscall.c kernel/drivers/task.c kernel/drivers/acl_security.c kernel/drivers/ramfs.c kernel/drivers/rules_engine.c kernel/drivers/pkg_manager.c tests/unit/test_suite.c -o bin_test
	./bin_test

# Verificación automatizada de arranque en QEMU y captura serial
verify-qemu: $(ISO_NAME)
	@echo "=== Verificando arranque de Zenthra OS en QEMU (Headless) ==="
	@if ! command -v qemu-system-i386 >/dev/null 2>&1; then \
		echo "[-] Error: 'qemu-system-i386' no esta disponible en el sistema."; \
		exit 1; \
	fi
	@rm -f $(BUILD_DIR)/serial.log
	@qemu_status=0; timeout 8s qemu-system-i386 -cdrom $(ISO_NAME) -serial file:$(BUILD_DIR)/serial.log -display none || qemu_status=$$?; \
	if [ $$qemu_status -ne 0 ] && [ $$qemu_status -ne 124 ]; then \
		echo "[-] Error fatal: QEMU aborto con fallo anomalo (exit code $$qemu_status)"; \
		exit 1; \
	fi
	@if [ ! -s $(BUILD_DIR)/serial.log ]; then \
		echo "[-] Error: QEMU no genero salida serial o el archivo serial.log esta vacio"; \
		exit 1; \
	fi
	@echo "--- Contenido del Log Serial de Zenthra OS ---"
	@cat $(BUILD_DIR)/serial.log
	@echo "-----------------------------------------------"
	@if grep -q "CPU PANIC" $(BUILD_DIR)/serial.log; then \
		echo "[-] Error: Se detecto un Panic de CPU no controlado en serial.log"; \
		exit 1; \
	fi
	@grep -q "ZENTHRA OS SERIAL LOG" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se encontro el banner serial del kernel" && exit 1)
	@grep -q "Worker A" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registro la ejecucion del Worker A" && exit 1)
	@grep -q "Worker B" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registro la ejecucion del Worker B" && exit 1)
	@grep -q "Interrupcion IRQ0" $(BUILD_DIR)/serial.log || (echo "[-] Error: No se registraron ticks de interrupcion de PIT IRQ0" && exit 1)
	@echo "[+] [PASS] Arranque verificado: Banner presente, PIT IRQ0 activo y alternancia Worker A/B exitosa."

qemu: $(BIN_NAME)
	qemu-system-i386 -kernel $(BIN_NAME) -serial stdio

qemu-iso: iso
	qemu-system-i386 -cdrom $(ISO_NAME) -serial stdio

.PHONY: all kernel check-multiboot clean qemu qemu-iso iso test verify-qemu
