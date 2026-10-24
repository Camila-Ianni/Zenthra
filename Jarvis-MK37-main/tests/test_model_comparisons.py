"""
Tests - Model Comparisons & Evaluation Benchmark (Fase 3 & Fase 4)
Verifica:
1. Comparación cuantitativa entre modelo candidato y alternativo.
2. Funcionamiento de las herramientas especializadas de cada agente:
   - Security Scanner (Ciberseguridad)
   - Kernel Tools (Kernel y Sistemas)
   - Data Tools (Datos)
   - UI Tools (Creativo/UI)
   - Automation Tools (Automatización)
3. Evaluación y guardado del benchmark en evaluation_results.json.
"""

import pytest
import asyncio
from pathlib import Path

from core.agent_evaluator import AgentEvaluator
from actions.security_scanner import scan_code_security, audit_dependency_file
from actions.kernel_tools import validate_multiboot_header, audit_page_fault_handler, generate_kernel_diff
from actions.data_tools import analyze_csv_structure, compute_descriptive_statistics, validate_sql_syntax
from actions.ui_tools import get_zenthra_palette, generate_glassmorphism_css, calculate_wcag_contrast
from actions.automation_tools import WorkflowEngine


@pytest.mark.unit
def test_tool_security_scanner():
    """Valida que el Security Scanner detecte secretos y SQL injection."""
    vulnerable_code = """
import os
GOOGLE_KEY = "AIzaSyAw_1234567890abcdefghijklmnopq"
def get_user(user_id):
    query = "SELECT * FROM users WHERE id = " + user_id
    os.system("echo " + user_id)
"""
    report = scan_code_security(vulnerable_code)
    assert report["is_clean"] is False
    assert report["critical_count"] >= 1  # Google API key
    assert report["high_count"] >= 1      # SQL injection


@pytest.mark.unit
def test_tool_kernel_multiboot_and_page_fault():
    """Valida herramientas de Kernel: Multiboot magic y auditoría de Page Fault."""
    asm_multiboot = """
    .set MULTIBOOT_HEADER_MAGIC, 0x1BADB002
    .set MULTIBOOT_HEADER_FLAGS, (1 << 0) | (1 << 1) | (1 << 2)
    .set MULTIBOOT_CHECKSUM, -(0x1BADB002 + ((1 << 0) | (1 << 1) | (1 << 2)))
    """
    res = validate_multiboot_header(asm_multiboot)
    assert res["is_valid"] is True
    assert res["vesa_lfb_requested"] is True

    # Auditoría de Page Fault seguro con fixup
    safe_handler = """
    if (copy_user_active) {
        regs->eip = (uint32_t)&vmm_user_copy_fixup;
        return -EFAULT;
    }
    """
    audit = audit_page_fault_handler(safe_handler)
    assert audit["is_safe_recovery"] is True


@pytest.mark.unit
def test_tool_data_statistics():
    """Valida cálculo estadístico descriptivo exacto sin alucinación."""
    data = [10.0, 20.0, 30.0, 40.0, 50.0]
    stats = compute_descriptive_statistics(data)
    assert stats["count"] == 5
    assert stats["mean"] == 30.0
    assert stats["median"] == 30.0
    assert stats["min"] == 10.0
    assert stats["max"] == 50.0

    # CSV
    csv_sample = "id,name,role\n1,Alice,admin\n2,Bob,dev\n"
    csv_res = analyze_csv_structure(csv_sample)
    assert csv_res["total_rows"] == 2
    assert csv_res["total_columns"] == 3


@pytest.mark.unit
def test_tool_ui_glassmorphism_and_contrast():
    """Valida generación de CSS Glassmorphism y contraste WCAG 2.1."""
    css_res = generate_glassmorphism_css(blur_px=20, opacity=0.7)
    assert "backdrop-filter: blur(20px)" in css_res["css_code"]
    assert "border-radius: 14px" in css_res["css_code"]

    # Contraste texto blanco (#FFFFFF) sobre fondo índigo (#18182E)
    contrast = calculate_wcag_contrast("#FFFFFF", "#18182E")
    assert contrast["contrast_ratio"] >= 4.5
    assert contrast["wcag_aa_normal_text"] is True


@pytest.mark.unit
def test_tool_automation_workflow_and_approval():
    """Valida motor de workflow con compuerta de aprobación para comandos sensibles."""
    engine = WorkflowEngine()
    plan_with_rm = {
        "steps": [
            {"step": 1, "action": "run_linter", "params": {}},
            {"step": 2, "action": "rm /tmp/cache", "params": {}}
        ]
    }
    # Sin aprobación previa, debe pausar
    res = engine.execute_plan(plan_with_rm, human_approved=False)
    assert res["status"] == "AWAITING_APPROVAL"

    # Con aprobación humana, ejecuta
    res_approved = engine.execute_plan(plan_with_rm, human_approved=True)
    assert res_approved["status"] == "COMPLETED"
    assert res_approved["total_steps"] == 2


@pytest.mark.unit
@pytest.mark.asyncio
async def test_agent_evaluator_rubric_unit(monkeypatch):
    """Valida la lógica de la rúbrica de evaluación multidimensional de forma unitaria y determinista."""
    from unittest.mock import AsyncMock
    evaluator = AgentEvaluator()

    # Caso 1: Salida válida con la palabra clave requerida -> Debe pasar
    mock_success = AsyncMock(return_value={
        "success": True,
        "content": "Para prevenir SQL Injection es mandatorio usar Prepared Statements y bind variables.",
        "model": "qwen2.5-coder:1.5b-base",
        "provider": "mock_test"
    })
    monkeypatch.setattr(evaluator.router, "execute_model", mock_success)

    res_pass = await evaluator.evaluate_specialist_task(
        specialty="cybersecurity",
        task="Explicar cómo prevenir SQL Injection",
        expected_keyword="prepared"
    )
    assert res_pass["success"] is True
    assert res_pass["keyword_verified"] is True
    assert res_pass["rubric"]["keyword_match"] is True
    assert res_pass["rubric"]["has_content"] is True

    # Caso 2: Salida sin la palabra clave requerida -> Debe fallar la rúbrica
    mock_no_kw = AsyncMock(return_value={
        "success": True,
        "content": "Debe sanitizar los inputs y usar funciones de escape nativas de la base de datos.",
        "model": "qwen2.5-coder:1.5b-base",
        "provider": "mock_test"
    })
    monkeypatch.setattr(evaluator.router, "execute_model", mock_no_kw)

    res_fail_kw = await evaluator.evaluate_specialist_task(
        specialty="cybersecurity",
        task="Explicar cómo prevenir SQL Injection",
        expected_keyword="prepared"
    )
    assert res_fail_kw["success"] is False
    assert res_fail_kw["keyword_verified"] is False
    assert res_fail_kw["rubric"]["keyword_match"] is False


@pytest.mark.integration
@pytest.mark.asyncio
async def test_agent_evaluator_benchmark():
    """Valida la suite de evaluación integral y guardado de resultados (maneja Ollama online y offline)."""
    from core.model_router import is_ollama_available
    evaluator = AgentEvaluator()
    res = await evaluator.run_full_benchmark()

    assert res["total_specialists_evaluated"] == 6
    assert "model_comparison" in res
    assert Path(evaluator.output_file).exists()

    if is_ollama_available():
        # Si Ollama está activo en el entorno, validar umbral de pase
        assert res["passed"] >= 5
    else:
        # Si Ollama está offline (como en el entorno de la auditoría), se verifica el reporte transparente
        assert res["ollama_online"] is False
        assert "reason" in res["model_comparison"] or "status" in res["model_comparison"]
