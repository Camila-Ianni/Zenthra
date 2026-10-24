"""
EJEMPLO: MCP Infrastructure - Cómo usar herramientas dinámicamente
"""

import json
from pathlib import Path

from core.orchestrator import Orchestrator
from core.mcp_orchestration_adapter import (
    get_mcp_orchestration_adapter,
    setup_mcp_with_orchestrator
)
from core.agent_registry import get_agent_registry
from core.global_state import get_global_state


def example_1_explore_tools():
    """Ejemplo 1: Explorar herramientas disponibles."""
    print("\n" + "=" * 70)
    print("EJEMPLO 1: Explorar Herramientas Disponibles")
    print("=" * 70)
    
    adapter = get_mcp_orchestration_adapter()
    
    # Lista todas las herramientas
    all_tools = adapter.list_available_tools()
    print(f"\n✅ Total de herramientas: {len(all_tools)}")
    
    # Agrupa por categoría
    from collections import defaultdict
    by_category = defaultdict(list)
    for tool in all_tools:
        by_category[tool["category"]].append(tool["name"])
    
    print("\nHerramientas por categoría:")
    for category, tools in sorted(by_category.items()):
        print(f"\n  📁 {category.upper()}")
        for tool_name in sorted(tools):
            print(f"     • {tool_name}")
    
    # Info detallada de una herramienta
    print("\n\nDetalles de 'code_helper':")
    info = adapter.get_tool_info("code_helper")
    print(json.dumps(info, indent=2, ensure_ascii=False))


def example_2_orchestrator_with_mcp():
    """Ejemplo 2: Usar Orchestrator con MCP tools."""
    print("\n" + "=" * 70)
    print("EJEMPLO 2: Orchestrator + MCP Tools")
    print("=" * 70)
    
    # Setup
    print("\n🔧 Setting up MCP + Orchestrator...")
    stats = setup_mcp_with_orchestrator(auto_register=True)
    
    # Obtén API key
    api_key_path = Path(__file__).parent.parent / "config" / "api_keys.json"
    with open(api_key_path, "r") as f:
        api_key = json.load(f)["gemini_api_key"]
    
    # Inicializa Orchestrator
    orchestrator = Orchestrator(api_key=api_key)
    adapter = get_mcp_orchestration_adapter()
    
    # Ejecuta una tarea
    print("\n🚀 Ejecutando tarea con Orchestrator...")
    
    result = orchestrator.run(
        user_input="Abre Chrome y busca información sobre inteligencia artificial",
        tool_executor=adapter.get_orchestrator_tool_executor()
    )
    
    print("\n📋 Resultado:")
    print(json.dumps(result, indent=2, ensure_ascii=False)[:1000])  # Primeros 1000 chars


def example_3_dynamic_preset():
    """Ejemplo 3: Cargar preset de herramientas."""
    print("\n" + "=" * 70)
    print("EJEMPLO 3: Dynamic Presets")
    print("=" * 70)
    
    adapter = get_mcp_orchestration_adapter()
    mcp_client = adapter.mcp_client
    
    # Carga diferentes presets
    presets = ["coder_bundle", "research_bundle", "entertainment_bundle"]
    
    for preset in presets:
        tools = mcp_client.get_tools_for_preset(preset)
        print(f"\n📦 {preset}:")
        for tool in tools:
            print(f"   • {tool.name}")
    
    # Full suite
    print(f"\n📦 full_suite:")
    all_tools = mcp_client.get_tools_for_preset("full_suite")
    print(f"   Total: {len(all_tools)} herramientas")


def example_4_mcp_providers():
    """Ejemplo 4: Información sobre proveedores MCP."""
    print("\n" + "=" * 70)
    print("EJEMPLO 4: MCP Providers")
    print("=" * 70)
    
    adapter = get_mcp_orchestration_adapter()
    mcp_client = adapter.mcp_client
    
    print(f"\n✅ Proveedores MCP disponibles: {len(mcp_client.mcp_providers)}")
    
    for provider_id, provider_config in mcp_client.mcp_providers.items():
        print(f"\n🌐 {provider_config['name']}")
        print(f"   Habilitado: {provider_config.get('enabled', False)}")
        print(f"   Descripción: {provider_config['description']}")
        print(f"   Capabilities: {', '.join(provider_config.get('capabilities', [])[:3])}")
        if "setup_instructions" in provider_config:
            print(f"   Setup: {provider_config['setup_instructions'][:60]}...")


def example_5_category_based():
    """Ejemplo 5: Ejecutar herramientas por categoría."""
    print("\n" + "=" * 70)
    print("EJEMPLO 5: Herramientas por Categoría")
    print("=" * 70)
    
    adapter = get_mcp_orchestration_adapter()
    mcp_client = adapter.mcp_client
    
    categories = ["coder", "research", "system", "entertainment"]
    
    for category in categories:
        tools = mcp_client.get_tools_by_category(category)
        print(f"\n📂 {category.upper()}: {len(tools)} herramientas")
        for tool in tools[:3]:  # Primeras 3
            print(f"   • {tool.name}")
        if len(tools) > 3:
            print(f"   ... y {len(tools) - 3} más")


def example_6_state_tracking():
    """Ejemplo 6: Rastreo de herramientas en estado global."""
    print("\n" + "=" * 70)
    print("EJEMPLO 6: Tool Execution Tracking")
    print("=" * 70)
    
    adapter = get_mcp_orchestration_adapter()
    state = get_global_state()
    
    # Setup MCP
    setup_mcp_with_orchestrator()
    
    # Simula algunas ejecuciones
    print("\n🔄 Simulando ejecuciones...")
    for i in range(3):
        result = adapter.execute_tool_unified("web_search", {"query": f"Test {i}"})
        print(f"   {i+1}. Ejecutado: {result.get('tool_id', '?')}")
    
    # Revisa el estado
    print("\n📊 Estado global:")
    summary = state.get_summary()
    print(f"   • Total de ejecuciones en historial: {summary['total_executions']}")
    print(f"   • Variables de contexto: {summary['context_vars']}")
    
    # Últimas ejecuciones
    insights = state.get_agent_insights()
    print(f"\n📝 Últimas acciones:")
    for action in insights["recent_history"][-3:]:
        print(f"   • {action.get('action', '?')} - {action.get('timestamp', '?')[:10]}")


# ==============================================================================
# MAIN
# ==============================================================================

if __name__ == "__main__":
    import sys
    
    examples = {
        "1": example_1_explore_tools,
        "2": example_2_orchestrator_with_mcp,
        "3": example_3_dynamic_preset,
        "4": example_4_mcp_providers,
        "5": example_5_category_based,
        "6": example_6_state_tracking,
    }
    
    print("\n" + "=" * 70)
    print("MCP INFRASTRUCTURE EXAMPLES")
    print("=" * 70)
    print("\nElige un ejemplo:")
    for key, func in examples.items():
        print(f"  {key}. {func.__doc__}")
    print("  7. Todos")
    
    if len(sys.argv) > 1:
        choice = sys.argv[1]
    else:
        choice = input("\nOpción (1-7): ").strip()
    
    if choice == "7":
        for func in examples.values():
            try:
                func()
            except Exception as e:
                print(f"\n❌ Error: {e}")
    elif choice in examples:
        try:
            examples[choice]()
        except Exception as e:
            print(f"\n❌ Error: {e}")
    else:
        print("❌ Opción inválida")
    
    print("\n" + "=" * 70)
    print("✅ EJEMPLOS COMPLETADOS")
    print("=" * 70)
