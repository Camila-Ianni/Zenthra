# Makefile para NOVA OS
# Arquitectura de destino: i686 (x86 32-bit Protected Mode ELF)

# Configuración por defecto usando clang con target freestanding
CC = clang -target i386-unknown-none-elf -m32
AS = clang -target i386-unknown-none-elf -m32 -c

ifneq ($(shell which i686-elf-ld 2>/dev/null),)
    LD = i686-elf-ld
else ifneq ($(shell which ld.lld 2>/dev/null),)
    LD = ld.lld -m elf_i386
else
    LD = false
endif

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-builtin -fno-stack-protector -nostdlib -D__KERNEL__ \
         -Ikernel/include

LDFLAGS = -T boot/linker.ld -nostdlib

BUILD_DIR = build
BIN_NAME  = $(BUILD_DIR)/nova_kernel.elf
ISO_NAME  = $(BUILD_DIR)/nova_os.iso
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

SRCS_S = boot/boot.s

OBJS = $(SRCS_S:boot/%.s=$(BUILD_DIR)/boot_%.o) \
       $(SRCS_C:kernel/%.c=$(BUILD_DIR)/kernel_%.o)

all: $(BIN_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot_%.o: boot/%.s | $(BUILD_DIR)
	$(AS) $< -o $@

$(BUILD_DIR)/kernel_%.o: kernel/%.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_NAME): $(OBJS)
	@if [ "$(LD)" = "false" ]; then \
		echo "[-] Cross-linker i686-elf-ld/ld.lld no disponible localmente en el host macOS."; \
		echo "    Todos los modulos bare-metal fueron compilados a objetos .o exitosamente."; \
		echo "    El enlace ELF e ISO se genera automaticamente en GitHub Actions CI."; \
		exit 1; \
	fi
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "=========================================="
	@echo " [OK] NOVA Kernel compilado con exito: $@"
	@echo "=========================================="

# Creación de imagen ISO arrancable con GRUB (para VirtualBox / QEMU)
iso: $(OBJS)
	@echo "=== Preparando estructura ISO de arranque (VirtualBox / GRUB) ==="
	mkdir -p $(ISO_DIR)/boot/grub
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@if [ -f $(BIN_NAME) ]; then \
		cp $(BIN_NAME) $(ISO_DIR)/boot/nova_kernel.elf; \
		if command -v grub-mkrescue >/dev/null 2>&1; then \
			grub-mkrescue -o $(ISO_NAME) $(ISO_DIR); \
			echo "========================================================"; \
			echo " [OK] Imagen ISO creada: $(ISO_NAME)"; \
			shasum -a 256 $(ISO_NAME) 2>/dev/null || sha256sum $(ISO_NAME); \
			echo "========================================================"; \
		else \
			echo "[-] 'grub-mkrescue' no disponible localmente en macOS."; \
			echo "    Estructura lista en $(ISO_DIR). Generable en Linux / GitHub Actions."; \
		fi; \
	else \
		echo "[-] Se compilaron los objetos ELF x86_32 en $(BUILD_DIR)."; \
		echo "    El enlace final del ELF/ISO se ejecuta en entorno Linux o con i686-elf-ld."; \
	fi

clean:
	rm -rf $(BUILD_DIR) bin_test

# Detección de compilador host para la suite de tests
ifneq ($(shell which clang 2>/dev/null),)
    CC_HOST = clang
else
    CC_HOST = gcc
endif

# Ejecutar suite de pruebas lógicas y unitarias nativas
test:
	@echo "=== Compilando y ejecutando Test Suite de Zenthra OS ==="
	$(CC_HOST) -Wall -Wextra -Ikernel/include tests/mocks/mock_hw.c kernel/lib/klib.c kernel/lib/kheap.c kernel/arch/x86/syscall.c kernel/drivers/task.c kernel/drivers/acl_security.c kernel/drivers/ramfs.c kernel/drivers/rules_engine.c kernel/drivers/pkg_manager.c tests/unit/test_suite.c -o bin_test
	./bin_test

# Regla de prueba con emulador QEMU
qemu: $(BIN_NAME)
	qemu-system-i386 -kernel $(BIN_NAME) -serial stdio

# Regla de arranque desde ISO en QEMU
qemu-iso: iso
	qemu-system-i386 -cdrom $(ISO_NAME) -serial stdio

.PHONY: all clean qemu qemu-iso iso test
