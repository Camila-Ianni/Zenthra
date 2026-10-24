#!/usr/bin/env python3
"""
Descargador e integrador de modelo Mistral 7B para JARVIS.
"""

import os
import sys
import subprocess
from pathlib import Path

def install_ollama():
    """Instala Ollama si no está disponible."""
    print("🔄 Instalando Ollama...")
    os.system("brew install ollama")
    print("✅ Ollama instalado")

def download_model():
    """Descarga modelo Mistral 7B."""
    print("📥 Descargando Mistral 7B (esto puede tardar 10-20 min)...")
    print("   Tamaño: ~4GB")
    
    # Ollama descarga automáticamente cuando se usa
    result = subprocess.run(
        ["ollama", "pull", "mistral"],
        capture_output=True, text=True
    )
    
    if result.returncode == 0:
        print("✅ Mistral 7B descargado correctamente")
        return True
    else:
        print(f"❌ Error: {result.stderr}")
        return False

def start_ollama_server():
    """Inicia el servidor Ollama."""
    print("🚀 Iniciando servidor Ollama...")
    subprocess.Popen(["ollama", "serve"], 
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL)
    print("✅ Servidor Ollama corriendo en localhost:11434")

def test_model():
    """Prueba el modelo."""
    print("🧪 Probando modelo...")
    import requests
    
    try:
        response = requests.post(
            "http://localhost:11434/api/generate",
            json={"model": "mistral", "prompt": "Hola", "stream": False},
            timeout=30
        )
        if response.status_code == 200:
            print("✅ Modelo funcionando correctamente")
            return True
    except Exception as e:
        print(f"⚠️ Error al conectar: {e}")
        return False
    
    return False

if __name__ == "__main__":
    print("=" * 60)
    print("JARVIS - Instalador de Modelo Mistral 7B")
    print("=" * 60)
    
    # 1. Instalar Ollama
    try:
        subprocess.run(["ollama", "--version"], 
                      capture_output=True, check=True)
        print("✅ Ollama ya está instalado")
    except:
        install_ollama()
    
    # 2. Descargar modelo
    if not download_model():
        sys.exit(1)
    
    # 3. Iniciar servidor
    start_ollama_server()
    
    # 4. Probar
    import time
    time.sleep(3)
    test_model()
    
    print("\n" + "=" * 60)
    print("✅ LISTO - JARVIS puede usar Mistral 7B para codear")
    print("=" * 60)
