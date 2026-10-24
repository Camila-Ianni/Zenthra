"""
GlobalState / Blackboard - Estado compartido entre todos los agentes
Persiste en memory/blackboard.json para continuidad entre sesiones
"""
import json
import sys
import threading
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any, List
from dataclasses import dataclass, asdict


def get_base_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent.parent


BASE_DIR = get_base_dir()
BLACKBOARD_PATH = BASE_DIR / "memory" / "blackboard.json"


@dataclass
class AgentState:
    """Estado de un agente específico."""
    agent_id: str
    last_action: Optional[str] = None
    last_output: Optional[str] = None
    status: str = "idle"  # idle, running, error
    last_updated: str = ""
    error_message: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class TaskState:
    """Estado de una tarea en progreso."""
    task_id: str
    user_input: str
    status: str = "pending"  # pending, executing, completed, failed
    started_at: str = ""
    completed_at: Optional[str] = None
    result: Optional[str] = None
    error: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


class GlobalState:
    """
    Blackboard global que almacena:
    - Estado de cada agente
    - Tareas en progreso
    - Contexto compartido (variables globales)
    - Historial de ejecuciones recientes
    """
    
    def __init__(self):
        self.agents_state: Dict[str, AgentState] = {}
        self.tasks_state: Dict[str, TaskState] = {}
        self.context: Dict[str, Any] = {}  # Variables globales
        self.execution_history: List[Dict[str, Any]] = []
        self._lock = threading.RLock()
        self._ensure_blackboard_dir()
        self.load()
    
    def _ensure_blackboard_dir(self) -> None:
        """Asegura que el directorio memory existe."""
        BLACKBOARD_PATH.parent.mkdir(parents=True, exist_ok=True)
    
    def load(self) -> None:
        """Carga el estado desde el archivo persistente."""
        with self._lock:
            if BLACKBOARD_PATH.exists():
                try:
                    data = json.loads(BLACKBOARD_PATH.read_text(encoding="utf-8"))
                    
                    # Restaura estado de agentes
                    for agent_id, state_dict in data.get("agents_state", {}).items():
                        self.agents_state[agent_id] = AgentState(**state_dict)
                    
                    # Restaura tareas
                    for task_id, task_dict in data.get("tasks_state", {}).items():
                        self.tasks_state[task_id] = TaskState(**task_dict)
                    
                    # Restaura contexto
                    self.context = data.get("context", {})
                    self.execution_history = data.get("execution_history", [])[-100:]  # Últimas 100
                    
                    print(f"[GlobalState] ✅ Loaded from {BLACKBOARD_PATH}")
                    
                except Exception as e:
                    print(f"[GlobalState] ⚠️ Failed to load: {e}, starting fresh")
                    self._reset()
            else:
                self._reset()
    
    def save(self) -> None:
        """Persiste el estado al archivo."""
        with self._lock:
            try:
                data = {
                    "agents_state": {aid: state.to_dict() for aid, state in self.agents_state.items()},
                    "tasks_state": {tid: state.to_dict() for tid, state in self.tasks_state.items()},
                    "context": self.context,
                    "execution_history": self.execution_history[-100:],  # Mantén últimas 100
                    "last_saved": datetime.now().isoformat()
                }
                BLACKBOARD_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
                
            except Exception as e:
                print(f"[GlobalState] ❌ Failed to save: {e}")
    
    def _reset(self) -> None:
        """Reinicia el estado."""
        self.agents_state.clear()
        self.tasks_state.clear()
        self.context.clear()
        self.execution_history.clear()
        print(f"[GlobalState] 🔄 Reset to empty state")
    
    # ===== AGENT STATE MANAGEMENT =====
    
    def set_agent_state(self, agent_id: str, 
                       status: Optional[str] = None,
                       last_action: Optional[str] = None,
                       last_output: Optional[str] = None,
                       error_message: Optional[str] = None) -> None:
        """Actualiza el estado de un agente."""
        with self._lock:
            if agent_id not in self.agents_state:
                self.agents_state[agent_id] = AgentState(agent_id=agent_id)
            
            state = self.agents_state[agent_id]
            if status:
                state.status = status
            if last_action:
                state.last_action = last_action
            if last_output:
                state.last_output = last_output
            if error_message:
                state.error_message = error_message
            
            state.last_updated = datetime.now().isoformat()
            self.save()
    
    def get_agent_state(self, agent_id: str) -> Optional[AgentState]:
        """Obtiene el estado de un agente."""
        with self._lock:
            return self.agents_state.get(agent_id)
    
    def get_all_agent_states(self) -> Dict[str, AgentState]:
        """Obtiene el estado de todos los agentes."""
        with self._lock:
            return dict(self.agents_state)
    
    # ===== TASK STATE MANAGEMENT =====
    
    def start_task(self, task_id: str, user_input: str) -> None:
        """Marca el inicio de una tarea."""
        with self._lock:
            task = TaskState(
                task_id=task_id,
                user_input=user_input,
                status="executing",
                started_at=datetime.now().isoformat()
            )
            self.tasks_state[task_id] = task
            self.save()
    
    def complete_task(self, task_id: str, result: str) -> None:
        """Marca una tarea como completada."""
        with self._lock:
            if task_id in self.tasks_state:
                task = self.tasks_state[task_id]
                task.status = "completed"
                task.result = result
                task.completed_at = datetime.now().isoformat()
                
                # Añade al historial
                self.execution_history.append({
                    "task_id": task_id,
                    "action": "completed",
                    "timestamp": datetime.now().isoformat()
                })
                self.save()
    
    def fail_task(self, task_id: str, error: str) -> None:
        """Marca una tarea como fallida."""
        with self._lock:
            if task_id in self.tasks_state:
                task = self.tasks_state[task_id]
                task.status = "failed"
                task.error = error
                task.completed_at = datetime.now().isoformat()
                
                self.execution_history.append({
                    "task_id": task_id,
                    "action": "failed",
                    "error": error,
                    "timestamp": datetime.now().isoformat()
                })
                self.save()
    
    def get_task_state(self, task_id: str) -> Optional[TaskState]:
        """Obtiene el estado de una tarea."""
        with self._lock:
            return self.tasks_state.get(task_id)
    
    def get_active_tasks(self) -> List[TaskState]:
        """Obtiene todas las tareas activas."""
        with self._lock:
            return [t for t in self.tasks_state.values() if t.status == "executing"]
    
    # ===== CONTEXT / GLOBAL VARIABLES =====
    
    def set_context_var(self, key: str, value: Any) -> None:
        """Establece una variable de contexto global."""
        with self._lock:
            self.context[key] = value
            self.execution_history.append({
                "action": "context_set",
                "key": key,
                "timestamp": datetime.now().isoformat()
            })
            self.save()
    
    def get_context_var(self, key: str, default: Any = None) -> Any:
        """Obtiene una variable de contexto."""
        with self._lock:
            return self.context.get(key, default)
    
    def get_all_context(self) -> Dict[str, Any]:
        """Obtiene todo el contexto."""
        with self._lock:
            return dict(self.context)
    
    def clear_context(self) -> None:
        """Limpia todo el contexto."""
        with self._lock:
            self.context.clear()
            self.save()
    
    # ===== UTILITY =====
    
    def get_agent_insights(self) -> Dict[str, Any]:
        """Retorna información útil para el Planner (contexto compartido)."""
        with self._lock:
            active_agents = [
                {
                    "id": aid,
                    "status": state.status,
                    "last_action": state.last_action
                }
                for aid, state in self.agents_state.items()
            ]
            
            return {
                "active_agents": active_agents,
                "active_tasks": len(self.get_active_tasks()),
                "context": self.context,
                "recent_history": self.execution_history[-5:]
            }
    
    def reset_session(self) -> None:
        """Limpia completamente el estado (nueva sesión)."""
        with self._lock:
            self._reset()
            self.save()
    
    def get_summary(self) -> Dict[str, Any]:
        """Retorna un resumen del estado actual."""
        with self._lock:
            return {
                "total_agents": len(self.agents_state),
                "active_tasks": len(self.get_active_tasks()),
                "total_executions": len(self.execution_history),
                "context_vars": len(self.context),
                "blackboard_file": str(BLACKBOARD_PATH),
                "last_saved": BLACKBOARD_PATH.stat().st_mtime if BLACKBOARD_PATH.exists() else None
            }


# Singleton global
_global_state = None


def get_global_state() -> GlobalState:
    """Obtiene la instancia única del estado global."""
    global _global_state
    if _global_state is None:
        _global_state = GlobalState()
    return _global_state
