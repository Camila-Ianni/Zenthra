# Makefile para NOVA OS
# Arquitectura de destino: i686 (x86 32-bit Protected Mode ELF)

TARGET ?= i686-elf
CC     ?= $(TARGET)-gcc
LD     ?= $(TARGET)-ld
AS     ?= $(TARGET)-as

# Detección de herramientas
ifeq ($(shell which $(CC) 2>/dev/null),)
    ifeq ($(shell which clang 2>/dev/null),)
        $(error "No se encontro ni $(TARGET)-gcc ni clang en el sistema")
    else
        CC = clang -target i386-unknown-none-elf -m32
        LD = ld.lld -m elf_i386
        AS = clang -target i386-unknown-none-elf -m32 -c
    endif
endif

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -fno-builtin -fno-stack-protector -nostdlib \
         -Ikernel/include

LDFLAGS = -T boot/linker.ld -nostdlib

BUILD_DIR = build
BIN_NAME  = $(BUILD_DIR)/nova_kernel.elf

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
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "=========================================="
	@echo " [OK] NOVA Kernel compilado con exito: $@"
	@echo "=========================================="

clean:
	rm -rf $(BUILD_DIR) bin_test

# Ejecutar suite de pruebas lógicas y unitarias nativas
test:
	@echo "=== Compilando y ejecutando Test Suite de NOVA OS ==="
	clang -Wall -Wextra -Ikernel/include tests/mocks/mock_hw.c kernel/lib/klib.c kernel/lib/kheap.c kernel/drivers/task.c kernel/drivers/acl_security.c kernel/drivers/ramfs.c kernel/drivers/rules_engine.c kernel/drivers/pkg_manager.c tests/unit/test_suite.c -o bin_test
	./bin_test

# Regla de prueba con emulador QEMU
qemu: $(BIN_NAME)
	qemu-system-i386 -kernel $(BIN_NAME) -serial stdio

.PHONY: all clean qemu test
