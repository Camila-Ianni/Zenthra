"""
Integración de modelo Mistral 7B local con JARVIS.
Usa Ollama para ejecutar el modelo sin necesidad de GPU costosa.
"""

import requests
import json
from pathlib import Path

import os

OLLAMA_BASE = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434")
OLLAMA_URL = f"{OLLAMA_BASE}/api/generate"
MODEL_NAME = os.getenv("AGENT_CREATIVE_MODEL", "mistral")


def is_ollama_running():
    """Verifica si Ollama está corriendo."""
    try:
        response = requests.get(f"{OLLAMA_BASE}/api/tags", timeout=2)
        return response.status_code == 200
    except:
        return False


def get_available_model(preferred: str = "mistral") -> str:
    """Retorna el modelo preferido o uno disponible en Ollama."""
    try:
        res = requests.get(f"{OLLAMA_BASE}/api/tags", timeout=2)
        if res.status_code == 200:
            models = [m.get("name", "") for m in res.json().get("models", [])]
            for m in models:
                if preferred in m:
                    return m
            if models:
                return models[0]
    except Exception:
        pass
    return preferred


def generate_code_with_mistral(prompt: str, language: str = "python", 
                               max_tokens: int = 2048) -> str:
    """
    Genera código usando Mistral 7B local.
    
    Args:
        prompt: Descripción de lo que se quiere codear
        language: Lenguaje de programación
        max_tokens: Máximo de tokens a generar
    
    Returns:
        Código generado
    """
    
    if not is_ollama_running():
        return "❌ Error: Ollama no está corriendo. Ejecuta: ollama serve"
    
    # Construir prompt mejorado
    full_prompt = f"""Eres un experto programador en {language}. 
Genera código limpio, eficiente y bien comentado.

Tarea: {prompt}

Responde SOLO con el código, sin explicaciones adicionales."""
    
    try:
        target_model = get_available_model(MODEL_NAME)
        response = requests.post(
            OLLAMA_URL,
            json={
                "model": target_model,
                "prompt": full_prompt,
                "stream": False,
                "options": {
                    "temperature": 0.7,
                    "top_p": 0.9,
                    "top_k": 40,
                }
            },
            timeout=300  # 5 minutos max
        )
        
        if response.status_code == 200:
            result = response.json()
            code = result.get("response", "").strip()
            
            # Limpiar código
            if code.startswith("```"):
                code = code.split("```", 2)[1]
                if code.startswith(language):
                    code = code[len(language):].lstrip("\n")
                code = code.rstrip("```").strip()
            
            return code
        else:
            return f"❌ Error Ollama: {response.status_code}"
    
    except requests.exceptions.Timeout:
        return "⏱️ Timeout: La generación tardó demasiado"
    except Exception as e:
        return f"❌ Error: {str(e)}"


def code_helper_with_mistral(description: str, language: str = "python",
                             output_path: str = None) -> dict:
    """
    Función compatible con la interfaz de code_helper de JARVIS.
    
    Args:
        description: Descripción de la tarea
        language: Lenguaje de programación
        output_path: Ruta donde guardar el código
    
    Returns:
        Dict con resultado
    """
    
    print(f"🤖 Generando {language} con Mistral 7B...")
    
    code = generate_code_with_mistral(description, language)
    
    if code.startswith("❌") or code.startswith("⏱️"):
        return {"success": False, "error": code, "code": ""}
    
    # Guardar si se pide
    if output_path:
        try:
            path = Path(output_path)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(code)
            print(f"💾 Guardado en: {output_path}")
            return {"success": True, "code": code, "file": str(path)}
        except Exception as e:
            return {"success": False, "error": str(e), "code": code}
    
    return {"success": True, "code": code}


if __name__ == "__main__":
    # Test
    if not is_ollama_running():
        print("❌ Ollama no está corriendo")
        print("Inicia con: ollama serve")
    else:
        print("✅ Ollama está corriendo")
        
        # Test generación
        code = generate_code_with_mistral(
            "Función que calcula fibonacci recursivamente",
            "python"
        )
        print("\n📝 Código generado:")
        print(code)
