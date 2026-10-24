"""
Specialist RAG - Recuperación de Documentación y Conocimiento Especializado
Implementa la Fase 5 del documento rector:
- Bases de conocimiento segmentadas por especialidad (Backend, Ciberseguridad, Kernel, Datos, UI, Automatización).
- Filtro de inyecciones de prompt (Prompt Injection Defense): neutraliza directivas sospechosas en repositorios o archivos.
- Inyección de contexto técnico verificado (manuales, convenciones de Zenthra OS, directrices OWASP/POSIX/WCAG).
"""

import re
from typing import Dict, List, Any, Optional

# Patrones maliciosos comunes de inyección de prompt
PROMPT_INJECTION_PATTERNS = [
    r'(?i)ignore\s+(all\s+)?previous\s+instructions',
    r'(?i)disregard\s+(all\s+)?prior\s+directives',
    r'(?i)you\s+are\s+now\s+in\s+developer\s+mode',
    r'(?i)system\s+prompt\s+override',
    r'(?i)jailbreak',
    r'(?i)bypass\s+all\s+safety\s+filters'
]


def sanitize_rag_content(content: str) -> str:
    """Neutraliza patrones de prompt injection encontrados en archivos externos o repositorios."""
    if not isinstance(content, str):
        return ""
    clean = content
    for pattern in PROMPT_INJECTION_PATTERNS:
        clean = re.sub(pattern, "[CONTENIDO NO CONFIABLE FILTRADO]", clean)
    return clean


class SpecialistRAG:
    """Motor de conocimiento recuperable segmentado por especialidad."""

    def __init__(self):
        self.knowledge_base: Dict[str, List[Dict[str, str]]] = {
            "backend": [
                {
                    "title": "Convenciones de Código y APIs REST",
                    "content": (
                        "Convenciones Backend Zenthra/Jarvis:\n"
                        "- Todo endpoint REST debe devolver respuestas estructuradas en JSON con código HTTP semántico (200, 201, 400, 404, 500).\n"
                        "- Funciones en Python deben incluir type hints (PEP 484) y docstrings descriptivos con parámetros y retorno.\n"
                        "- Manejo explícito de excepciones sin bloques `except:` vacíos. Toda función crítica requiere pruebas unitarias con pytest."
                    )
                },
                {
                    "title": "Arquitectura de Base de Datos y Consultas",
                    "content": (
                        "Reglas de SQL y Persistencia:\n"
                        "- Prohibida la concatenación de variables en queries; siempre usar Prepared Statements o ORM.\n"
                        "- Transacciones atómicas (BEGIN/COMMIT/ROLLBACK) para operaciones financieras o de múltiples tablas."
                    )
                }
            ],
            "cybersecurity": [
                {
                    "title": "Directrices Defensivas OWASP Top 10 y CWE",
                    "content": (
                        "Directrices de Ciberseguridad Defensiva Zenthra OS:\n"
                        "- CWE-89 (SQL Injection): Exigir validación de tipos y sentencias parametrizadas.\n"
                        "- CWE-798 (Credenciales Hardcodeadas): Ningún secreto o token en código; usar variables de entorno.\n"
                        "- Modo Solo Lectura: Nunca generar payloads de ataque, scripts de DDoS ni pruebas de penetración no autorizadas."
                    )
                }
            ],
            "kernel": [
                {
                    "title": "Arquitectura Multiboot 1 y Manejo de #PF en x86",
                    "content": (
                        "Especificación Kernel x86 Zenthra OS:\n"
                        "- Arranque Multiboot: Magic 0x1BADB002, flags (1<<0 | 1<<1 | 1<<2) para VESA LFB 1024x768x32bpp, checksum suma cero.\n"
                        "- Manejador de Page Fault #PF: En fallos durante copy_from_user, no basta con marcar bandera booleana; "
                        "es imperativo redirigir regs->eip a la etiqueta de fixup (&vmm_user_copy_fixup) y retornar -EFAULT para evitar bucle infinito.\n"
                        "- Modo Sandbox: Prohibido modificar memoria de Ring 0 en caliente o escribir en `/dev/sda`."
                    )
                }
            ],
            "data": [
                {
                    "title": "Metodología Estadística y Tratamiento de Outliers",
                    "content": (
                        "Estándares de Datos Zenthra OS:\n"
                        "- Cuando los datos presentan asimetría o valores atípicos (outliers), la mediana y el rango intercuartílico (IQR) son más robustos que la media y la desviación estándar.\n"
                        "- Prohibido imputar datos o inventar métricas sin declarar explícitamente el supuesto estadístico empleado."
                    )
                }
            ],
            "creative": [
                {
                    "title": "Especificación de Diseño Glassmorphism Zenthra OS",
                    "content": (
                        "Guía de Estilos Zenthra OS / Jarvis UI:\n"
                        "- Fondo cósmico: Degradado índigo profundo (#18182E a #060913).\n"
                        "- Ventanas: Paneles translúcidos con `backdrop-filter: blur(16px)` y borde sutil `rgba(255, 255, 255, 0.12)`.\n"
                        "- Controles: Semáforos macOS circulares (Rojo #FF5F56, Amarillo #FFBD2E, Verde #27C93F).\n"
                        "- Accesibilidad: Contraste mínimo 4.5:1 (WCAG 2.1 AA) para texto legible sobre fondo oscuro."
                    )
                }
            ],
            "automation": [
                {
                    "title": "Orquestación Supervisada y Compuerta de Aprobación",
                    "content": (
                        "Principios de Automatización Confiable:\n"
                        "- Todo plan debe ser estructurado en pasos secuenciales con límite de reintentos y timeouts.\n"
                        "- Acciones con impacto irreversible (borrado de datos, apagado del sistema) requieren detención inmediata y solicitud de confirmación humana.\n"
                        "- Toda tarea debe soportar el comando de cancelación abortando limpiamente los procesos en curso."
                    )
                }
            ]
        }

    def get_specialist_knowledge(self, specialty: str, query: str = "") -> str:
        """Recupera documentación y directrices aprobadas para una especialidad."""
        spec_clean = specialty.lower().strip()
        snippets = self.knowledge_base.get(spec_clean, [])
        if not snippets:
            return ""

        output_lines = [f"=== CONOCIMIENTO RAG ESPECIALIZADO ({spec_clean.upper()}) ==="]
        for snip in snippets:
            clean_title = sanitize_rag_content(snip["title"])
            clean_content = sanitize_rag_content(snip["content"])
            output_lines.append(f"\n[DOCUMENTO: {clean_title}]\n{clean_content}")

        return "\n".join(output_lines)

    def add_knowledge_snippet(self, specialty: str, title: str, content: str) -> None:
        """Añade un nuevo fragmento de conocimiento sanitizado a la base."""
        spec_clean = specialty.lower().strip()
        clean_title = sanitize_rag_content(title)
        clean_content = sanitize_rag_content(content)
        self.knowledge_base.setdefault(spec_clean, []).append({
            "title": clean_title,
            "content": clean_content
        })


# Singleton
_rag_instance: Optional[SpecialistRAG] = None


def get_specialist_rag() -> SpecialistRAG:
    global _rag_instance
    if _rag_instance is None:
        _rag_instance = SpecialistRAG()
    return _rag_instance
