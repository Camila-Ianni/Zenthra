# 🤖 JARVIS MARK XXXVII (MK37) — Multi-Agent & 6 Specialists Architecture
### Modular Personal AI Assistant & Specialist Agents Engine (Zenthra OS Integration)

Asistente modular capaz de seleccionar modelos de IA dinámicamente y delegar tareas a **seis agentes especializados, verificables y seguros**, con soporte para inferencia local con **Ollama** y proveedores remotos configurables.

---

## 🏛️ Matriz de Capacidades y 6 Agentes Especialistas

| Agente | Especialidad | Modelo Candidato | Modelo Local Resuelto (Ollama) | Modalidad | Licencia / Fuente Oficial | Herramientas Asignadas (Principio Privilegios Mínimos) | Política de Seguridad |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **1. Backend** | Python, JS, SQL, APIs REST, tests y revisión de repos | `Qwen3-Coder` | `qwen2.5-coder:1.5b-base` / `qwen3.5:latest` | Local / API | [Apache 2.0 (Qwen)](https://qwenlm.github.io/blog/qwen3-coder/) | `code_helper`, `linter_agent`, `run_tests`, `file_controller`, `git_status` | No declara éxito sin evidencia ni tests ejecutados. |
| **2. Ciberseguridad** | Revisión defensiva de código, CVEs, dependencias y mitigaciones | `DeepSeek-R1` | Local / OpenRouter (`deepseek-r1:free`) | Híbrida | [MIT (DeepSeek)](https://github.com/deepseek-ai/DeepSeek-R1) | `security_scanner`, `secret_leak_checker`, `dependency_audit`, `read_file` | **Solo Lectura estricto por defecto**. Bloquea acciones ofensivas y DDoS. |
| **3. Kernel y Sistemas** | C/ASM x86, arranque Multiboot, GDT/IDT, PMM/VMM, syscalls y scheduler | `Kimi K2` | Local / OpenRouter | Híbrida | [Moonshot AI](https://www.kimi.ai/blog/kimi-k2) | `c_asm_linter`, `diff_generator`, `sandbox_compiler`, `verify_qemu_test` | **Modo Sandbox**. Prohíbe modificar Ring 0 en caliente, MBR o `/dev/sda`. Requiere diff. |
| **4. Datos** | SQL, análisis descriptivo CSV, transformaciones y métricas | `Gemma 4` | `gemma4:latest` (8.0B) | Local | [Gemma Terms of Use](https://ai.google.dev/gemma) | `csv_analyzer`, `sql_validator`, `stats_calculator`, `read_data_file` | No inventa métricas ni filas; muestra supuestos y límites de los datos. |
| **5. Creativo / UI** | Diseño visual Glassmorphism, temas cósmicos, semáforos macOS y accesibilidad | `Mistral Small 4` | `mistral` / Ollama Local | Local / API | [Apache 2.0 (Mistral)](https://docs.mistral.ai/models) | `ui_theme_generator`, `css_component_builder`, `accessibility_checker` | Respeta la estética acordada Zenthra OS (Glassmorphism ARGB, semáforos, contraste AA). |
| **6. Automatización** | Descomposición de tareas, workflows y coordinación de planes supervisados | `GLM-4.5-Air` | `glm-4.5-air` / local fallback | Híbrida | [Apache 2.0 (THUDM)](https://github.com/THUDM/GLM-4.5) | `task_decomposer`, `workflow_runner`, `plan_validator`, `cancel_task` | **Gatekeeper humano**. Exige aprobación previa antes de acciones destructivas. |

---

## 🔒 Política de Seguridad, Memoria y Credenciales

- **Sin secretos en el repositorio**: Los archivos `config/model_providers.json` y `config/api_keys.json` no contienen claves en texto plano.
- **Configuración mediante `.env`**: Las credenciales se cargan exclusivamente desde variables de entorno (`.env.example`).
- **Modo Offline / Local First**: Si no hay claves remotas configuradas, Jarvis y los 6 especialistas operan de forma autónoma utilizando los modelos locales de Ollama (`gemma4`, `qwen2.5-coder`, `qwen3.5`).
- **Memoria Selectiva Aislada (Fase 5)**: Cada agente dispone de su propio archivo en `memory/agents/<agent_id>_memory.json`. Se censuran automáticamente secretos (`[REDACTED_...]`) antes de guardar en disco y se admite retención con TTL y borrado selectivo.
- **RAG Defensivo**: Documentación técnica segmentada por especialidad con neutralización activa contra inyecciones de prompt.

---

## 🧠 Evaluación de Viabilidad de Fine-Tuning (Fase 6)

El análisis formal de viabilidad (`core/fine_tuning_evaluator.py`, exportado en `evaluation_fine_tuning.json`) concluye que el **Fine-Tuning supervisado no es necesario ni recomendable en la etapa actual**:
1. **Rendimiento Base Excelente:** La combinación de **Instrucciones de Sistema + RAG Segmentado + Herramientas Deterministas** supera el 94% de precisión en todos los benchmarks.
2. **Riesgos de Degeneración:** En Ciberseguridad y Kernel, reentrenar pesos estocásticos induce riesgo de olvido catastrófico (*catastrophic forgetting*), eliminando salvaguardas nativas o induciendo fallos sutiles en código bare-metal x86.
3. **Costo Cero y Eficiencia:** La arquitectura actual funciona 100% en local en CPU y GPU Apple Silicon sin incurrir en costos de cómputo en la nube ($0 USD) ni mantenimiento de datasets masivos.

---

## ⚙️ Instalación y Configuración

1. **Instalar dependencias**:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   ```

2. **Configurar variables de entorno**:
   ```bash
   cp .env.example .env
   # Editar .env para agregar claves opcionales (GOOGLE_API_KEY, OPENROUTER_API_KEY, etc.)
   ```

3. **Verificar modelos locales con Ollama**:
   ```bash
   # Asegurarse de que Ollama esté corriendo:
   ollama list
   ```

---

## 🧪 Ejecución de Pruebas Automatizadas

```bash
# Ejecutar todas las pruebas unitarias y de integración de Jarvis MK37:
PYTHONPATH=. pytest tests/ -v

# Pruebas específicas por fase:
PYTHONPATH=. pytest tests/test_specialist_agents.py -v   # Fases 2 y 4 (Agentes & Privilegios)
PYTHONPATH=. pytest tests/test_model_comparisons.py -v   # Fase 3 (Evaluación & Benchmarks)
PYTHONPATH=. pytest tests/test_fase5_memory_rag.py -v    # Fase 5 (Memoria Selectiva & RAG)
PYTHONPATH=. pytest tests/test_fase6_fine_tuning.py -v   # Fase 6 (Viabilidad de Fine-Tuning)
```

---

## 🚀 Uso en Código

```python
import asyncio
from core.agent_registry import get_agent_registry
from core.orchestrator import Orchestrator

async def demo():
    # 1. Acceder al registro de especialistas
    registry = get_agent_registry()
    backend_agent = registry.get_specialist("backend")
    
    # 2. Ejecutar tarea con RAG y memoria selectiva aislada
    resultado = await backend_agent.execute_task(
        task_description="Crear una función para validar tokens JWT en FastAPI"
    )
    print("Éxito:", resultado.success)
    print("Salida:", resultado.output)
    print("Herramientas usadas:", resultado.tools_used)

if __name__ == "__main__":
    asyncio.run(demo())
```
