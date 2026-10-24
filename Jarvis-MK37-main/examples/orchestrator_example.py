"""
EJEMPLO: Cómo integrar el Orchestrator en el flujo de JARVIS

Este archivo demuestra cómo usar el Orchestrator para ejecutar tareas complejas
con auto-corrección automática.
"""

import asyncio
import json
from pathlib import Path
from typing import Dict, Any

from core.orchestrator import Orchestrator
from core.orchestrator_adapter import get_orchestrator_adapter
from core.agent_registry import get_agent_registry
from core.global_state import get_global_state


def setup_orchestrator_example():
    """
    Ejemplo de setup: cómo inicializar el Orchestrator con herramientas.
    """
    
    print("=" * 60)
    print("JARVIS Orchestrator - Example Setup")
    print("=" * 60)
    
    # 1. Obtén la API key
    api_key_path = Path(__file__).parent.parent / "config" / "api_keys.json"
    with open(api_key_path, "r") as f:
        api_key = json.load(f)["gemini_api_key"]
    
    # 2. Inicializa componentes
    print("\n📦 Initializing components...")
    
    agent_registry = get_agent_registry()
    global_state = get_global_state()
    adapter = get_orchestrator_adapter()
    orchestrator = Orchestrator(api_key=api_key)
    
    print(f"   ✅ Agent Registry: {len(agent_registry.list_agents())} agents available")
    print(f"   ✅ Global State: Ready")
    print(f"   ✅ Adapter: Ready")
    print(f"   ✅ Orchestrator: Ready")
    
    # 3. Registra algunas herramientas dummy para demostración
    print("\n🔧 Registering tools...")
    
    def dummy_open_app(app_name: str) -> Dict[str, Any]:
        """Tool dummy: abre una aplicación."""
        return {
            "output": f"Opening {app_name}...",
            "success": True
        }
    
    def dummy_search(query: str) -> Dict[str, Any]:
        """Tool dummy: busca en internet."""
        return {
            "output": f"Found results for: {query}",
            "success": True
        }
    
    def dummy_write_code(description: str, language: str = "python") -> Dict[str, Any]:
        """Tool dummy: escribe código."""
        code = f"# Generated code for: {description}\nprint('Hello from {language}')"
        return {
            "output": code,
            "success": True
        }
    
    adapter.register_tool("open_app", dummy_open_app)
    adapter.register_tool("web_search", dummy_search)
    adapter.register_tool("code_helper", dummy_write_code)
    
    print(f"   ✅ Registered {len(adapter.tool_registry)} tools")
    
    # 4. Configura el executor callback
    print("\n🚀 Setting up executor callback...")
    
    def tool_executor(tool_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Callback que el Orchestrator usa para ejecutar herramientas."""
        return adapter.execute_tool(tool_name, params)
    
    # 5. Actualiza el estado global
    print("\n💾 Updating global state...")
    global_state.set_context_var("orchestrator_initialized", True)
    global_state.set_context_var("available_tools", list(adapter.tool_registry.keys()))
    
    return {
        "orchestrator": orchestrator,
        "adapter": adapter,
        "agent_registry": agent_registry,
        "global_state": global_state,
        "tool_executor": tool_executor
    }


async def run_orchestrator_example(user_input: str):
    """
    Ejemplo de cómo ejecutar una tarea a través del Orchestrator.
    """
    
    # Setup
    components = setup_orchestrator_example()
    orchestrator = components["orchestrator"]
    tool_executor = components["tool_executor"]
    global_state = components["global_state"]
    
    print("\n" + "=" * 60)
    print(f"EXECUTING: {user_input}")
    print("=" * 60)
    
    # Inicia la tarea en el estado global
    task_id = f"task_{int(__import__('time').time())}"
    global_state.start_task(task_id, user_input)
    
    # Ejecuta el ciclo completo del Orchestrator
    result = orchestrator.run(
        user_input=user_input,
        context=None,
        tool_executor=tool_executor
    )
    
    # Actualiza el estado global
    if result["status"] == "completed":
        global_state.complete_task(task_id, json.dumps(result))
    else:
        global_state.fail_task(task_id, result.get("error", "Unknown error"))
    
    # Muestra resultados
    print("\n" + "=" * 60)
    print("RESULTS")
    print("=" * 60)
    
    print(json.dumps(result, indent=2))
    
    print("\n" + "=" * 60)
    print("STATE SUMMARY")
    print("=" * 60)
    
    print(json.dumps(global_state.get_summary(), indent=2))
    
    return result


# ==============================================================================
# EJEMPLOS DE USO
# ==============================================================================

if __name__ == "__main__":
    
    # EJEMPLO 1: Tarea simple
    print("\n\n🔹 EJEMPLO 1: Tarea Simple")
    asyncio.run(run_orchestrator_example("Abre Chrome y busca noticias sobre IA"))
    
    # EJEMPLO 2: Tarea con código
    print("\n\n🔹 EJEMPLO 2: Tarea con Código")
    asyncio.run(run_orchestrator_example("Escribe código Python que calcule fibonacci hasta 10"))
    
    # EJEMPLO 3: Múltiples pasos
    print("\n\n🔹 EJEMPLO 3: Múltiples Pasos")
    asyncio.run(run_orchestrator_example(
        "Abre VS Code, crea un archivo test.py, escribe código de prueba y ejecútalo"
    ))
