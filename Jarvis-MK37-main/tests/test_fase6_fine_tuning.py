"""
Tests - Fase 6: Evaluación de Viabilidad de Fine-Tuning
Valida:
1. Generación completa de reportes de viabilidad para los 6 especialistas.
2. Contrato de datos de cómputo (VRAM, costos, horas de entrenamiento).
3. Requisitos de dataset de instrucción (volumen mínimo, curación humana).
4. Evaluación de riesgos (olvido catastrófico, seguridad, deuda técnica).
5. Exportación formal de JSON de viabilidad.
"""

import os
import json
import pytest
from pathlib import Path

from core.fine_tuning_evaluator import FineTuningEvaluator


def test_fine_tuning_evaluator_covers_all_agents():
    """Verifica que el evaluador cubra exactamente a los 6 especialistas."""
    evaluator = FineTuningEvaluator()
    reports = evaluator.evaluate_all()

    assert len(reports) == 6

    expected_ids = {
        "agent_backend_specialist",
        "agent_cyber_specialist",
        "agent_kernel_specialist",
        "agent_data_specialist",
        "agent_creative_specialist",
        "agent_automation_specialist"
    }

    found_ids = {r.agent_id for r in reports}
    assert found_ids == expected_ids


def test_fine_tuning_metrics_consistency():
    """Verifica que las métricas de cómputo y dataset sean coherentes."""
    evaluator = FineTuningEvaluator()
    reports = evaluator.evaluate_all()

    for r in reports:
        # Puntuación actual basada en RAG + Tools debe ser superior a 0.90
        assert 0.90 <= r.in_context_score <= 1.0

        # Verificación de requerimientos de cómputo
        assert r.compute.qlora_vram_gb <= 16.0  # Viable en Apple Silicon (16GB/32GB)
        assert r.compute.estimated_cloud_cost_usd > 0
        assert r.compute.local_mac_mps_feasible is True

        # Requerimientos de dataset
        assert r.dataset.min_instruction_pairs >= 800
        assert r.dataset.recommended_pairs > r.dataset.min_instruction_pairs
        assert r.dataset.curation_hours_human > 0

        # En la fase actual, ningún especialista justifica fine-tuning
        assert r.fine_tuning_justified is False
        assert len(r.mitigation_strategy) > 20


def test_export_summary_json(tmp_path):
    """Verifica la exportación del JSON de evaluación de Fase 6."""
    evaluator = FineTuningEvaluator()
    out_file = tmp_path / "test_ft_eval.json"

    result_path = evaluator.export_summary_json(str(out_file))
    assert os.path.exists(result_path)

    with open(result_path, encoding="utf-8") as f:
        data = json.load(f)

    assert "global_verdict" in data
    assert "FINE-TUNING NO RECOMENDADO" in data["global_verdict"]
    assert len(data["agents"]) == 6
