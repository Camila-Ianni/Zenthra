"""
Kimi/Moonshot Context Architect
Long-term memory supervisor for large projects (>1M tokens)
Enables cross-session context reuse and knowledge persistence

Compatible with Python 3.9+
"""

import json
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
import os

logger = logging.getLogger(__name__)


class TaskType(Enum):
    """Types of tasks for routing decisions"""
    READ_MASSIVE = "read_massive"      # Large codebase analysis
    WRITE_CODE = "write_code"          # Code generation
    ARCHITECTURE = "architecture"      # Design decisions
    DEBUG = "debug"                    # Error troubleshooting
    REVIEW = "review"                  # Code review
    REFACTOR = "refactor"              # Code improvement


class ContextPriority(Enum):
    """Priority levels for context preservation"""
    CRITICAL = "critical"      # Must keep in next session
    HIGH = "high"              # Should keep if possible
    MEDIUM = "medium"          # Keep if space available
    LOW = "low"                # Can be pruned


@dataclass
class SessionSummary:
    """Summary of a completed session for context reuse"""
    session_id: str
    project_name: str
    timestamp: str
    token_count: int
    architecture_notes: Dict[str, Any] = field(default_factory=dict)
    key_decisions: List[str] = field(default_factory=list)
    patterns_discovered: List[str] = field(default_factory=list)
    technical_debt: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    critical_files: List[str] = field(default_factory=list)
    dependencies: Dict[str, str] = field(default_factory=dict)
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'SessionSummary':
        """Deserialize from dictionary"""
        return cls(**data)


@dataclass
class ContextWindow:
    """Current context window for a task"""
    task_type: TaskType
    project_size_tokens: int
    available_context_tokens: int
    current_usage: int = 0
    critical_context: List[str] = field(default_factory=list)
    
    def has_space(self, needed_tokens: int) -> bool:
        """Check if context window has space for more content"""
        return (self.current_usage + needed_tokens) <= self.available_context_tokens
    
    def remaining_tokens(self) -> int:
        """Get remaining tokens in context window"""
        return self.available_context_tokens - self.current_usage


@dataclass
class RoutingDecision:
    """Decision for routing a task to appropriate model"""
    task_type: TaskType
    recommended_model: str
    should_use_kimi: bool
    context_size: int
    kimi_reason: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        """Serialize to dictionary"""
        return asdict(self)


class KimiSupervisor:
    """
    Kimi/Moonshot supervisor for long-term memory and task routing
    Delegates large projects to specialized memory management
    """
    
    def __init__(
        self,
        blackboard_path: str = "memory/blackboard.json",
        delegation_threshold: int = 1_000_000,
    ):
        self.blackboard_path = blackboard_path
        self.delegation_threshold = delegation_threshold
        self.sessions: List[SessionSummary] = []
        self.current_context: Optional[ContextWindow] = None
        self.model_router = None
        
        self._load_sessions()
    
    def set_model_router(self, model_router) -> None:
        """Set reference to model router for delegation"""
        self.model_router = model_router
    
    def detect_project_size(self, project_context: Dict[str, Any]) -> int:
        """Estimate project size in tokens"""
        total_chars = 0
        
        if "files" in project_context:
            for file_info in project_context["files"]:
                if isinstance(file_info, dict):
                    total_chars += file_info.get("size", 0)
                elif isinstance(file_info, str):
                    total_chars += len(file_info)
        
        if "description" in project_context:
            total_chars += len(project_context["description"])
        
        if "context" in project_context:
            total_chars += len(str(project_context["context"]))
        
        estimated_tokens = int(total_chars / 3.5)
        return estimated_tokens
    
    def should_delegate_to_kimi(self, project_context: Dict[str, Any]) -> bool:
        """Determine if task should be delegated to Kimi"""
        project_size = self.detect_project_size(project_context)
        return project_size > self.delegation_threshold
    
    def route_task(
        self,
        task_type: TaskType,
        project_context: Dict[str, Any],
    ) -> RoutingDecision:
        """
        Route task to appropriate model based on type and context
        """
        project_size = self.detect_project_size(project_context)
        use_kimi = project_size > self.delegation_threshold
        
        model_mapping = {
            TaskType.READ_MASSIVE: ("glm-5", True),
            TaskType.WRITE_CODE: ("deepseek-v4", False),
            TaskType.ARCHITECTURE: ("glm-5", True),
            TaskType.DEBUG: ("gemini-2.0-flash", False),
            TaskType.REVIEW: ("glm-5", False),
            TaskType.REFACTOR: ("qwen3-coder", False),
        }
        
        base_model, high_complexity = model_mapping.get(
            task_type,
            ("gemini-2.0-flash", False)
        )
        
        if use_kimi and high_complexity:
            recommended_model = "glm-5"
            kimi_reason = f"Large project ({project_size}M+ tokens) requires {task_type.value}"
        else:
            recommended_model = base_model
            kimi_reason = None
        
        return RoutingDecision(
            task_type=task_type,
            recommended_model=recommended_model,
            should_use_kimi=use_kimi,
            context_size=project_size,
            kimi_reason=kimi_reason,
        )
    
    def create_context_window(
        self,
        task_type: TaskType,
        project_context: Dict[str, Any],
    ) -> ContextWindow:
        """Create a context window for the task"""
        project_size = self.detect_project_size(project_context)
        
        if task_type == TaskType.READ_MASSIVE:
            available_tokens = 100_000
        elif task_type == TaskType.ARCHITECTURE:
            available_tokens = 150_000
        elif task_type == TaskType.WRITE_CODE:
            available_tokens = 50_000
        else:
            available_tokens = 30_000
        
        critical_context = project_context.get("critical_files", [])
        
        return ContextWindow(
            task_type=task_type,
            project_size_tokens=project_size,
            available_context_tokens=available_tokens,
            critical_context=critical_context,
        )
    
    def build_context_from_session(
        self,
        project_name: str,
        window: ContextWindow,
    ) -> Dict[str, Any]:
        """Build context from previous sessions"""
        previous_sessions = [
            s for s in self.sessions
            if s.project_name == project_name
        ]
        
        if not previous_sessions:
            return {}
        
        latest_session = max(previous_sessions, key=lambda s: s.timestamp)
        
        context = {
            "architecture_notes": latest_session.architecture_notes,
            "key_decisions": latest_session.key_decisions,
            "patterns": latest_session.patterns_discovered,
            "technical_debt": latest_session.technical_debt,
            "critical_files": latest_session.critical_files,
            "dependencies": latest_session.dependencies,
            "previous_session_id": latest_session.session_id,
            "timestamp": latest_session.timestamp,
        }
        
        estimated_tokens = int(
            (len(json.dumps(context)) / 3.5)
        )
        
        if window.has_space(estimated_tokens):
            window.current_usage += estimated_tokens
            logger.info(
                f"✅ Loaded context from session {latest_session.session_id} "
                f"({estimated_tokens} tokens)"
            )
            return context
        
        logger.warning(
            f"⚠️ Not enough context space for previous session "
            f"({estimated_tokens} > {window.remaining_tokens()})"
        )
        return {}
    
    def save_session_summary(
        self,
        session_id: str,
        project_name: str,
        token_count: int,
        architecture_notes: Dict[str, Any],
        key_decisions: List[str],
        patterns: List[str],
        technical_debt: List[str],
        recommendations: List[str],
        critical_files: List[str],
        dependencies: Dict[str, str],
    ) -> SessionSummary:
        """Save session summary for future reference"""
        summary = SessionSummary(
            session_id=session_id,
            project_name=project_name,
            timestamp=datetime.now().isoformat(),
            token_count=token_count,
            architecture_notes=architecture_notes,
            key_decisions=key_decisions,
            patterns_discovered=patterns,
            technical_debt=technical_debt,
            recommendations=recommendations,
            critical_files=critical_files,
            dependencies=dependencies,
        )
        
        self.sessions.append(summary)
        self._save_to_blackboard()
        
        logger.info(f"✅ Saved session summary: {session_id}")
        return summary
    
    def get_context_reuse_score(self, project_name: str) -> float:
        """
        Calculate how much context can be reused for a project (0.0-1.0)
        """
        project_sessions = [
            s for s in self.sessions
            if s.project_name == project_name
        ]
        
        if not project_sessions:
            return 0.0
        
        latest = max(project_sessions, key=lambda s: s.timestamp)
        now = datetime.fromisoformat(latest.timestamp)
        age_days = (datetime.now() - now).days
        
        if age_days > 30:
            return 0.0
        elif age_days > 14:
            return 0.3
        elif age_days > 7:
            return 0.7
        else:
            return 0.9
    
    def get_stats(self) -> Dict[str, Any]:
        """Get Kimi supervisor statistics"""
        total_sessions = len(self.sessions)
        unique_projects = len(set(s.project_name for s in self.sessions))
        
        total_patterns = sum(
            len(s.patterns_discovered) for s in self.sessions
        )
        
        total_decisions = sum(
            len(s.key_decisions) for s in self.sessions
        )
        
        return {
            "total_sessions": total_sessions,
            "unique_projects": unique_projects,
            "total_patterns_learned": total_patterns,
            "total_decisions_tracked": total_decisions,
            "delegation_threshold": self.delegation_threshold,
        }
    
    def _save_to_blackboard(self) -> None:
        """Save all sessions to blackboard"""
        os.makedirs(os.path.dirname(self.blackboard_path) or ".", exist_ok=True)
        
        data = {
            "sessions": [s.to_dict() for s in self.sessions],
            "last_updated": datetime.now().isoformat(),
            "total_sessions": len(self.sessions),
        }
        
        with open(self.blackboard_path, 'w') as f:
            json.dump(data, f, indent=2, default=str)
        
        logger.debug(f"Saved {len(self.sessions)} sessions to blackboard")
    
    def _load_sessions(self) -> None:
        """Load sessions from blackboard"""
        if not os.path.exists(self.blackboard_path):
            logger.info(f"Blackboard not found: {self.blackboard_path}")
            return
        
        try:
            with open(self.blackboard_path, 'r') as f:
                data = json.load(f)
            
            self.sessions = [
                SessionSummary.from_dict(s)
                for s in data.get("sessions", [])
            ]
            
            logger.info(f"Loaded {len(self.sessions)} sessions from blackboard")
        except Exception as e:
            logger.error(f"Error loading blackboard: {str(e)}")


class KimiContextBuilder:
    """Helper class for building context efficiently"""
    
    def __init__(self, supervisor: KimiSupervisor, window: ContextWindow):
        self.supervisor = supervisor
        self.window = window
        self.context_parts: List[Tuple[str, int, ContextPriority]] = []
    
    def add_context(
        self,
        label: str,
        content: Any,
        priority: ContextPriority = ContextPriority.MEDIUM,
    ) -> bool:
        """Add context if space available"""
        content_str = str(content)
        estimated_tokens = int(len(content_str) / 3.5)
        
        if self.window.has_space(estimated_tokens):
            self.context_parts.append((label, estimated_tokens, priority))
            self.window.current_usage += estimated_tokens
            return True
        
        logger.warning(
            f"Context limit reached: cannot add {label} "
            f"({estimated_tokens} > {self.window.remaining_tokens()})"
        )
        return False
    
    def build(self) -> Dict[str, Any]:
        """Build final context dictionary"""
        sorted_parts = sorted(
            self.context_parts,
            key=lambda x: {
                ContextPriority.CRITICAL: 0,
                ContextPriority.HIGH: 1,
                ContextPriority.MEDIUM: 2,
                ContextPriority.LOW: 3,
            }[x[2]]
        )
        
        return {
            "parts": [p[0] for p in sorted_parts],
            "total_tokens": self.window.current_usage,
            "remaining_tokens": self.window.remaining_tokens(),
            "parts_by_priority": {
                "critical": [p[0] for p in sorted_parts if p[2] == ContextPriority.CRITICAL],
                "high": [p[0] for p in sorted_parts if p[2] == ContextPriority.HIGH],
                "medium": [p[0] for p in sorted_parts if p[2] == ContextPriority.MEDIUM],
                "low": [p[0] for p in sorted_parts if p[2] == ContextPriority.LOW],
            }
        }
