"""
Specialist Agents Framework - 6 Agentes Especialistas de Jarvis-MK37

1. Backend: Python, PHP, JS, SQL, APIs REST, pruebas y revisión de repositorios. (Candidato: Qwen3-Coder / qwen3.5 local)
2. Ciberseguridad Defensiva: Auditoría de código, secretos, CVEs y mitigaciones en modo solo lectura. (Candidato: DeepSeek-R1)
3. Kernel y Sistemas: C/ASM, interrupciones, paginación, scheduler y drivers en sandbox aislado. (Candidato: Kimi K2)
4. Datos: SQL, análisis descriptivo CSV, transformaciones y visualizaciones reproducibles. (Candidato: Gemma 4 / gemma4 local)
5. Creativo / UI: Temas, Glassmorphism, widgets, semáforos, dock y accesibilidad. (Candidato: Mistral Small 4)
6. Automatización: Descomposición de planes, workflows y confirmación de acciones sensibles. (Candidato: GLM-4.5-Air)
"""

import os
import time
import logging
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple

from core.model_router import get_model_router

logger = logging.getLogger("SpecialistAgents")


@dataclass
class SpecialistEvaluationResult:
    """Resultado de evaluación de una tarea por un especialista."""
    task_name: str
    agent_id: str
    model_used: str
    provider_used: str
    success: bool
    output: str
    tools_executed: List[str] = field(default_factory=list)
    latency_ms: int = 0
    requires_approval: bool = False
    evidence: str = ""
    error: Optional[str] = None


Tuple_Safety = Tuple[bool, bool, str]  # (is_safe, requires_approval, reason)


class SpecialistAgent(ABC):
    """Contrato base uniforme para los 6 Agentes Especialistas."""

    def __init__(self,
                 agent_id: str,
                 name: str,
                 specialty: str,
                 candidate_model: str,
                 allowed_tools: List[str],
                 system_prompt: str,
                 safe_mode: bool = True):
        self.agent_id = agent_id
        self.name = name
        self.specialty = specialty
        self.candidate_model = candidate_model
        self.allowed_tools = set(allowed_tools)
        self.system_prompt = system_prompt
        self.safe_mode = safe_mode
        self.router = get_model_router()
        from core.selective_memory import get_selective_memory_manager
        from core.specialist_rag import get_specialist_rag
        self.memory = get_selective_memory_manager()
        self.rag = get_specialist_rag()

    def get_configured_model(self) -> str:
        """Determina el modelo exacto a usar según variables de entorno o local."""
        env_var = f"AGENT_{self.specialty.upper()}_MODEL"
        override = os.getenv(env_var)
        if override:
            return override
        cand = self.candidate_model
        # Si el modelo candidato remoto no tiene API key configurada en entorno, usar local
        if "deepseek" in cand and not os.getenv("OPENROUTER_API_KEY"):
            return "qwen2.5-coder:1.5b-base"
        if "kimi" in cand and not os.getenv("OPENROUTER_API_KEY"):
            return "qwen2.5-coder:1.5b-base"
        if "glm" in cand and not os.getenv("ZHIPU_API_KEY"):
            return "qwen2.5-coder:1.5b-base"
        if "mistral" in cand:
            return "qwen2.5-coder:1.5b-base"
        return self.candidate_model

    def validate_tool_permission(self, tool_name: str) -> bool:
        """Principio de privilegios mínimos: valida si la herramienta está permitida."""
        return tool_name in self.allowed_tools

    @abstractmethod
    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        """Verifica restricciones de seguridad previas a la ejecución."""
        pass

    async def execute_task(self, task_description: str,
                           context: Optional[str] = None,
                           tools_executor: Optional[Any] = None) -> SpecialistEvaluationResult:
        """Ejecuta una tarea con el especialista respetando su sandbox y modelo."""
        start_time = time.time()
        is_safe, requires_approval, safety_reason = self.verify_safety_policy(task_description)

        if not is_safe:
            return SpecialistEvaluationResult(
                task_name=task_description[:50],
                agent_id=self.agent_id,
                model_used="none",
                provider_used="policy_gate",
                success=False,
                output="",
                error=f"Bloqueo de seguridad: {safety_reason}",
                requires_approval=True
            )

        model_name = self.get_configured_model()
        rag_doc = self.rag.get_specialist_knowledge(self.specialty, task_description)
        context_notes = self.memory.get_context_notes(self.agent_id)
        notes_str = "\n".join([f"- {k}: {v}" for k, v in context_notes.items()]) if context_notes else ""

        full_system = f"{self.system_prompt}\n\n{rag_doc}\n[REGLA DE AISLAMIENTO: Opera con permisos mínimos. No declares éxito sin evidencia verificable.]"
        if notes_str:
            full_system += f"\n[NOTAS DE MEMORIA PREVIAS]:\n{notes_str}"

        # Inferencia mediante ModelRouter
        prompt = f"Tarea del usuario:\n{task_description}\n"
        if context:
            prompt += f"\nContexto del proyecto:\n{context}\n"

        response = await self.router.execute_model(
            model_id=model_name,
            prompt=prompt,
            system=full_system,
            temperature=0.3 if self.safe_mode else 0.7
        )

        elapsed_ms = int((time.time() - start_time) * 1000)

        if not response.get("success"):
            return SpecialistEvaluationResult(
                task_name=task_description[:50],
                agent_id=self.agent_id,
                model_used=response.get("model", model_name),
                provider_used=response.get("provider", "unknown"),
                success=False,
                output="",
                latency_ms=elapsed_ms,
                error=response.get("error", "Error de inferencia en modelo")
            )

        content = response.get("content", "")
        tools_used = []

        # Registrar decisión en memoria selectiva aislada (sin secretos)
        self.memory.record_decision(
            agent_id=self.agent_id,
            summary=f"Tarea: {task_description[:160]} -> {content[:100]}",
            tool_used="inference",
            approved=not requires_approval
        )

        return SpecialistEvaluationResult(
            task_name=task_description[:50],
            agent_id=self.agent_id,
            model_used=response.get("model", model_name),
            provider_used=response.get("provider", "unknown"),
            success=True,
            output=content,
            tools_executed=tools_used,
            latency_ms=elapsed_ms,
            requires_approval=requires_approval,
            evidence=f"Respuesta generada por {response.get('model')} vía {response.get('provider')} en {elapsed_ms}ms"
        )


# =====================================================================
# 1. Agente Backend (Qwen3-Coder / Qwen local)
# =====================================================================
class BackendSpecialistAgent(SpecialistAgent):
    def __init__(self):
        super().__init__(
            agent_id="agent_backend_specialist",
            name="Backend & APIs Specialist",
            specialty="backend",
            candidate_model=os.getenv("AGENT_BACKEND_MODEL", "qwen2.5-coder:1.5b-base"),
            allowed_tools=[
                "code_helper", "linter_agent", "run_tests", "file_controller", "git_status"
            ],
            system_prompt=(
                "Eres el Especialista Backend de Zenthra OS / Jarvis.\n"
                "Misión: Desarrollar, depurar y optimizar lógica en Python, PHP, JS, SQL y APIs REST.\n"
                "Reglas de operación:\n"
                "- Resuelve tareas de código agregando pruebas y explicando cambios.\n"
                "- Nunca declares éxito sin mostrar resultados o evidencia de ejecución.\n"
                "- Entrega código modular, tipado y testeable."
            ),
            safe_mode=False
        )

    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        # Detectar comandos potencialmente destructivos
        desc_lower = task_description.lower()
        if any(w in desc_lower for w in ["drop database", "rm -rf /", "truncate table"]):
            return (False, True, "Operación destructiva de base de datos o sistema de archivos detectada")
        return (True, False, "")


# =====================================================================
# 2. Agente Ciberseguridad Defensiva (DeepSeek-R1)
# =====================================================================
class CybersecuritySpecialistAgent(SpecialistAgent):
    def __init__(self):
        super().__init__(
            agent_id="agent_cyber_specialist",
            name="Defensive Cybersecurity Specialist",
            specialty="cybersecurity",
            candidate_model=os.getenv("AGENT_CYBER_MODEL", "deepseek/deepseek-r1:free"),
            allowed_tools=[
                "security_scanner", "secret_leak_checker", "dependency_audit", "read_file"
            ],
            system_prompt=(
                "Eres el Especialista de Ciberseguridad Defensiva de Zenthra OS.\n"
                "Misión: Análisis estático de vulnerabilidades, auditoría de dependencias, detección de fugas de secretos y configuraciones inseguras.\n"
                "Reglas de operación:\n"
                "- Modo SOLO LECTURA por defecto. No explotes sistemas ni ejecutes payloads ofensivos.\n"
                "- Estructura siempre los hallazgos en: Hallazgo, Severidad (Crítica/Alta/Media/Baja), Evidencia, Impacto, Pasos de Reproducción Seguros y Mitigación Sugerida.\n"
                "- Prioriza la protección de credenciales y aislamiento de procesos."
            ),
            safe_mode=True
        )

    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        desc_lower = task_description.lower()
        if any(w in desc_lower for w in ["exploit", "ddos", "crack password", "port scan external", "brute force"]):
            return (False, False, "Acción ofensiva prohibida por la política defensiva del agente")
        return (True, False, "")


# =====================================================================
# 3. Agente Kernel y Sistemas (Kimi K2)
# =====================================================================
class KernelSystemsSpecialistAgent(SpecialistAgent):
    def __init__(self):
        super().__init__(
            agent_id="agent_kernel_specialist",
            name="Kernel & Systems Specialist",
            specialty="kernel",
            candidate_model=os.getenv("AGENT_KERNEL_MODEL", "kimi-k2"),
            allowed_tools=[
                "c_asm_linter", "diff_generator", "sandbox_compiler", "verify_qemu_test"
            ],
            system_prompt=(
                "Eres el Especialista de Kernel y Sistemas x86 de Zenthra OS.\n"
                "Misión: Arquitectura de bajo nivel en C y ensamblador x86 (arranque Multiboot, GDT/IDT, paginación PMM/VMM, syscalls, scheduler y drivers VGA/VESA/PS2).\n"
                "Reglas de operación:\n"
                "- Modo SANDBOX estricto. Nunca intentes modificar la memoria del kernel en caliente ni escribir directamente a discos físicos.\n"
                "- Toda modificación requiere diff, explicación técnica, verificación de compilación y pruebas.\n"
                "- Nunca afirmes haber resuelto un fallo de CPU (#PF, #GP) basándote solo en pruebas simuladas."
            ),
            safe_mode=True
        )

    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        desc_lower = task_description.lower()
        if any(w in desc_lower for w in ["flash bios", "write /dev/sda", "ring 0 hotpatch", "overwrite mbr"]):
            return (False, True, "Modificación destructiva de hardware o almacenamiento físico bloqueada")
        return (True, False, "")


# =====================================================================
# 4. Agente de Datos (Gemma 4)
# =====================================================================
class DataSpecialistAgent(SpecialistAgent):
    def __init__(self):
        super().__init__(
            agent_id="agent_data_specialist",
            name="Data & Analytics Specialist",
            specialty="data",
            candidate_model=os.getenv("AGENT_DATA_MODEL", "gemma4:latest"),
            allowed_tools=[
                "csv_analyzer", "sql_validator", "stats_calculator", "read_data_file"
            ],
            system_prompt=(
                "Eres el Especialista de Datos y Analítica de Zenthra OS.\n"
                "Misión: Consultas SQL, validación de datasets, análisis descriptivo de CSV, transformaciones y métricas estadísticas.\n"
                "Reglas de operación:\n"
                "- Muestra supuestos, metodología, resultados reproducibles y límites de los datos.\n"
                "- Prohibido inventar filas, imputar métricas sin declarar o simular distribuciones inexistentes."
            ),
            safe_mode=False
        )

    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        return (True, False, "")


# =====================================================================
# 5. Agente Creativo / UI (Mistral Small 4)
# =====================================================================
class CreativeUISpecialistAgent(SpecialistAgent):
    def __init__(self):
        super().__init__(
            agent_id="agent_creative_specialist",
            name="Creative & UI Specialist",
            specialty="creative",
            candidate_model=os.getenv("AGENT_CREATIVE_MODEL", "mistral-small"),
            allowed_tools=[
                "ui_theme_generator", "css_component_builder", "accessibility_checker"
            ],
            system_prompt=(
                "Eres el Especialista Creativo y de Interfaz de Usuario (UI/UX) de Zenthra OS.\n"
                "Misión: Diseño de componentes Glassmorphism, paletas índigo/cósmico, semáforos macOS, docks flotantes, accesibilidad y copy de producto.\n"
                "Reglas de operación:\n"
                "- Respeta la estética acordada (Glassmorphism, sombras 3D de elevación, bordes redondeados y contraste WCAG AA).\n"
                "- Entrega componentes claros, reutilizables y sin alterar funcionalidades de backend."
            ),
            safe_mode=False
        )

    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        return (True, False, "")


# =====================================================================
# 6. Agente de Automatización (GLM-4.5-Air)
# =====================================================================
class AutomationSpecialistAgent(SpecialistAgent):
    def __init__(self):
        super().__init__(
            agent_id="agent_automation_specialist",
            name="Automation & Workflows Specialist",
            specialty="automation",
            candidate_model=os.getenv("AGENT_AUTOMATION_MODEL", "glm-4.5-air"),
            allowed_tools=[
                "task_decomposer", "workflow_runner", "plan_validator", "cancel_task"
            ],
            system_prompt=(
                "Eres el Especialista de Automatización y Workflows de Zenthra OS.\n"
                "Misión: Descomponer peticiones complejas en planes paso a paso ejecutables, coordinar herramientas y gestionar estados de aprobación.\n"
                "Reglas de operación:\n"
                "- Devuelve planes estructurados en JSON con pasos específicos y trazables.\n"
                "- Exige aprobación humana explícita antes de ejecutar pasos sensibles o destructivos.\n"
                "- Soporta cancelación de planes en cualquier momento mediante botón o comando de aborto."
            ),
            safe_mode=True
        )

    def verify_safety_policy(self, task_description: str) -> Tuple_Safety:
        desc_lower = task_description.lower()
        if any(w in desc_lower for w in ["delete all", "kill -9", "shutdown -h now"]):
            return (True, True, "Paso de automatización requiere aprobación humana explícita")
        return (True, False, "")


# =====================================================================
# Factoría de Agentes Especialistas
# =====================================================================
_SPECIALISTS: Dict[str, SpecialistAgent] = {}


def get_specialist_agent(specialty: str) -> Optional[SpecialistAgent]:
    """Obtiene una instancia del agente especialista solicitado."""
    global _SPECIALISTS
    spec_clean = specialty.lower().strip()
    if not _SPECIALISTS:
        _SPECIALISTS = {
            "backend": BackendSpecialistAgent(),
            "cybersecurity": CybersecuritySpecialistAgent(),
            "cyber": CybersecuritySpecialistAgent(),
            "kernel": KernelSystemsSpecialistAgent(),
            "systems": KernelSystemsSpecialistAgent(),
            "data": DataSpecialistAgent(),
            "creative": CreativeUISpecialistAgent(),
            "ui": CreativeUISpecialistAgent(),
            "automation": AutomationSpecialistAgent(),
        }
    return _SPECIALISTS.get(spec_clean)


def list_specialist_agents() -> List[SpecialistAgent]:
    """Lista los 6 agentes especialistas únicos."""
    return [
        get_specialist_agent("backend"),
        get_specialist_agent("cybersecurity"),
        get_specialist_agent("kernel"),
        get_specialist_agent("data"),
        get_specialist_agent("creative"),
        get_specialist_agent("automation"),
    ]
