"""
MCPOrchestrationAdapter - Integra MCP Client con el Orchestrator
Proporciona una forma unificada de ejecutar tools (locales + MCP) desde el Orchestrator
"""
import json
from typing import Dict, Any, Callable, Optional, List

from actions.mcp_client import get_mcp_client
from core.orchestrator_adapter import get_orchestrator_adapter
from core.global_state import get_global_state


class MCPOrchestrationAdapter:
    """
    Adaptador que integra MCPClient con OrchestratorAdapter.
    Permite que el Orchestrator ejecute tools dinámicamente desde config files.
    """
    
    def __init__(self):
        self.mcp_client = get_mcp_client()
        self.orchestrator_adapter = get_orchestrator_adapter()
        self.global_state = get_global_state()
        self._auto_register_tools = True
    
    def auto_register_all_tools(self) -> int:
        """
        Registra automáticamente TODAS las herramientas del MCPClient
        en el OrchestratorAdapter.
        """
        count = 0
        
        # Obtén todas las herramientas del config
        for tool_id, tool_def in self.mcp_client.tool_registry.items():
            # Skip MCP tools (requieren conexión separada)
            if tool_def.mcp_server:
                continue
            
            # Carga la implementación
            impl = self.mcp_client.load_tool_implementation(tool_id)
            if impl:
                # Registra en el Orchestrator adapter
                self.orchestrator_adapter.register_tool(tool_id, impl)
                count += 1
        
        print(f"[MCPOrchestrationAdapter] ✅ Auto-registered {count} tools")
        return count
    
    def register_tool_from_config(self, tool_id: str) -> bool:
        """Registra una herramienta específica basada en config."""
        impl = self.mcp_client.load_tool_implementation(tool_id)
        if impl:
            self.orchestrator_adapter.register_tool(tool_id, impl)
            return True
        return False
    
    def execute_tool_unified(self, tool_id: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """
        Ejecuta una herramienta a través de MCPClient.
        Combina herramientas locales + MCP en una interfaz unificada.
        """
        # Intenta con MCPClient (maneja ambos tipos)
        result = self.mcp_client.execute_tool(tool_id, params)
        
        # Registra en el estado global
        self.global_state.set_context_var(f"last_tool_execution", {
            "tool_id": tool_id,
            "success": result.get("success", False),
            "timestamp": __import__('datetime').datetime.now().isoformat()
        })
        
        return result
    
    def get_orchestrator_tool_executor(self) -> Callable:
        """
        Retorna un callback que el Orchestrator puede usar directamente.
        """
        def tool_executor(tool_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
            return self.execute_tool_unified(tool_name, params)
        
        return tool_executor
    
    def list_available_tools(self, category: Optional[str] = None) -> List[Dict[str, Any]]:
        """Lista todas las herramientas disponibles."""
        return self.mcp_client.list_tools(category=category)
    
    def get_tool_info(self, tool_id: str) -> Optional[Dict[str, Any]]:
        """Obtiene información detallada de una herramienta."""
        tool_def = self.mcp_client.tool_registry.get(tool_id)
        if not tool_def:
            return None
        
        return {
            "id": tool_def.id,
            "name": tool_def.name,
            "category": tool_def.category,
            "description": tool_def.description,
            "type": "mcp" if tool_def.mcp_server else "local",
            "mcp_server": tool_def.mcp_server,
            "parameters": tool_def.parameters,
            "requires_healing": tool_def.requires_healing,
            "timeout_seconds": tool_def.timeout_seconds,
            "max_retries": tool_def.max_retries
        }
    
    def get_statistics(self) -> Dict[str, Any]:
        """Retorna estadísticas combinadas."""
        mcp_stats = self.mcp_client.get_statistics()
        orch_stats = self.orchestrator_adapter.get_statistics()
        
        return {
            "mcp": mcp_stats,
            "orchestrator": orch_stats,
            "total_available": mcp_stats["total_tools"] + orch_stats["total_tools_registered"]
        }


# Singleton global
_adapter = None


def get_mcp_orchestration_adapter() -> MCPOrchestrationAdapter:
    """Obtiene la instancia única del adaptador."""
    global _adapter
    if _adapter is None:
        _adapter = MCPOrchestrationAdapter()
    return _adapter


def setup_mcp_with_orchestrator(auto_register: bool = True) -> Dict[str, Any]:
    """
    Setup completo: Inicializa MCPClient + OrchestratorAdapter.
    Recomendado llamar esto al inicio de main.py
    """
    print("\n" + "=" * 70)
    print("SETTING UP MCP WITH ORCHESTRATOR")
    print("=" * 70)
    
    adapter = get_mcp_orchestration_adapter()
    
    if auto_register:
        print("\n📝 Auto-registering all local tools...")
        count = adapter.auto_register_all_tools()
    
    stats = adapter.get_statistics()
    
    print("\n" + "=" * 70)
    print("SETUP COMPLETE")
    print("=" * 70)
    print(f"✅ Total tools available: {stats['total_available']}")
    print(f"   • MCP tools: {stats['mcp']['mcp_tools']}")
    print(f"   • Local tools: {stats['mcp']['local_tools']}")
    print(f"   • Active MCP providers: {stats['mcp']['active_mcp_providers']}")
    print(f"   • Registered with Orchestrator: {stats['orchestrator'].get('total_tools_registered', 0)}")
    
    return stats
