"""
Security Scanner - Herramienta de auditoría defensiva para el Agente de Ciberseguridad
Modo SOLO LECTURA. Detecta:
- Fugas de secretos / credenciales hardcodeadas
- Inyecciones SQL (concatenación en queries)
- Comandos shell inseguros (shell=True, os.system con variables de usuario)
- Deserialización insegura (pickle) y uso de eval/exec
"""

import re
from typing import Dict, List, Any


def scan_code_security(code: str) -> Dict[str, Any]:
    """Escanea código fuente en busca de patrones de vulnerabilidades comunes."""
    findings = []
    lines = code.split("\n")

    # 1. Detección de claves / secretos hardcodeados
    secret_patterns = [
        (r'(?i)([a-z0-9_]*key|[a-z0-9_]*secret|[a-z0-9_]*password|[a-z0-9_]*token)\s*=\s*["\'][A-Za-z0-9_\-]{16,}["\']', "CRÍTICA", "Credencial o secreto hardcodeado en código fuente", "Usar variables de entorno (os.getenv) o almacén seguro de secretos."),
        (r'(?i)AIzaSy[A-Za-z0-9_\-]{20,40}', "CRÍTICA", "Google API Key hardcodeada", "Revocar la clave y moverla a la variable GOOGLE_API_KEY."),
        (r'(?i)ghp_[A-Za-z0-9]{36}', "CRÍTICA", "GitHub Personal Access Token expuesto", "Revocar el token y configurar GitHub Secrets."),
    ]

    # 2. Inyección SQL
    sql_patterns = [
        (r'(?i)(SELECT|INSERT|UPDATE|DELETE).*\+.*|\+.*(SELECT|INSERT|UPDATE|DELETE)', "ALTA", "Posible SQL Injection por concatenación de cadenas", "Utilizar consultas parametrizadas o Prepared Statements."),
        (r'(?i)f["\'](SELECT|INSERT|UPDATE|DELETE).*\{.*\}', "ALTA", "Posible SQL Injection mediante f-strings", "Usar parámetros bind (?, :param) del motor de base de datos."),
    ]

    # 3. Comandos de sistema inseguros
    shell_patterns = [
        (r'(?i)subprocess\.(Popen|run|call)\(.*shell\s*=\s*True', "ALTA", "Ejecución de shell arbitraria (shell=True)", "Pasar los argumentos como lista y mantener shell=False."),
        (r'(?i)os\.system\(', "MEDIA", "Uso de os.system() para ejecutar comandos", "Migrar a subprocess.run con argumentos validados y shell=False."),
        (r'(?i)\beval\(|\bexec\(', "ALTA", "Ejecución de código dinámico mediante eval/exec", "Reemplazar por parsers seguros (ej: ast.literal_eval para JSON/literales)."),
        (r'(?i)pickle\.loads?\(', "ALTA", "Deserialización insegura de objetos con pickle", "Usar formatos de serialización sin ejecución arbitraria como JSON o Protobuf."),
    ]

    all_rules = secret_patterns + sql_patterns + shell_patterns

    for i, line in enumerate(lines, 1):
        for pattern, severity, desc, mitigation in all_rules:
            if re.search(pattern, line):
                findings.append({
                    "line_number": i,
                    "evidence": line.strip(),
                    "severity": severity,
                    "finding": desc,
                    "mitigation": mitigation,
                    "impact": "Exposición de credenciales, compromiso de datos o ejecución arbitraria de código."
                })

    return {
        "total_findings": len(findings),
        "critical_count": sum(1 for f in findings if f["severity"] == "CRÍTICA"),
        "high_count": sum(1 for f in findings if f["severity"] == "ALTA"),
        "medium_count": sum(1 for f in findings if f["severity"] == "MEDIA"),
        "findings": findings,
        "is_clean": len(findings) == 0
    }


def audit_dependency_file(content: str) -> Dict[str, Any]:
    """Audita dependencias en busca de paquetes riesgosos o fijaciones inseguras."""
    warnings = []
    lines = content.split("\n")
    for i, line in enumerate(lines, 1):
        line_clean = line.strip()
        if not line_clean or line_clean.startswith("#"):
            continue
        if "==" not in line_clean and ">=" not in line_clean:
            warnings.append({
                "line": i,
                "package": line_clean,
                "warning": "Dependencia sin fijar versión estricta (unpinned dependency)",
                "mitigation": "Fijar versión exacta con == para evitar ataques de supply chain."
            })
    return {
        "unpinned_dependencies": len(warnings),
        "warnings": warnings,
        "is_safe": len(warnings) == 0
    }
