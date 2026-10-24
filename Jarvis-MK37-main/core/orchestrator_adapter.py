"""
OrchestratorAdapter - Integra el Orchestrator con los tools y healing
Proporciona un ejecutor que el Orchestrator puede usar para llamar herramientas
"""
import json
import sys
from pathlib import Path
from typing import Dict, Any, Callable, Optional


def get_base_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent.parent


BASE_DIR = get_base_dir()


class OrchestratorAdapter:
    """
    Adaptador que proporciona una interfaz estándar para que el Orchestrator
    ejecute herramientas desde main.py
    """
    
    def __init__(self):
        self.tool_registry: Dict[str, Callable] = {}
        self.execution_count = 0
    
    def register_tool(self, tool_name: str, tool_func: Callable) -> None:
        """Registra una herramienta disponible."""
        self.tool_registry[tool_name] = tool_func
        print(f"[OrchestratorAdapter] ✅ Registered tool: {tool_name}")
    
    def execute_tool(self, tool_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """
        Ejecuta una herramienta registrada.
        
        Retorna:
        {
            "success": bool,
            "output": str,
            "error": str or None,
            "stderr": str or None
        }
        """
        self.execution_count += 1
        
        if tool_name not in self.tool_registry:
            return {
                "success": False,
                "output": None,
                "error": f"Unknown tool: {tool_name}",
                "stderr": f"Tool '{tool_name}' not registered"
            }
        
        tool_func = self.tool_registry[tool_name]
        
        try:
            print(f"[OrchestratorAdapter] 🔧 Executing: {tool_name}({params})")
            
            # Intenta ejecutar la herramienta
            if callable(tool_func):
                # Si es una función simple
                if isinstance(params, dict):
                    result = tool_func(**params)
                else:
                    result = tool_func(params)
            else:
                result = tool_func
            
            # Normaliza el resultado
            if isinstance(result, dict):
                output = result
            elif isinstance(result, str):
                output = {"output": result}
            else:
                output = {"output": str(result)}
            
            # Asegura estructura estándar
            if "success" not in output:
                output["success"] = True
            if "error" not in output:
                output["error"] = None
            if "stderr" not in output:
                output["stderr"] = None
            
            print(f"[OrchestratorAdapter] ✅ Tool succeeded: {tool_name}")
            return output
            
        except Exception as e:
            error_str = str(e)
            print(f"[OrchestratorAdapter] ❌ Tool failed: {tool_name} -> {error_str}")
            
            return {
                "success": False,
                "output": None,
                "error": error_str,
                "stderr": error_str
            }
    
    def execute_python_code(self, code: str) -> Dict[str, Any]:
        """
        Ejecuta código Python usando executor_healing.
        """
        try:
            from actions.executor_healing import run_code_with_healing
            
            success, output, metadata = run_code_with_healing(
                code=code,
                description="Orchestrator-generated code",
                max_retries=3
            )
            
            return {
                "success": success,
                "output": output,
                "error": None if success else output,
                "stderr": None if success else output,
                "metadata": metadata
            }
            
        except Exception as e:
            return {
                "success": False,
                "output": None,
                "error": str(e),
                "stderr": str(e)
            }
    
    def execute_shell_command(self, command: str, cwd: Optional[str] = None) -> Dict[str, Any]:
        """
        Ejecuta un comando de shell usando executor_healing.
        """
        try:
            from actions.executor_healing import run_shell_command_with_healing
            
            success, stdout, stderr = run_shell_command_with_healing(
                command=command,
                max_retries=2,
                cwd=cwd
            )
            
            return {
                "success": success,
                "output": stdout,
                "error": None if success else stderr,
                "stderr": stderr
            }
            
        except Exception as e:
            return {
                "success": False,
                "output": None,
                "error": str(e),
                "stderr": str(e)
            }
    
    def get_statistics(self) -> Dict[str, Any]:
        """Retorna estadísticas de ejecución."""
        return {
            "total_tools_registered": len(self.tool_registry),
            "tools": list(self.tool_registry.keys()),
            "total_executions": self.execution_count,
            "registered_at": self.tool_registry
        }


# Singleton global
_adapter = None


def get_orchestrator_adapter() -> OrchestratorAdapter:
    """Obtiene la instancia única del adaptador."""
    global _adapter
    if _adapter is None:
        _adapter = OrchestratorAdapter()
    return _adapter


def register_tool_for_orchestrator(tool_name: str, tool_func: Callable) -> None:
    """Helper function para registrar herramientas."""
    adapter = get_orchestrator_adapter()
    adapter.register_tool(tool_name, tool_func)
