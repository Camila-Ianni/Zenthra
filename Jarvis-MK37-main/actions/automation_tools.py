"""
Automation Tools - Motor de flujos de trabajo, planificación y compuerta de aprobación
Controla:
- Descomposición y secuenciación de tareas
- Verificación previa de seguridad
- Cancelación inmediata de flujos
- Compuerta de aprobación para acciones sensibles
"""

import time
from typing import Dict, List, Any, Optional, Callable


class WorkflowEngine:
    """Motor de ejecución de planes de automatización con compuerta de confirmación humana."""

    def __init__(self, max_steps: int = 10, step_timeout_sec: float = 30.0):
        self.max_steps = max_steps
        self.step_timeout_sec = step_timeout_sec
        self.is_cancelled = False
        self.execution_log: List[Dict[str, Any]] = []

    def cancel(self) -> None:
        """Cancela inmediatamente la ejecución del plan en curso."""
        self.is_cancelled = True

    def validate_plan_steps(self, steps: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Valida que los pasos del plan sean específicos, ejecutables y seguros."""
        validated = []
        requires_approval = False
        approval_reasons = []

        for idx, step in enumerate(steps, 1):
            action = step.get("action", "unknown")
            params = step.get("params", {})
            action_lower = str(action).lower()
            params_str = str(params).lower()

            # Detectar pasos potencialmente destructivos o privilegiados
            sensitive = any(w in action_lower or w in params_str for w in [
                "delete", "rm ", "drop ", "truncate", "shutdown", "reboot", "kill", "format"
            ])

            if sensitive:
                requires_approval = True
                approval_reasons.append(f"Paso {idx} ({action}): acción destructiva o sensible detectada")

            validated.append({
                "step_number": idx,
                "action": action,
                "params": params,
                "rationale": step.get("rationale", ""),
                "is_sensitive": sensitive
            })

        return {
            "total_steps": len(validated),
            "validated_steps": validated,
            "requires_human_approval": requires_approval,
            "approval_reasons": approval_reasons
        }

    def execute_plan(self, plan: Dict[str, Any], tool_executor: Optional[Callable] = None,
                     human_approved: bool = False) -> Dict[str, Any]:
        """Ejecuta los pasos de un plan supervisado."""
        self.is_cancelled = False
        self.execution_log = []
        raw_steps = plan.get("steps", [])

        validation = self.validate_plan_steps(raw_steps)
        if validation["requires_human_approval"] and not human_approved:
            return {
                "status": "AWAITING_APPROVAL",
                "message": "Plan pausado: requiere aprobación humana explícita antes de ejecutar pasos sensibles.",
                "reasons": validation["approval_reasons"],
                "steps_pending": validation["validated_steps"]
            }

        start_time = time.time()
        for step in validation["validated_steps"]:
            if self.is_cancelled:
                return {
                    "status": "CANCELLED",
                    "message": "Plan cancelado por el usuario.",
                    "completed_steps": len(self.execution_log),
                    "log": self.execution_log
                }

            action = step["action"]
            params = step["params"]
            step_record = {
                "step": step["step_number"],
                "action": action,
                "timestamp": time.time(),
                "success": True,
                "output": f"Paso {step['step_number']} ({action}) simulado/ejecutado con éxito"
            }

            if tool_executor:
                try:
                    res = tool_executor(action, params)
                    step_record["output"] = res
                except Exception as e:
                    step_record["success"] = False
                    step_record["error"] = str(e)
                    self.execution_log.append(step_record)
                    return {
                        "status": "FAILED",
                        "failed_at_step": step["step_number"],
                        "error": str(e),
                        "log": self.execution_log
                    }

            self.execution_log.append(step_record)

        return {
            "status": "COMPLETED",
            "total_steps": len(self.execution_log),
            "elapsed_seconds": round(time.time() - start_time, 3),
            "log": self.execution_log
        }
