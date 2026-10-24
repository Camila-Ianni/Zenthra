"""
ExecutorHealing - Enhanced executor with self-healing capabilities
Detects errors, analyzes them, and auto-corrects without human intervention
"""
import json
import re
import sys
import tempfile
import subprocess
import os
import time
from pathlib import Path
from typing import Optional, Dict, Any, Tuple, Callable
from google import genai


def get_base_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent.parent


BASE_DIR = get_base_dir()
API_CONFIG_PATH = BASE_DIR / "config" / "api_keys.json"


def _get_api_key() -> str:
    """Obtiene la API key de Google."""
    with open(API_CONFIG_PATH, "r", encoding="utf-8") as f:
        return json.load(f)["gemini_api_key"]


def _analyze_error(error_message: str, code: str, model: str = "gemini-2.5-flash") -> Dict[str, Any]:
    """
    Analiza un error y genera sugerencias de corrección usando LLM.
    """
    client = genai.Client(api_key=_get_api_key())
    
    prompt = f"""Eres un experto debugger. Analiza este error y proporciona una solución.

CODE:
```python
{code}
```

ERROR:
{error_message}

Proporciona tu análisis en formato JSON:
{{
  "root_cause": "Causa raíz del error (1-2 líneas)",
  "severity": "critical|warning|info",
  "suggested_fix": "Descripción de la corrección necesaria",
  "fixed_code": "Código corregido completo en Python"
}}

IMPORTANTE: El fixed_code debe ser funcional y listo para ejecutar."""

    try:
        response = client.models.generate_content(
            model=model,
            contents=prompt
        )
        
        text = response.text
        start = text.find("{")
        end = text.rfind("}") + 1
        
        if start >= 0 and end > start:
            analysis = json.loads(text[start:end])
            return analysis
        else:
            return {
                "root_cause": "Could not parse fix",
                "severity": "critical",
                "suggested_fix": "Manual intervention required",
                "fixed_code": code
            }
            
    except Exception as e:
        print(f"[ExecutorHealing] ⚠️ Error analysis failed: {e}")
        return {
            "root_cause": str(e),
            "severity": "critical",
            "suggested_fix": "Analysis service unavailable",
            "fixed_code": code
        }


def run_code_with_healing(
    code: str, 
    description: str = "",
    max_retries: int = 3,
    speak: Optional[Callable] = None
) -> Tuple[bool, str, Dict[str, Any]]:
    """
    Ejecuta código Python con self-healing automático.
    
    Retorna: (success: bool, output: str, metadata: dict)
    
    Si el código falla:
    1. Detecta el error en stderr
    2. Analiza la causa raíz con LLM
    3. Genera código corregido
    4. Reintenta (hasta max_retries veces)
    """
    
    metadata = {
        "attempts": 0,
        "errors": [],
        "fixes_applied": []
    }
    
    current_code = code
    
    for attempt in range(max_retries):
        metadata["attempts"] = attempt + 1
        
        print(f"[ExecutorHealing] 🚀 Execution attempt {attempt + 1}/{max_retries}")
        
        try:
            # Crea archivo temporal
            with tempfile.NamedTemporaryFile(
                mode="w", suffix=".py", delete=False, encoding="utf-8"
            ) as f:
                f.write(current_code)
                tmp_path = f.name
            
            # Ejecuta el código
            result = subprocess.run(
                [sys.executable, tmp_path],
                capture_output=True,
                text=True,
                timeout=120,
                cwd=str(Path.home())
            )
            
            # Limpia archivo temporal
            try:
                os.unlink(tmp_path)
            except Exception:
                pass
            
            output = result.stdout.strip()
            stderr = result.stderr.strip()
            
            # Caso 1: Éxito total
            if result.returncode == 0:
                if output:
                    print(f"[ExecutorHealing] ✅ Success on attempt {attempt + 1}")
                    return True, output, metadata
                else:
                    print(f"[ExecutorHealing] ✅ Completed on attempt {attempt + 1}")
                    return True, "Task completed successfully.", metadata
            
            # Caso 2: Error detectado
            if stderr or result.returncode != 0:
                error_msg = stderr if stderr else f"Process exited with code {result.returncode}"
                print(f"[ExecutorHealing] ⚠️ Error detected: {error_msg[:100]}")
                metadata["errors"].append({
                    "attempt": attempt + 1,
                    "error": error_msg[:500]
                })
                
                # Si es el último intento, falla
                if attempt == max_retries - 1:
                    print(f"[ExecutorHealing] ❌ Max retries reached. Last error:")
                    return False, error_msg, metadata
                
                # Intenta auto-corrección
                print(f"[ExecutorHealing] 🔧 Analyzing error for automatic fix...")
                analysis = _analyze_error(error_msg, current_code)
                
                print(f"[ExecutorHealing] 💡 Root cause: {analysis.get('root_cause', '?')}")
                print(f"[ExecutorHealing] 📋 Fix suggestion: {analysis.get('suggested_fix', '?')}")
                
                metadata["fixes_applied"].append({
                    "attempt": attempt + 1,
                    "analysis": analysis
                })
                
                # Usa el código corregido para el próximo intento
                current_code = analysis.get("fixed_code", current_code)
                
                # Wait a bit before retry (backoff)
                time.sleep(1 + attempt)
                
        except subprocess.TimeoutExpired:
            error_msg = "Code execution timed out (120s)"
            print(f"[ExecutorHealing] ⚠️ {error_msg}")
            metadata["errors"].append({
                "attempt": attempt + 1,
                "error": error_msg
            })
            
            if attempt == max_retries - 1:
                return False, error_msg, metadata
            
            time.sleep(1 + attempt)
            
        except Exception as e:
            error_msg = f"Execution failed: {str(e)}"
            print(f"[ExecutorHealing] ⚠️ {error_msg}")
            metadata["errors"].append({
                "attempt": attempt + 1,
                "error": error_msg
            })
            
            if attempt == max_retries - 1:
                return False, error_msg, metadata
            
            time.sleep(1 + attempt)
    
    # Fallback (shouldn't reach here)
    return False, "Execution failed after all attempts", metadata


def run_shell_command_with_healing(
    command: str,
    max_retries: int = 2,
    cwd: Optional[str] = None
) -> Tuple[bool, str, str]:
    """
    Ejecuta comandos de shell con self-healing.
    
    Retorna: (success: bool, stdout: str, stderr: str)
    """
    
    for attempt in range(max_retries):
        print(f"[ExecutorHealing] 🔧 Shell execution attempt {attempt + 1}/{max_retries}")
        
        try:
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                timeout=60,
                cwd=cwd or str(Path.home())
            )
            
            if result.returncode == 0:
                print(f"[ExecutorHealing] ✅ Shell command succeeded")
                return True, result.stdout, result.stderr
            
            # Si falla y no es el último intento
            if attempt < max_retries - 1:
                stderr = result.stderr.strip()
                print(f"[ExecutorHealing] ⚠️ Command failed: {stderr[:100]}")
                time.sleep(1 + attempt)
            else:
                print(f"[ExecutorHealing] ❌ Shell command failed after {max_retries} attempts")
                return False, result.stdout, result.stderr
                
        except subprocess.TimeoutExpired:
            print(f"[ExecutorHealing] ⚠️ Shell command timed out")
            if attempt == max_retries - 1:
                return False, "", "Command timeout"
            time.sleep(1 + attempt)
            
        except Exception as e:
            print(f"[ExecutorHealing] ⚠️ Shell execution error: {e}")
            if attempt == max_retries - 1:
                return False, "", str(e)
            time.sleep(1 + attempt)
    
    return False, "", "Unknown error"


# Wrapper para compatibilidad con código existente
def run_generated_code_with_healing(
    description: str,
    speak: Optional[Callable] = None,
    max_retries: int = 3
) -> str:
    """
    Genera código desde descripción y lo ejecuta con self-healing.
    
    Nota: Mantiene compatibilidad con executor.py existente.
    """
    client = genai.Client(api_key=_get_api_key())
    
    if speak:
        speak("Writing and executing custom code for this task, sir.")
    
    # Genera el código
    try:
        response = client.models.generate_content(
            model="gemini-2.5-flash",
            contents=f"Write Python code to accomplish this task:\n\n{description}"
        )
        code = response.text.strip()
        code = re.sub(r"```(?:python)?", "", code).strip().rstrip("`").strip()
        
    except Exception as e:
        raise RuntimeError(f"Failed to generate code: {e}")
    
    # Ejecuta con self-healing
    success, output, metadata = run_code_with_healing(
        code=code,
        description=description,
        max_retries=max_retries,
        speak=speak
    )
    
    if success:
        return output
    else:
        raise RuntimeError(f"Code execution failed after {metadata['attempts']} attempts: {output}")


class SelfHealingExecutor:
    """
    Wrapper class que integra self-healing en el Orchestrator.
    """
    
    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or _get_api_key()
        self.execution_history: list = []
    
    def execute_python(self, code: str, description: str = "") -> Dict[str, Any]:
        """Ejecuta código Python con self-healing."""
        success, output, metadata = run_code_with_healing(
            code=code,
            description=description,
            max_retries=3
        )
        
        result = {
            "success": success,
            "output": output,
            "metadata": metadata,
            "stderr": "" if success else output
        }
        
        self.execution_history.append(result)
        return result
    
    def execute_shell(self, command: str) -> Dict[str, Any]:
        """Ejecuta comando shell con self-healing."""
        success, stdout, stderr = run_shell_command_with_healing(
            command=command,
            max_retries=2
        )
        
        result = {
            "success": success,
            "output": stdout,
            "stderr": stderr
        }
        
        self.execution_history.append(result)
        return result
