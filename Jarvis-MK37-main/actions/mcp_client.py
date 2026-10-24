"""
MCPClient - Dynamic MCP (Model Context Protocol) tool loader
Carga herramientas dinámicamente desde config/tools.json y config/mcp_providers.json
"""
import json
import sys
import importlib
import subprocess
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable, Tuple
from dataclasses import dataclass


def get_base_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent.parent


BASE_DIR = get_base_dir()
TOOLS_CONFIG_PATH = BASE_DIR / "config" / "tools.json"
MCP_PROVIDERS_PATH = BASE_DIR / "config" / "mcp_providers.json"


@dataclass
class ToolDefinition:
    """Define una herramienta disponible."""
    id: str
    name: str
    category: str
    description: str
    module: Optional[str] = None
    function: Optional[str] = None
    mcp_server: Optional[str] = None
    parameters: Dict[str, Any] = None
    requires_healing: bool = False
    timeout_seconds: int = 60
    max_retries: int = 1
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "category": self.category,
            "description": self.description,
            "module": self.module,
            "function": self.function,
            "mcp_server": self.mcp_server,
            "parameters": self.parameters,
            "requires_healing": self.requires_healing,
            "timeout_seconds": self.timeout_seconds,
            "max_retries": self.max_retries
        }


class MCPClient:
    """
    Cliente dinámico de MCP que carga herramientas desde configuración.
    Soporta tanto tools locales como MCP servers externos.
    """
    
    def __init__(self, auto_enable_local: bool = True):
        self.tool_registry: Dict[str, ToolDefinition] = {}
        self.tool_implementations: Dict[str, Callable] = {}
        self.mcp_providers: Dict[str, Any] = {}
        self.active_mcp_connections: Dict[str, Any] = {}
        self.auto_enable_local = auto_enable_local
        
        # Carga configuraciones
        self._load_tools_config()
        self._load_mcp_providers_config()
    
    def _load_tools_config(self) -> None:
        """Carga config/tools.json."""
        try:
            with open(TOOLS_CONFIG_PATH, "r", encoding="utf-8") as f:
                config = json.load(f)
            
            # Carga tools locales
            for tool_data in config.get("tools", []):
                tool_def = ToolDefinition(
                    id=tool_data["id"],
                    name=tool_data["name"],
                    category=tool_data["category"],
                    description=tool_data["description"],
                    module=tool_data.get("module"),
                    function=tool_data.get("function"),
                    parameters=tool_data.get("parameters", {}),
                    requires_healing=tool_data.get("requires_healing", False),
                    timeout_seconds=tool_data.get("timeout_seconds", 60),
                    max_retries=tool_data.get("max_retries", 1)
                )
                self.tool_registry[tool_def.id] = tool_def
            
            # Carga MCP tools
            for tool_data in config.get("mcp_tools", []):
                tool_def = ToolDefinition(
                    id=tool_data["id"],
                    name=tool_data["name"],
                    category=tool_data["category"],
                    description=tool_data["description"],
                    mcp_server=tool_data.get("mcp_server"),
                    parameters=tool_data.get("capabilities", [])
                )
                self.tool_registry[tool_def.id] = tool_def
            
            print(f"[MCPClient] ✅ Loaded {len(self.tool_registry)} tools from config")
            
        except Exception as e:
            print(f"[MCPClient] ❌ Failed to load tools.json: {e}")
    
    def _load_mcp_providers_config(self) -> None:
        """Carga config/mcp_providers.json."""
        try:
            with open(MCP_PROVIDERS_PATH, "r", encoding="utf-8") as f:
                config = json.load(f)
            
            self.mcp_providers = config.get("providers", {})
            
            # Auto-enable local providers
            if self.auto_enable_local:
                for provider_id, provider_config in self.mcp_providers.items():
                    if provider_config.get("enabled", False):
                        self._enable_provider(provider_id, provider_config)
            
            print(f"[MCPClient] ✅ Loaded {len(self.mcp_providers)} MCP providers")
            
        except Exception as e:
            print(f"[MCPClient] ❌ Failed to load mcp_providers.json: {e}")
    
    def _enable_provider(self, provider_id: str, provider_config: Dict[str, Any]) -> bool:
        """Habilita un proveedor MCP."""
        try:
            print(f"[MCPClient] 🔌 Enabling provider: {provider_id}")
            
            # Por ahora, solo marca como disponible
            # En producción, esto iniciaría el proceso MCP
            self.active_mcp_connections[provider_id] = {
                "status": "connected",
                "provider": provider_config["name"],
                "capabilities": provider_config.get("capabilities", [])
            }
            
            return True
            
        except Exception as e:
            print(f"[MCPClient] ❌ Failed to enable {provider_id}: {e}")
            return False
    
    def register_local_tool(self, tool_id: str, implementation: Callable) -> bool:
        """Registra la implementación de una herramienta local."""
        if tool_id not in self.tool_registry:
            print(f"[MCPClient] ❌ Tool {tool_id} not in registry")
            return False
        
        self.tool_implementations[tool_id] = implementation
        print(f"[MCPClient] ✅ Registered implementation for {tool_id}")
        return True
    
    def load_tool_implementation(self, tool_id: str) -> Optional[Callable]:
        """
        Carga dinámicamente la implementación de una herramienta.
        Intenta importar el módulo y obtener la función.
        """
        if tool_id in self.tool_implementations:
            return self.tool_implementations[tool_id]
        
        tool_def = self.tool_registry.get(tool_id)
        if not tool_def or not tool_def.module or not tool_def.function:
            print(f"[MCPClient] ❌ Tool {tool_id} has no module/function")
            return None
        
        try:
            # Importa dinámicamente el módulo
            module = importlib.import_module(tool_def.module)
            implementation = getattr(module, tool_def.function)
            
            self.tool_implementations[tool_id] = implementation
            print(f"[MCPClient] ✅ Loaded implementation for {tool_id}")
            return implementation
            
        except Exception as e:
            print(f"[MCPClient] ❌ Failed to load {tool_id}: {e}")
            return None
    
    def execute_tool(self, tool_id: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """
        Ejecuta una herramienta (local o MCP).
        """
        tool_def = self.tool_registry.get(tool_id)
        if not tool_def:
            return {
                "success": False,
                "error": f"Unknown tool: {tool_id}"
            }
        
        # Si es MCP tool, redirige al provider
        if tool_def.mcp_server:
            return self._execute_mcp_tool(tool_id, tool_def, params)
        
        # Si es local tool, carga e intenta ejecutar
        impl = self.load_tool_implementation(tool_id)
        if not impl:
            return {
                "success": False,
                "error": f"Could not load implementation for {tool_id}"
            }
        
        try:
            result = impl(**params) if isinstance(params, dict) else impl(params)
            return {
                "success": True,
                "output": str(result),
                "tool_id": tool_id
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "tool_id": tool_id
            }
    
    def _execute_mcp_tool(self, tool_id: str, tool_def: ToolDefinition, 
                         params: Dict[str, Any]) -> Dict[str, Any]:
        """Ejecuta una herramienta a través de MCP."""
        provider_id = tool_def.mcp_server
        
        if provider_id not in self.active_mcp_connections:
            return {
                "success": False,
                "error": f"MCP provider not connected: {provider_id}"
            }
        
        try:
            # Simulación: en producción, esto enviaría la solicitud al MCP server
            connection = self.active_mcp_connections[provider_id]
            print(f"[MCPClient] 🌐 Executing {tool_id} via {provider_id}")
            
            return {
                "success": True,
                "output": f"MCP call to {provider_id}",
                "mcp_provider": provider_id,
                "tool_id": tool_id,
                "note": "Full MCP integration pending"
            }
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }
    
    def get_tools_by_category(self, category: str) -> List[ToolDefinition]:
        """Obtiene todas las herramientas de una categoría."""
        return [
            tool for tool in self.tool_registry.values()
            if tool.category == category
        ]
    
    def get_tools_for_preset(self, preset: str) -> List[ToolDefinition]:
        """Obtiene herramientas para un preset."""
        try:
            with open(TOOLS_CONFIG_PATH, "r", encoding="utf-8") as f:
                config = json.load(f)
            
            preset_tools = config.get("presets", {}).get(preset, [])
            
            if preset == "full_suite":
                return list(self.tool_registry.values())
            
            return [
                self.tool_registry[tool_id]
                for tool_id in preset_tools
                if tool_id in self.tool_registry
            ]
            
        except Exception as e:
            print(f"[MCPClient] ⚠️ Error loading preset: {e}")
            return []
    
    def list_tools(self, category: Optional[str] = None) -> List[Dict[str, Any]]:
        """Lista herramientas disponibles."""
        tools = self.tool_registry.values()
        
        if category:
            tools = [t for t in tools if t.category == category]
        
        return [
            {
                "id": t.id,
                "name": t.name,
                "category": t.category,
                "description": t.description,
                "type": "mcp" if t.mcp_server else "local",
                "available": t.module is not None or t.mcp_server is not None
            }
            for t in sorted(tools, key=lambda x: x.name)
        ]
    
    def get_statistics(self) -> Dict[str, Any]:
        """Retorna estadísticas del cliente."""
        local_tools = sum(1 for t in self.tool_registry.values() if t.module)
        mcp_tools = sum(1 for t in self.tool_registry.values() if t.mcp_server)
        
        categories = {}
        for tool in self.tool_registry.values():
            if tool.category not in categories:
                categories[tool.category] = 0
            categories[tool.category] += 1
        
        return {
            "total_tools": len(self.tool_registry),
            "local_tools": local_tools,
            "mcp_tools": mcp_tools,
            "active_mcp_providers": len(self.active_mcp_connections),
            "categories": categories,
            "implementations_loaded": len(self.tool_implementations)
        }


# Singleton global
_mcp_client = None


def get_mcp_client() -> MCPClient:
    """Obtiene la instancia única del cliente MCP."""
    global _mcp_client
    if _mcp_client is None:
        _mcp_client = MCPClient(auto_enable_local=True)
    return _mcp_client


def execute_mcp_tool(tool_id: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Helper para ejecutar una herramienta."""
    client = get_mcp_client()
    return client.execute_tool(tool_id, params)
