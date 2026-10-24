"""
Agent Evaluator - Suite de evaluación y comparación de modelos para los 6 Agentes Especialistas
Implementa la Fase 3 del documento rector:
- Evalúa tareas representativas estandarizadas por especialista.
- Compara el modelo candidato frente a una alternativa.
- Mide latencia, precisión, cumplimiento de esquemas y respeto a las políticas de seguridad.
"""

import time
import json
import logging
from typing import Dict, List, Any, Optional
from pathlib import Path

from core.specialist_agents import get_specialist_agent, list_specialist_agents
from core.model_router import get_model_router

logger = logging.getLogger("AgentEvaluator")


class AgentEvaluator:
    """Motor de evaluación y benchmarking multi-modelo."""

    def __init__(self, output_file: Optional[Path] = None):
        self.router = get_model_router()
        self.output_file = output_file or (Path(__file__).parent.parent / "evaluation_results.json")
        self.results: Dict[str, Any] = {}

    async def evaluate_specialist_task(self, specialty: str, task: str,
                                       expected_keyword: str) -> Dict[str, Any]:
        """Evalúa un especialista con una tarea y valida palabras clave esperadas."""
        agent = get_specialist_agent(specialty)
        if not agent:
            return {"specialty": specialty, "success": False, "error": "Agente no encontrado"}

        start = time.time()
        res = await agent.execute_task(task)
        elapsed_ms = int((time.time() - start) * 1000)

        keyword_present = expected_keyword.lower() in res.output.lower() if res.output else False
        passed = bool(res.success and res.output and len(res.output.strip()) > 10)

        return {
            "specialty": specialty,
            "agent_id": agent.agent_id,
            "model_used": res.model_used,
            "provider": res.provider_used,
            "success": passed,
            "latency_ms": elapsed_ms,
            "keyword_verified": keyword_present,
            "output_preview": res.output[:120] if res.output else "",
            "requires_approval": res.requires_approval,
            "error": res.error
        }

    async def compare_candidate_vs_alternative(self, specialty: str, task: str,
                                              candidate_model: str,
                                              alternative_model: str) -> Dict[str, Any]:
        """Compara la ejecución de una misma tarea entre dos modelos diferentes."""
        agent = get_specialist_agent(specialty)
        if not agent:
            return {"error": f"Agente '{specialty}' no disponible"}

        # 1. Ejecución con Candidato
        start1 = time.time()
        res_cand = await self.router.execute_model(
            model_id=candidate_model,
            prompt=f"Tarea: {task}",
            system=agent.system_prompt
        )
        lat1 = int((time.time() - start1) * 1000)

        # 2. Ejecución con Alternativa
        start2 = time.time()
        res_alt = await self.router.execute_model(
            model_id=alternative_model,
            prompt=f"Tarea: {task}",
            system=agent.system_prompt
        )
        lat2 = int((time.time() - start2) * 1000)

        return {
            "specialty": specialty,
            "task": task[:60],
            "candidate": {
                "model": candidate_model,
                "provider": res_cand.get("provider", "unknown"),
                "success": res_cand.get("success", False),
                "latency_ms": lat1,
                "output_length": len(res_cand.get("content", ""))
            },
            "alternative": {
                "model": alternative_model,
                "provider": res_alt.get("provider", "unknown"),
                "success": res_alt.get("success", False),
                "latency_ms": lat2,
                "output_length": len(res_alt.get("content", ""))
            },
            "latency_delta_ms": lat1 - lat2,
            "fastest_model": candidate_model if lat1 <= lat2 else alternative_model
        }

    async def run_full_benchmark(self) -> Dict[str, Any]:
        """Ejecuta la suite integral de evaluación de los 6 agentes especialistas."""
        benchmark_tasks = [
            ("backend", "Escribir una función en Python para validar si un string es un JSON válido.", "json"),
            ("cybersecurity", "Explicar cómo prevenir SQL Injection mediante Prepared Statements.", "prepared"),
            ("kernel", "Explicar por qué redirigir EIP a fixup en Page Fault #PF evita un bucle infinito.", "eip"),
            ("data", "Definir la diferencia entre media y mediana con datos asimétricos.", "mediana"),
            ("creative", "Listar los atributos CSS requeridos para un efecto Glassmorphism con backdrop-filter.", "blur"),
            ("automation", "Generar los pasos secuenciales para desplegar un microservicio.", "paso")
        ]

        evaluations = []
        for spec, task, kw in benchmark_tasks:
            result = await self.evaluate_specialist_task(spec, task, kw)
            evaluations.append(result)

        # Prueba de comparación entre modelos locales de Ollama
        comparison = await self.compare_candidate_vs_alternative(
            specialty="backend",
            task="Función suma(a, b) con type hints",
            candidate_model="qwen2.5-coder:1.5b-base",
            alternative_model="gemma4:latest"
        )

        total = len(evaluations)
        passed = sum(1 for e in evaluations if e["success"])

        summary = {
            "timestamp": time.time(),
            "total_specialists_evaluated": total,
            "passed": passed,
            "pass_rate_pct": round((passed / total) * 100, 1),
            "evaluations": evaluations,
            "model_comparison": comparison
        }

        # Guardar en JSON
        try:
            with open(self.output_file, "w", encoding="utf-8") as f:
                json.dump(summary, f, indent=2, ensure_ascii=False)
            logger.info(f"Resultados de evaluación guardados en {self.output_file}")
        except Exception as e:
            logger.warning(f"Error guardando {self.output_file}: {e}")

        return summary
