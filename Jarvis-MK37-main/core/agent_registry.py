"""
AgentRegistry - Registro centralizado de agentes disponibles
"""
from dataclasses import dataclass, field
from typing import Optional, Dict, Any, Callable, List
from enum import Enum


class AgentType(Enum):
    """Tipos de agentes disponibles."""
    ENTERTAINMENT = "entertainment"  # Música, juegos, relajación
    CODER = "coder"  # Escribir código, debugging, DevOps
    RESEARCH = "research"  # Búsqueda web, data analysis
    SYSTEM = "system"  # Control del sistema, apps
    COMMUNICATION = "communication"  # Slack, email, mensajes
    CUSTOM = "custom"
    # ── 6 Agentes Especialistas de Jarvis-MK37 ──
    BACKEND = "backend"
    CYBERSECURITY = "cybersecurity"
    KERNEL_SYSTEMS = "kernel_systems"
    DATA = "data"
    CREATIVE_UI = "creative_ui"
    AUTOMATION = "automation"


@dataclass
class AgentCapability:
    """Define lo que un agente puede hacer."""
    name: str  # ej: "code_helper", "open_app"
    description: str
    tool_name: str  # referencia a la herramienta en main.py
    requires_params: List[str] = field(default_factory=list)  # ej: ["code", "language"]
    async_capable: bool = False


@dataclass
class Agent:
    """Define un agente en el sistema."""
    id: str
    name: str
    agent_type: AgentType
    description: str
    capabilities: List[AgentCapability] = field(default_factory=list)
    enabled: bool = True
    priority: int = 0  # Mayor = más prioritario
    executor: Optional[Callable] = None  # Función que ejecuta este agente
    specialist_agent: Optional[Any] = None  # Referencia al SpecialistAgent implementado
    
    def add_capability(self, capability: AgentCapability) -> None:
        """Añade una capacidad a este agente."""
        self.capabilities.append(capability)


class AgentRegistry:
    """Registro centralizado de agentes disponibles."""
    
    def __init__(self):
        self.agents: Dict[str, Agent] = {}
        self._init_default_agents()
    
    def _init_default_agents(self) -> None:
        """Inicializa los agentes por defecto del sistema."""
        
        # AGENTE: ENTRETENIMIENTO
        entertainment_agent = Agent(
            id="agent_entertainment",
            name="Entertainment Agent",
            agent_type=AgentType.ENTERTAINMENT,
            description="Reproduce música, maneja entretenimiento y relajación",
            priority=10
        )
        entertainment_agent.add_capability(
            AgentCapability(
                name="play_music",
                description="Reproduce música en Spotify",
                tool_name="spotify_player",
                requires_params=["artist", "song"]
            )
        )
        entertainment_agent.add_capability(
            AgentCapability(
                name="stop_music",
                description="Detiene la reproducción actual",
                tool_name="spotify_player"
            )
        )
        self.register_agent(entertainment_agent)
        
        # AGENTE: CODER
        coder_agent = Agent(
            id="agent_coder",
            name="Coder Agent",
            agent_type=AgentType.CODER,
            description="Genera código, debuggea y maneja DevOps",
            priority=8
        )
        coder_agent.add_capability(
            AgentCapability(
                name="write_code",
                description="Genera código Python/JavaScript/etc",
                tool_name="code_helper",
                requires_params=["description", "language"]
            )
        )
        coder_agent.add_capability(
            AgentCapability(
                name="fix_code",
                description="Arregla errores en código",
                tool_name="code_helper",
                requires_params=["error", "code"]
            )
        )
        coder_agent.add_capability(
            AgentCapability(
                name="run_code",
                description="Ejecuta código generado",
                tool_name="executor",
                requires_params=["code"]
            )
        )
        self.register_agent(coder_agent)
        
        # AGENTE: SYSTEM
        system_agent = Agent(
            id="agent_system",
            name="System Agent",
            agent_type=AgentType.SYSTEM,
            description="Controla aplicaciones, pantalla y configuración",
            priority=7
        )
        system_agent.add_capability(
            AgentCapability(
                name="open_app",
                description="Abre aplicaciones",
                tool_name="open_app",
                requires_params=["app_name"]
            )
        )
        system_agent.add_capability(
            AgentCapability(
                name="screenshot",
                description="Toma una captura de pantalla",
                tool_name="screen_processor"
            )
        )
        system_agent.add_capability(
            AgentCapability(
                name="control_system",
                description="Ejecuta comandos de sistema",
                tool_name="computer_control",
                requires_params=["action"]
            )
        )
        self.register_agent(system_agent)
        
        # AGENTE: RESEARCH
        research_agent = Agent(
            id="agent_research",
            name="Research Agent",
            agent_type=AgentType.RESEARCH,
            description="Búsqueda web, análisis de datos",
            priority=6
        )
        research_agent.add_capability(
            AgentCapability(
                name="search_web",
                description="Busca información en internet",
                tool_name="web_search",
                requires_params=["query"]
            )
        )
        research_agent.add_capability(
            AgentCapability(
                name="get_weather",
                description="Obtiene info de clima",
                tool_name="weather_report",
                requires_params=["location"]
            )
        )
        self.register_agent(research_agent)
        
        # ── 6 AGENTES ESPECIALISTAS DE JARVIS-MK37 ──
        try:
            from core.specialist_agents import list_specialist_agents
            specialists = list_specialist_agents()
            type_mapping = {
                "backend": AgentType.BACKEND,
                "cybersecurity": AgentType.CYBERSECURITY,
                "kernel": AgentType.KERNEL_SYSTEMS,
                "data": AgentType.DATA,
                "creative": AgentType.CREATIVE_UI,
                "automation": AgentType.AUTOMATION,
            }
            for spec in specialists:
                if not spec:
                    continue
                ag_type = type_mapping.get(spec.specialty, AgentType.CUSTOM)
                ag = Agent(
                    id=spec.agent_id,
                    name=spec.name,
                    agent_type=ag_type,
                    description=f"{spec.specialty.title()} specialist - {spec.get_configured_model()}",
                    priority=9,
                    specialist_agent=spec
                )
                for tool in spec.allowed_tools:
                    ag.add_capability(AgentCapability(
                        name=tool,
                        description=f"Capacidad del especialista {spec.name}",
                        tool_name=tool
                    ))
                self.register_agent(ag)
        except Exception as e:
            print(f"[AgentRegistry] ⚠️ Error loading specialist agents: {e}")
    
    def get_specialist(self, specialty: str) -> Optional[Agent]:
        """Obtiene un agente especialista por nombre de especialidad."""
        spec_clean = specialty.lower().strip()
        for agent in self.agents.values():
            if agent.specialist_agent and (agent.specialist_agent.specialty == spec_clean or spec_clean in agent.id):
                return agent
        return None
    
    def register_agent(self, agent: Agent) -> None:
        """Registra un nuevo agente."""
        self.agents[agent.id] = agent
        print(f"[AgentRegistry] ✅ Registered: {agent.name} ({agent.id})")
    
    def get_agent(self, agent_id: str) -> Optional[Agent]:
        """Obtiene un agente por ID."""
        return self.agents.get(agent_id)
    
    def get_agents_by_type(self, agent_type: AgentType) -> List[Agent]:
        """Obtiene todos los agentes de un tipo."""
        return [a for a in self.agents.values() if a.agent_type == agent_type]
    
    def list_agents(self) -> List[Agent]:
        """Lista todos los agentes registrados."""
        return sorted(self.agents.values(), key=lambda a: a.priority, reverse=True)
    
    def find_agent_for_capability(self, capability_name: str) -> Optional[Agent]:
        """Encuentra el agente que puede hacer una acción específica."""
        for agent in self.agents.values():
            if agent.enabled:
                for cap in agent.capabilities:
                    if cap.name == capability_name:
                        return agent
        return None
    
    def get_capability(self, agent_id: str, capability_name: str) -> Optional[AgentCapability]:
        """Obtiene una capacidad específica de un agente."""
        agent = self.get_agent(agent_id)
        if agent:
            for cap in agent.capabilities:
                if cap.name == capability_name:
                    return cap
        return None
    
    def list_all_capabilities(self) -> Dict[str, List[str]]:
        """Lista todas las capacidades disponibles organizadas por agente."""
        result = {}
        for agent in self.agents.values():
            result[agent.name] = [cap.name for cap in agent.capabilities]
        return result
    
    def enable_agent(self, agent_id: str) -> bool:
        """Habilita un agente."""
        agent = self.get_agent(agent_id)
        if agent:
            agent.enabled = True
            print(f"[AgentRegistry] ✅ Enabled: {agent.name}")
            return True
        return False
    
    def disable_agent(self, agent_id: str) -> bool:
        """Desactiva un agente."""
        agent = self.get_agent(agent_id)
        if agent:
            agent.enabled = False
            print(f"[AgentRegistry] ⛔ Disabled: {agent.name}")
            return True
        return False


# Singleton global
_registry = None


def get_agent_registry() -> AgentRegistry:
    """Obtiene la instancia única del registry."""
    global _registry
    if _registry is None:
        _registry = AgentRegistry()
    return _registry
