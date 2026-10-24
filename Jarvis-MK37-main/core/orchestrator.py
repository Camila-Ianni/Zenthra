"""
Orchestrator - Gestiona el ciclo de vida multi-agente: Plan -> Execute -> Review -> Correct
"""
import json
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Optional, Union, Dict, List, Any, Callable

try:
    from google import genai
except ImportError:
    genai = None


def get_base_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent.parent


BASE_DIR = get_base_dir()


class TaskStatus(Enum):
    """Estados del ciclo de ejecución."""
    PLANNING = "planning"
    EXECUTING = "executing"
    REVIEWING = "reviewing"
    CORRECTING = "correcting"
    COMPLETED = "completed"
    FAILED = "failed"


@dataclass
class ExecutionPlan:
    """Representa un plan generado por el Planner."""
    task_id: str
    user_input: str
    plan_description: str
    steps: List[Dict[str, Any]] = field(default_factory=list)
    created_at: str = field(default_factory=lambda: datetime.now().isoformat())
    
    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class ToolCall:
    """Representa una llamada a herramienta."""
    tool_name: str
    parameters: Dict[str, Any]
    result: Optional[str] = None
    error: Optional[str] = None
    stderr: Optional[str] = None
    execution_time: float = 0.0


@dataclass
class ReviewResult:
    """Resultado de la revisión de un output."""
    is_valid: bool
    error_detected: Optional[str] = None
    severity: Optional[str] = None  # "critical", "warning", "info"
    suggestion: Optional[str] = None
    original_output: Optional[str] = None


class Orchestrator:
    """
    Orquestador central que gestiona el ciclo Plan->Execute->Review->Correct
    y delega a los 6 Agentes Especialistas de Jarvis-MK37.
    """
    
    def __init__(self, api_key: Optional[str] = None, model: str = "models/gemini-2.5-flash"):
        import os
        self.api_key = api_key or os.getenv("GOOGLE_API_KEY")
        if not self.api_key:
            try:
                from memory.config_manager import get_gemini_key
                self.api_key = get_gemini_key()
            except Exception:
                self.api_key = None
        self.model = model
        self.client = None
        if genai and self.api_key:
            try:
                self.client = genai.Client(api_key=self.api_key)
            except Exception as e:
                print(f"[Orchestrator] ⚠️ genai client init warning: {e}")
        self.status = TaskStatus.PLANNING
        self.current_plan: Optional[ExecutionPlan] = None
        self.execution_history: List[ToolCall] = []
        self.retry_count = 0
        self.max_retries = 3
        self.agents: Dict[str, Any] = {}
    
    def set_agents(self, agents: Dict[str, Any]) -> None:
        """Registra agentes disponibles."""
        self.agents = agents

    def delegate_to_specialist(self, specialty: str, task: str, context: Optional[str] = None) -> Dict[str, Any]:
        """Delega una tarea directamente al agente especialista correspondiente."""
        from core.specialist_agents import get_specialist_agent
        agent = get_specialist_agent(specialty)
        if not agent:
            return {"success": False, "error": f"Especialista '{specialty}' no encontrado"}
        import asyncio
        import concurrent.futures
        try:
            loop = asyncio.get_event_loop()
            if loop.is_running():
                with concurrent.futures.ThreadPoolExecutor() as pool:
                    res = pool.submit(asyncio.run, agent.execute_task(task, context)).result()
            else:
                res = loop.run_until_complete(agent.execute_task(task, context))
        except Exception:
            res = asyncio.run(agent.execute_task(task, context))
        return asdict(res)
    
    def plan(self, user_input: str, context: Optional[str] = None) -> ExecutionPlan:
        """
        PASO 1: Plan - Genera un plan ejecutable basado en input del usuario.
        """
        self.status = TaskStatus.PLANNING
        print(f"[Orchestrator] 📋 Planning: {user_input[:50]}...")
        
        prompt = f"""Eres un planificador de tareas. Tu trabajo es descomponer una solicitud del usuario en pasos específicos.

User Input: {user_input}
{f"Context: {context}" if context else ""}

Genera un plan JSON con estructura:
{{
  "description": "Descripción breve del plan",
  "steps": [
    {{"step": 1, "action": "nombre_herramienta", "params": {{}}, "rationale": "porqué"}}
  ],
  "estimated_complexity": "low|medium|high"
}}

IMPORTANTE: Cada step debe ser específico y ejecutable."""

        # Intento con Gemini si está configurado
        if self.client:
            try:
                response = self.client.models.generate_content(
                    model=self.model,
                    contents=prompt
                )
                text = response.text
                start = text.find("{")
                end = text.rfind("}") + 1
                plan_json = json.loads(text[start:end])
                plan = ExecutionPlan(
                    task_id=f"task_{datetime.now().timestamp()}",
                    user_input=user_input,
                    plan_description=plan_json.get("description", ""),
                    steps=plan_json.get("steps", [])
                )
                self.current_plan = plan
                print(f"[Orchestrator] ✅ Plan ready: {len(plan.steps)} steps")
                return plan
            except Exception as e:
                print(f"[Orchestrator] ⚠️ Gemini planning error: {e}. Trying fallback...")

        # Fallback mediante ModelRouter / planificador local
        try:
            from core.model_router import get_model_router
            import asyncio
            router = get_model_router()
            res = asyncio.run(router.execute_model(
                model_id="qwen2.5-coder:1.5b-base",
                prompt=prompt,
                system="Eres un planificador que responde en formato JSON válido."
            ))
            if res.get("success") and res.get("content"):
                text = res["content"]
                start = text.find("{")
                end = text.rfind("}") + 1
                if start != -1 and end > start:
                    plan_json = json.loads(text[start:end])
                    plan = ExecutionPlan(
                        task_id=f"task_{datetime.now().timestamp()}",
                        user_input=user_input,
                        plan_description=plan_json.get("description", "Plan estructurado"),
                        steps=plan_json.get("steps", [])
                    )
                    self.current_plan = plan
                    print(f"[Orchestrator] ✅ Local Plan ready: {len(plan.steps)} steps")
                    return plan
        except Exception as e:
            print(f"[Orchestrator] ⚠️ Local router planning error: {e}")

        # Plan base heurístico determinista si no hay modelo disponible
        plan = ExecutionPlan(
            task_id=f"task_{datetime.now().timestamp()}",
            user_input=user_input,
            plan_description=f"Plan determinista para: {user_input[:40]}",
            steps=[{"step": 1, "action": "code_helper", "params": {"description": user_input}, "rationale": "Ejecución por especialista"}]
        )
        self.current_plan = plan
        return plan
    
    def execute(self, plan: ExecutionPlan, tool_executor: Callable) -> List[ToolCall]:
        """
        PASO 2: Execute - Ejecuta los steps del plan usando tool_executor.
        """
        self.status = TaskStatus.EXECUTING
        self.execution_history = []
        print(f"[Orchestrator] 🚀 Executing plan: {plan.task_id}")
        
        for i, step in enumerate(plan.steps):
            tool_name = step.get("action", "unknown")
            params = step.get("params", {})
            
            print(f"[Orchestrator] → Step {i+1}: {tool_name}")
            
            try:
                # Ejecuta la herramienta
                result = tool_executor(tool_name, params)
                
                tool_call = ToolCall(
                    tool_name=tool_name,
                    parameters=params,
                    result=result.get("output") if isinstance(result, dict) else str(result)
                )
                
                # Captura stderr si existe
                if isinstance(result, dict) and result.get("stderr"):
                    tool_call.stderr = result["stderr"]
                    tool_call.error = result.get("error")
                
                self.execution_history.append(tool_call)
                
            except Exception as e:
                print(f"[Orchestrator] ⚠️ Tool execution error: {e}")
                tool_call = ToolCall(
                    tool_name=tool_name,
                    parameters=params,
                    error=str(e)
                )
                self.execution_history.append(tool_call)
        
        return self.execution_history
    
    def review(self, execution_results: List[ToolCall]) -> List[ReviewResult]:
        """
        PASO 3: Review - Revisa los resultados y detecta errores.
        """
        self.status = TaskStatus.REVIEWING
        print(f"[Orchestrator] 👁️ Reviewing {len(execution_results)} results...")
        
        review_results: List[ReviewResult] = []
        
        for tool_call in execution_results:
            # Detecta errores en: error field, stderr, o si result es None
            if tool_call.error or tool_call.stderr:
                review_result = ReviewResult(
                    is_valid=False,
                    error_detected=tool_call.error or tool_call.stderr,
                    severity="critical" if tool_call.error else "warning",
                    original_output=tool_call.result
                )
                print(f"[Orchestrator] ⚠️ Error in {tool_call.tool_name}: {tool_call.error}")
                
            elif tool_call.result is None:
                review_result = ReviewResult(
                    is_valid=False,
                    error_detected="No output from tool",
                    severity="warning"
                )
                
            else:
                review_result = ReviewResult(
                    is_valid=True,
                    original_output=tool_call.result
                )
            
            review_results.append(review_result)
        
        return review_results
    
    def correct(self, 
               tool_call: ToolCall, 
               review_result: ReviewResult,
               tool_executor: Callable) -> Optional[ToolCall]:
        """
        PASO 4: Correct - Genera y aplica una corrección automáticamente.
        """
        self.status = TaskStatus.CORRECTING
        
        if self.retry_count >= self.max_retries:
            print(f"[Orchestrator] ❌ Max retries ({self.max_retries}) reached")
            return None
        
        self.retry_count += 1
        print(f"[Orchestrator] 🔧 Attempting correction (retry {self.retry_count}/{self.max_retries})")
        
        prompt = f"""Eres un debugger de errores. Analiza este error y propón una corrección.

Tool: {tool_call.tool_name}
Original Params: {json.dumps(tool_call.parameters, indent=2)}
Error: {review_result.error_detected}

Proporciona:
1. Root cause (en 1-2 líneas)
2. Corrected params (JSON)

Formato:
{{
  "root_cause": "...",
  "corrected_params": {{ ... }}
}}"""

        try:
            response = self.client.models.generate_content(
                model=self.model,
                contents=prompt
            )
            
            text = response.text
            start = text.find("{")
            end = text.rfind("}") + 1
            correction = json.loads(text[start:end])
            
            corrected_params = correction.get("corrected_params", tool_call.parameters)
            print(f"[Orchestrator] 💡 Correction generated, retrying...")
            
            # Reintenta con los parámetros corregidos
            result = tool_executor(tool_call.tool_name, corrected_params)
            
            corrected_call = ToolCall(
                tool_name=tool_call.tool_name,
                parameters=corrected_params,
                result=result.get("output") if isinstance(result, dict) else str(result)
            )
            
            if isinstance(result, dict):
                corrected_call.stderr = result.get("stderr")
                corrected_call.error = result.get("error")
            
            return corrected_call
            
        except Exception as e:
            print(f"[Orchestrator] ❌ Correction failed: {e}")
            return None
    
    def run(self, user_input: str, 
           context: Optional[str] = None,
           tool_executor: Optional[Callable] = None) -> Dict[str, Any]:
        """
        Ejecuta el ciclo completo: Plan -> Execute -> Review -> Correct
        """
        if tool_executor is None:
            raise ValueError("tool_executor callback required")
        
        try:
            # PASO 1: Planificar
            plan = self.plan(user_input, context)
            
            # PASO 2: Ejecutar
            execution_results = self.execute(plan, tool_executor)
            
            # PASO 3: Revisar
            review_results = self.review(execution_results)
            
            # PASO 4: Corregir si hay errores
            corrected_results = []
            for tool_call, review_result in zip(execution_results, review_results):
                if not review_result.is_valid:
                    corrected = self.correct(tool_call, review_result, tool_executor)
                    if corrected:
                        corrected_results.append(corrected)
                        # Re-revisar el resultado corregido
                        self.retry_count = 0  # Reset para siguientes errores
                    else:
                        corrected_results.append(tool_call)
                else:
                    corrected_results.append(tool_call)
            
            self.status = TaskStatus.COMPLETED
            
            return {
                "status": "completed",
                "plan": plan.to_dict(),
                "results": [asdict(tc) for tc in corrected_results],
                "review_summary": self._summarize_results(corrected_results)
            }
            
        except Exception as e:
            self.status = TaskStatus.FAILED
            print(f"[Orchestrator] ❌ Execution failed: {e}")
            return {
                "status": "failed",
                "error": str(e)
            }
    
    def _summarize_results(self, results: List[ToolCall]) -> Dict[str, Any]:
        """Genera un resumen de los resultados."""
        successful = sum(1 for r in results if not r.error)
        failed = len(results) - successful
        
        return {
            "total_steps": len(results),
            "successful": successful,
            "failed": failed,
            "final_outputs": [r.result for r in results if r.result]
        }
    
    # ============= PHASE 6: OPENMYTHOS INTEGRATION =============
    
    def enable_swarm_mode(self) -> None:
        """Enable OpenMythos swarm orchestration for complex tasks"""
        try:
            from core.swarm_orchestrator import SwarmOrchestrator
            from core.knowledge_graph import KnowledgeGraph
            
            self.swarm_orchestrator = SwarmOrchestrator(model_router=None)
            self.knowledge_graph = KnowledgeGraph()
            
            print("[Orchestrator] 🧠 OpenMythos Swarm Mode ENABLED")
        except ImportError as e:
            print(f"[Orchestrator] ⚠️ Could not enable swarm mode: {e}")
    
    def is_complex_task(self, user_input: str) -> bool:
        """Detect if task requires swarm decomposition"""
        complexity_indicators = [
            "architecture", "design", "refactor", "plan",
            "analyze", "investigate", "multiple", "parallel"
        ]
        
        return any(indicator in user_input.lower() for indicator in complexity_indicators)
    
    async def execute_with_swarm(self, user_input: str, context: Optional[str] = None) -> Dict[str, Any]:
        """Execute task using swarm orchestration"""
        if not hasattr(self, 'swarm_orchestrator'):
            self.enable_swarm_mode()
        
        task_context = {
            "user_input": user_input,
            "context": context,
            "timestamp": datetime.now().isoformat(),
        }
        
        result = await self.swarm_orchestrator.decompose_and_execute(
            main_task=user_input,
            task_context=task_context,
        )
        
        return result.to_dict()
