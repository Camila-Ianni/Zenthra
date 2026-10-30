#!/usr/bin/env bash
set -e

# Script para compilar, generar la ISO y validar Zenthra OS en un contenedor oficial Ubuntu
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "============================================================"
echo "    ZENTHRA OS — Entorno de Construccion y QEMU en Ubuntu    "
echo "============================================================"

if ! command -v docker >/dev/null 2>&1; then
    echo "[-] Error: Docker no esta instalado o no se encuentra en el PATH."
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "[*] Iniciando Docker Desktop..."
    open -a Docker || true
    sleep 5
fi

# Construir imagen Ubuntu si no existe
if ! docker image inspect zenthra-ubuntu:latest >/dev/null 2>&1; then
    echo "[*] Construyendo imagen zenthra-ubuntu:latest..."
    docker build --platform linux/amd64 -t zenthra-ubuntu -f Dockerfile.ubuntu .
fi

echo "[*] Ejecutando compilacion y verificaciones en Ubuntu 22.04 LTS (x86_64)..."
docker run --rm --platform linux/amd64 -v "$ROOT_DIR:/workspace" zenthra-ubuntu \
    /bin/bash -c "make clean && make check-env && make test && make iso && make verify-qemu"

echo ""
echo "============================================================"
echo " [+] Zenthra OS levantado y verificado con exito en Ubuntu!  "
echo "============================================================"
