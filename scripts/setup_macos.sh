#!/usr/bin/env bash
# Script de preparación del entorno de compilación para NOVA OS en macOS
set -e

echo "=== Verificando dependencias para NOVA OS en macOS ==="

if ! command -v brew &> /dev/null; then
    echo "[-] Homebrew no detectado. Instala Homebrew primero: https://brew.sh"
    exit 1
fi

echo "[*] Instalando herramientas esenciales (QEMU, NASM, LLVM)..."
brew install qemu nasm llvm x86_64-elf-binutils x86_64-elf-gcc || true

echo "[+] Entorno preparado. Puedes compilar con: make"
