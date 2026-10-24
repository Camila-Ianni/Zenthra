"""
Test Suite - 6 Agentes Especialistas de Jarvis-MK37 (Auditoría v21)
Verifica:
1. Registro y descubrimiento de los 6 agentes en AgentRegistry.
2. Contrato unificado y validación de permisos mínimos por especialista.
3. Puertas de seguridad (Safety Policies) para Ciberseguridad, Kernel y Automatización.
4. Transparencia en resolución de modelos: candidate_model vs effective_model y detección de fallback.
5. Manejo explícito de estado 'no disponible' cuando se desactiva el fallback genérico.
6. Ejecución unitaria determinista con mock (sin depender de red ni Ollama).
7. Pruebas de integración con Ollama marcadas explícitamente con skipif.
"""

import os
import pytest
import asyncio
from unittest.mock import AsyncMock

from core.agent_registry import get_agent_registry, AgentType
from core.specialist_agents import (
    get_specialist_agent,
    list_specialist_agents,
    BackendSpecialistAgent,
    CybersecuritySpecialistAgent,
    KernelSystemsSpecialistAgent,
    DataSpecialistAgent,
    CreativeUISpecialistAgent,
    AutomationSpecialistAgent,
)
from core.model_router import get_model_router, is_ollama_available


@pytest.mark.unit
def test_registry_contains_all_six_specialists():
    """Prueba 1: AgentRegistry inicializa y contiene los 6 especialistas con prioridad 9."""
    reg = get_agent_registry()
    specialist_types = [
        AgentType.BACKEND,
        AgentType.CYBERSECURITY,
        AgentType.KERNEL_SYSTEMS,
        AgentType.DATA,
        AgentType.CREATIVE_UI,
        AgentType.AUTOMATION,
    ]
    for stype in specialist_types:
        agents = reg.get_agents_by_type(stype)
        assert len(agents) >= 1, f"Falta agente de tipo {stype}"
        agent = agents[0]
        assert agent.specialist_agent is not None, f"Agente {agent.id} sin specialist_agent"
        assert agent.priority == 9, f"Prioridad incorrecta para {agent.id}"


@pytest.mark.unit
def test_specialist_factory_and_listing():
    """Prueba 2: Factory list_specialist_agents devuelve exactamente los 6 agentes."""
    specialists = list_specialist_agents()
    assert len(specialists) == 6
    specs = {s.specialty for s in specialists}
    assert specs == {"backend", "cybersecurity", "kernel", "data", "creative", "automation"}


@pytest.mark.unit
def test_least_privilege_tool_permissions():
    """Prueba 3: Cada especialista solo tiene autorizadas sus herramientas asignadas."""
    backend = get_specialist_agent("backend")
    cyber = get_specialist_agent("cybersecurity")
    kernel = get_specialist_agent("kernel")
    data = get_specialist_agent("data")

    # Backend tiene code_helper y linter, pero no security_scanner ni asm_c_linter
    assert backend.validate_tool_permission("code_helper") is True
    assert backend.validate_tool_permission("linter_agent") is True
    assert backend.validate_tool_permission("security_scanner") is False

    # Ciberseguridad tiene security_scanner y secret_leak_checker, pero no file_controller de escritura
    assert cyber.validate_tool_permission("security_scanner") is True
    assert cyber.validate_tool_permission("secret_leak_checker") is True
    assert cyber.validate_tool_permission("file_controller") is False

    # Kernel tiene diff_generator y c_asm_linter, pero no csv_analyzer
    assert kernel.validate_tool_permission("diff_generator") is True
    assert kernel.validate_tool_permission("c_asm_linter") is True
    assert kernel.validate_tool_permission("csv_analyzer") is False

    # Datos tiene csv_analyzer y stats_calculator, pero no security_scanner
    assert data.validate_tool_permission("csv_analyzer") is True
    assert data.validate_tool_permission("security_scanner") is False


@pytest.mark.unit
def test_cybersecurity_defensive_safety_policy():
    """Prueba 4: Ciberseguridad bloquea payloads ofensivos o DDoS."""
    cyber = get_specialist_agent("cybersecurity")
    is_safe, req_app, reason = cyber.verify_safety_policy("Auditar dependencias de requirements.txt para vulnerabilidades conocidas")
    assert is_safe is True
    assert req_app is False

    # Intento de acción ofensiva
    is_safe, req_app, reason = cyber.verify_safety_policy("Generar un exploit para DDoS a un servidor externo")
    assert is_safe is False
    assert "prohibida por la política defensiva" in reason


@pytest.mark.unit
def test_kernel_systems_sandbox_policy():
    """Prueba 5: Kernel y Sistemas bloquea escritura destructiva en discos o Ring 0 caliente."""
    kernel = get_specialist_agent("kernel")
    is_safe, req_app, reason = kernel.verify_safety_policy("Analizar estructura del Framebuffer VESA LFB en boot/boot.s")
    assert is_safe is True

    # Intento destructivo
    is_safe, req_app, reason = kernel.verify_safety_policy("Sobreescribir MBR y write /dev/sda directamente")
    assert is_safe is False
    assert req_app is True
    assert "bloqueada" in reason


@pytest.mark.unit
def test_automation_approval_gate():
    """Prueba 6: Automatización exige confirmación en comandos irreversibles."""
    auto = get_specialist_agent("automation")
    is_safe, req_app, reason = auto.verify_safety_policy("Ejecutar tests unitarios y compilar binario")
    assert is_safe is True
    assert req_app is False

    # Comando sensible
    is_safe, req_app, reason = auto.verify_safety_policy("Delete all staging instances and shutdown -h now")
    assert is_safe is True
    assert req_app is True
    assert "requiere aprobación humana" in reason


@pytest.mark.unit
def test_model_override_via_environment(monkeypatch):
    """Prueba 7: Los modelos de cada agente pueden sobreescribirse por configuración."""
    backend = get_specialist_agent("backend")
    monkeypatch.setenv("AGENT_BACKEND_MODEL", "custom-qwen-finetuned")
    assert backend.get_configured_model() == "custom-qwen-finetuned"

    data = get_specialist_agent("data")
    monkeypatch.setenv("AGENT_DATA_MODEL", "custom-gemma-analytics")
    assert data.get_configured_model() == "custom-gemma-analytics"


@pytest.mark.unit
def test_specialist_fallback_transparency(monkeypatch):
    """Prueba 8 (Auditoría v21): Valida que cuando faltan claves, el fallback sea transparente y explícito."""
    cyber = get_specialist_agent("cybersecurity")
    monkeypatch.delenv("OPENROUTER_API_KEY", raising=False)
    monkeypatch.delenv("AGENT_CYBER_MODEL", raising=False)
    monkeypatch.setenv("AGENT_ALLOW_GENERIC_FALLBACK", "true")

    model_eff, provider, is_fb, fb_reason, is_avail = cyber.resolve_model_and_availability()
    assert is_fb is True
    assert "OPENROUTER_API_KEY ausente" in fb_reason
    assert model_eff == "qwen2.5-coder:1.5b-base"
    assert is_avail is True


@pytest.mark.unit
def test_specialist_unavailable_when_fallback_disabled(monkeypatch):
    """Prueba 9 (Auditoría v21): Con fallback genérico desactivado, reporta no disponible sin simulación."""
    cyber = get_specialist_agent("cybersecurity")
    monkeypatch.delenv("OPENROUTER_API_KEY", raising=False)
    monkeypatch.delenv("AGENT_CYBER_MODEL", raising=False)
    monkeypatch.setenv("AGENT_ALLOW_GENERIC_FALLBACK", "false")

    model_eff, provider, is_fb, fb_reason, is_avail = cyber.resolve_model_and_availability()
    assert is_avail is False
    assert is_fb is False
    assert "no disponible" in fb_reason.lower()


@pytest.mark.unit
@pytest.mark.asyncio
async def test_specialist_mock_task_execution(monkeypatch):
    """Prueba 10 (Auditoría v21): Ejecución unitaria determinista con mock (sin depender de red)."""
    backend = get_specialist_agent("backend")
    
    mock_response = {
        "success": True,
        "content": "def suma(a: int, b: int) -> int:\n    return a + b",
        "model": "mock-qwen-coder",
        "provider": "mock_unit_provider"
    }
    
    # Inyectar mock en el router de forma segura para evitar contaminar otros tests
    monkeypatch.setattr(backend.router, "execute_model", AsyncMock(return_value=mock_response))
    
    res = await backend.execute_task("Crear función suma en Python")
    assert res.success is True
    assert "def suma" in res.output
    assert res.provider_used == "mock_unit_provider"
    assert len(res.evidence) > 0


@pytest.mark.integration
@pytest.mark.asyncio
async def test_backend_agent_task_execution():
    """Prueba 11 (Integración): Agente Backend resuelve una tarea real con Ollama local."""
    if not is_ollama_available():
        pytest.skip("Ollama local no disponible en http://localhost:11434")

    backend = get_specialist_agent("backend")
    result = await backend.execute_task(
        task_description="Escribir una función en Python para validar si un string es un JSON válido.",
        context="Entorno de producción Jarvis-MK37"
    )
    assert result.success is True, f"Error en ejecución: {result.error}"
    assert result.agent_id == "agent_backend_specialist"
    assert len(result.output) > 20
    assert "json" in result.output.lower() or "def" in result.output.lower()
    assert result.latency_ms > 0
    assert len(result.evidence) > 0


@pytest.mark.integration
@pytest.mark.asyncio
async def test_data_agent_task_execution():
    """Prueba 12 (Integración): Agente de Datos resuelve una tarea con modelo local de Ollama (Gemma 4 / Qwen)."""
    if not is_ollama_available():
        pytest.skip("Ollama local no disponible en http://localhost:11434")

    data_agent = get_specialist_agent("data")
    result = await data_agent.execute_task(
        task_description="Explicar en 2 líneas qué métrica representa la mediana frente al promedio en datos con outliers."
    )
    assert result.success is True, f"Error en ejecución: {result.error}"
    assert result.agent_id == "agent_data_specialist"
    assert len(result.output) > 10
    assert result.latency_ms > 0
