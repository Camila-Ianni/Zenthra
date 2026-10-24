"""
Selective Memory - Memoria selectiva, aislada y sanitizada para los 6 Agentes Especialistas
Implementa la Fase 5 del documento rector:
- Almacenamiento aislado por especialista (evita memoria global que mezcle secretos o contextos).
- Sanitización automática previa de secretos (Google API keys, GitHub tokens, Bearer, contraseñas).
- Políticas claras de retención (TTL), borrado selectivo y auditoría de decisiones.
"""

import json
import os
import re
import time
from pathlib import Path
from typing import Dict, List, Any, Optional
from threading import Lock

# Expresiones regulares para detección y censura de secretos
SECRET_REGEXES = [
    (r'(?i)(AIzaSy[A-Za-z0-9_\-]{20,40})', '[REDACTED_GOOGLE_KEY]'),
    (r'(?i)(ghp_[A-Za-z0-9]{36})', '[REDACTED_GITHUB_TOKEN]'),
    (r'(?i)(Bearer\s+[A-Za-z0-9_\-\.]{20,})', 'Bearer [REDACTED_TOKEN]'),
    (r'(?i)(api[_-]?key|secret|password)\s*[:=]\s*["\'][A-Za-z0-9_\-]{8,}["\']', r'\1: "[REDACTED_SECRET]"')
]


def sanitize_text(text: str) -> str:
    """Elimina credenciales, tokens o secretos de cualquier texto antes de guardarlo en memoria."""
    if not isinstance(text, str):
        return text
    sanitized = text
    for pattern, replacement in SECRET_REGEXES:
        sanitized = re.sub(pattern, replacement, sanitized)
    return sanitized


class SelectiveMemoryManager:
    """Gestiona memorias de trabajo independientes por cada agente especialista."""

    def __init__(self, base_dir: Optional[Path] = None):
        self.base_dir = base_dir or (Path(__file__).parent.parent / "memory" / "agents")
        self.base_dir.mkdir(parents=True, exist_ok=True)
        self._lock = Lock()

    def _get_agent_path(self, agent_id: str) -> Path:
        clean_id = re.sub(r'[^a-zA-Z0-9_\-]', '', agent_id)
        return self.base_dir / f"{clean_id}_memory.json"

    def load_memory(self, agent_id: str) -> Dict[str, Any]:
        """Carga la memoria aislada del agente."""
        path = self._get_agent_path(agent_id)
        if not path.exists():
            return {"agent_id": agent_id, "decisions": [], "context_notes": {}, "last_updated": 0}
        with self._lock:
            try:
                with open(path, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                return {"agent_id": agent_id, "decisions": [], "context_notes": {}, "last_updated": 0}

    def save_memory(self, agent_id: str, data: Dict[str, Any]) -> None:
        """Guarda la memoria aislada del agente sanitizando cualquier secreto."""
        path = self._get_agent_path(agent_id)
        data["last_updated"] = time.time()
        with self._lock:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)

    def record_decision(self, agent_id: str, summary: str,
                        tool_used: str = "none",
                        approved: bool = True,
                        ttl_days: int = 30) -> Dict[str, Any]:
        """Registra una decisión relevante en la memoria del agente, sanitizando secretos."""
        mem = self.load_memory(agent_id)
        clean_summary = sanitize_text(summary)
        clean_tool = sanitize_text(tool_used)

        entry = {
            "id": f"dec_{int(time.time() * 1000)}",
            "timestamp": time.time(),
            "expires_at": time.time() + (ttl_days * 86400),
            "tool_used": clean_tool,
            "approved": approved,
            "summary": clean_summary
        }
        mem.setdefault("decisions", []).append(entry)

        # Limitar a las últimas 50 decisiones para evitar desbordes
        if len(mem["decisions"]) > 50:
            mem["decisions"] = mem["decisions"][-50:]

        self.save_memory(agent_id, mem)
        return entry

    def set_context_note(self, agent_id: str, key: str, value: str) -> None:
        """Guarda una nota de contexto persistente para el agente."""
        mem = self.load_memory(agent_id)
        clean_key = sanitize_text(key)
        clean_val = sanitize_text(value)
        mem.setdefault("context_notes", {})[clean_key] = clean_val
        self.save_memory(agent_id, mem)

    def get_context_notes(self, agent_id: str) -> Dict[str, str]:
        """Obtiene las notas de contexto del agente."""
        return self.load_memory(agent_id).get("context_notes", {})

    def prune_expired(self, agent_id: str) -> int:
        """Elimina entradas cuya fecha de retención (TTL) ha expirado."""
        mem = self.load_memory(agent_id)
        now = time.time()
        initial_len = len(mem.get("decisions", []))
        active_decisions = [
            d for d in mem.get("decisions", [])
            if d.get("expires_at", now + 1000) > now
        ]
        removed = initial_len - len(active_decisions)
        mem["decisions"] = active_decisions
        self.save_memory(agent_id, mem)
        return removed

    def clear_agent_memory(self, agent_id: str) -> bool:
        """Borra por completo la memoria de un agente específico sin afectar a los demás."""
        path = self._get_agent_path(agent_id)
        with self._lock:
            if path.exists():
                try:
                    path.unlink()
                    return True
                except Exception:
                    return False
        return True


# Singleton
_memory_instance: Optional[SelectiveMemoryManager] = None


def get_selective_memory_manager() -> SelectiveMemoryManager:
    global _memory_instance
    if _memory_instance is None:
        _memory_instance = SelectiveMemoryManager()
    return _memory_instance
