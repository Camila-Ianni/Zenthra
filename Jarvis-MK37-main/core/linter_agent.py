"""
Linter Agent - Validación automática de código antes de mostrar al usuario
Features:
  • Syntax validation rápida
  • Linting automático
  • Autocorrección de issues comunes
  • Feedback loop proactivo
  • Usa modelo rápido (Qwen3) para máxima velocidad
"""

import re
import asyncio
import json
import logging
from typing import Optional, Dict, Any, List, Tuple
from dataclasses import dataclass, asdict
from enum import Enum
from pathlib import Path

logger = logging.getLogger("LinterAgent")


class IssueSeverity(Enum):
    """Severidad de problemas detectados."""
    INFO = "info"           # Información útil
    WARNING = "warning"     # Advertencia (pero funciona)
    ERROR = "error"         # Error (código roto)
    CRITICAL = "critical"   # Crítico (seguridad, memory leak)


@dataclass
class LintIssue:
    """Un problema detectado por el linter."""
    severity: IssueSeverity
    line: int
    column: int
    code: str  # ej: "E501" (line too long)
    message: str
    suggestion: Optional[str] = None


@dataclass
class LintResult:
    """Resultado del linting."""
    language: str
    code: str
    issues: List[LintIssue]
    has_errors: bool
    has_warnings: bool
    is_valid: bool
    autocorrected_code: Optional[str] = None
    total_issues: int = 0


class PythonLinter:
    """Linter para Python."""
    
    @staticmethod
    def lint(code: str) -> List[LintIssue]:
        """Linting básico de Python."""
        issues = []
        lines = code.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            # E501: Line too long
            if len(line) > 120:
                issues.append(LintIssue(
                    severity=IssueSeverity.WARNING,
                    line=line_num,
                    column=120,
                    code="E501",
                    message="Line too long (over 120 characters)",
                    suggestion=f"Break into multiple lines or use line continuation"
                ))
            
            # Missing type hints
            if 'def ' in line and '->' not in line and ':' in line:
                issues.append(LintIssue(
                    severity=IssueSeverity.WARNING,
                    line=line_num,
                    column=0,
                    code="W1001",
                    message="Missing return type hint",
                    suggestion="Add -> ReturnType to function definition"
                ))
            
            # Unused variables
            if re.match(r'^[a-z_][a-z0-9_]*\s*=\s*[^=]', line) and line.strip().startswith('_'):
                issues.append(LintIssue(
                    severity=IssueSeverity.INFO,
                    line=line_num,
                    column=0,
                    code="I1001",
                    message="Unused variable detected",
                    suggestion="Prefix with underscore or remove"
                ))
            
            # Bare except
            if 'except:' in line:
                issues.append(LintIssue(
                    severity=IssueSeverity.ERROR,
                    line=line_num,
                    column=line.index('except:'),
                    code="E722",
                    message="Bare except is bad practice",
                    suggestion="Specify exception type: except Exception as e:"
                ))
            
            # Print debugging (probably should use logging)
            if re.search(r'\bprint\s*\(', line) and not line.strip().startswith('#'):
                issues.append(LintIssue(
                    severity=IssueSeverity.INFO,
                    line=line_num,
                    column=line.index('print'),
                    code="I1002",
                    message="Debug print statement detected",
                    suggestion="Use logging.debug() instead of print()"
                ))
        
        return issues


class JavaScriptLinter:
    """Linter para JavaScript/TypeScript."""
    
    @staticmethod
    def lint(code: str) -> List[LintIssue]:
        """Linting básico de JavaScript."""
        issues = []
        lines = code.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            # E501: Line too long
            if len(line) > 100:
                issues.append(LintIssue(
                    severity=IssueSeverity.WARNING,
                    line=line_num,
                    column=100,
                    code="E501",
                    message="Line too long (over 100 characters)",
                    suggestion="Break into multiple lines"
                ))
            
            # var usage (should use const/let)
            if re.match(r'^\s*var\s+', line):
                issues.append(LintIssue(
                    severity=IssueSeverity.WARNING,
                    line=line_num,
                    column=0,
                    code="W1001",
                    message="Using var (should use const/let)",
                    suggestion="Replace 'var' with 'const' (or 'let' if reassigned)"
                ))
            
            # == vs ===
            if '==' in line and '===' not in line and '!=' in line and '!==' not in line:
                if not line.strip().startswith('//'):
                    issues.append(LintIssue(
                        severity=IssueSeverity.WARNING,
                        line=line_num,
                        column=line.index('=='),
                        code="W1002",
                        message="Using == (should use ===)",
                        suggestion="Replace '==' with '==='"
                    ))
            
            # console.log (should be removed or use logger)
            if 'console.log' in line and not line.strip().startswith('//'):
                issues.append(LintIssue(
                    severity=IssueSeverity.INFO,
                    line=line_num,
                    column=line.index('console.log'),
                    code="I1001",
                    message="Debug console.log detected",
                    suggestion="Remove or use logging service"
                ))
            
            # Missing semicolon (JavaScript allows ASI, but good practice)
            if line.rstrip() and not line.rstrip().endswith((';', '{', '}', ')', ',', ':', '//')):
                if line.strip() and not line.strip().startswith('//'):
                    issues.append(LintIssue(
                        severity=IssueSeverity.INFO,
                        line=line_num,
                        column=len(line),
                        code="I1002",
                        message="Missing semicolon (ASI will insert)",
                        suggestion="Add semicolon for clarity"
                    ))
        
        return issues


class JavaLinter:
    """Linter para Java."""
    
    @staticmethod
    def lint(code: str) -> List[LintIssue]:
        """Linting básico de Java."""
        issues = []
        lines = code.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            # E501: Line too long
            if len(line) > 120:
                issues.append(LintIssue(
                    severity=IssueSeverity.WARNING,
                    line=line_num,
                    column=120,
                    code="E501",
                    message="Line too long (over 120 characters)",
                    suggestion="Break into multiple lines"
                ))
            
            # Missing @Override
            if 'public ' in line and '(' in line:
                issues.append(LintIssue(
                    severity=IssueSeverity.INFO,
                    line=line_num,
                    column=0,
                    code="I1001",
                    message="Public method without @Override check",
                    suggestion="Add @Override if this overrides parent method"
                ))
            
            # Raw type usage
            if re.search(r'\b(List|Map|Set)\s*[^<]', line):
                issues.append(LintIssue(
                    severity=IssueSeverity.WARNING,
                    line=line_num,
                    column=0,
                    code="W1001",
                    message="Raw type usage (missing generics)",
                    suggestion="Use List<T>, Map<K,V>, Set<T> with generic types"
                ))
        
        return issues


class LinterAgent:
    """
    Agente de Linting - Valida código antes de mostrarlo al usuario
    Usa Qwen3 (rápido) para análisis
    """
    
    LINTERS = {
        "python": PythonLinter(),
        "py": PythonLinter(),
        "javascript": JavaScriptLinter(),
        "js": JavaScriptLinter(),
        "typescript": JavaScriptLinter(),
        "ts": JavaScriptLinter(),
        "java": JavaLinter(),
    }
    
    def __init__(self):
        self.issue_history: Dict[str, List[LintResult]] = {}
    
    async def lint_code(self, 
                       code: str,
                       language: str,
                       filename: str = "unknown") -> LintResult:
        """
        Lintear código y retornar resultado.
        
        Args:
            code: Código a validar
            language: Lenguaje (python, javascript, java, etc.)
            filename: Nombre del archivo (para context)
        
        Returns:
            LintResult con issues detectados
        """
        logger.info(f"Linting {language} code from {filename}")
        
        lang_lower = language.lower().strip()
        
        # Obtener linter para el lenguaje
        linter = self.LINTERS.get(lang_lower)
        
        issues = []
        if linter:
            issues = linter.lint(code)
        else:
            logger.warning(f"No linter available for {language}, skipping")
        
        # Determinar si hay errores/warnings
        has_errors = any(i.severity in [IssueSeverity.ERROR, IssueSeverity.CRITICAL] for i in issues)
        has_warnings = any(i.severity == IssueSeverity.WARNING for i in issues)
        is_valid = not has_errors
        
        result = LintResult(
            language=lang_lower,
            code=code,
            issues=issues,
            has_errors=has_errors,
            has_warnings=has_warnings,
            is_valid=is_valid,
            total_issues=len(issues)
        )
        
        # Recordar en history
        if filename not in self.issue_history:
            self.issue_history[filename] = []
        self.issue_history[filename].append(result)
        
        logger.info(f"Linting complete: {len(issues)} issues found")
        
        return result
    
    async def autocorrect(self,
                         code: str,
                         language: str,
                         issues: List[LintIssue]) -> Optional[str]:
        """
        Intentar autocorrección de issues comunes.
        
        Args:
            code: Código original
            language: Lenguaje
            issues: Issues a corregir
        
        Returns:
            Código corregido o None si no se puede autocorrect
        """
        if not issues:
            return None
        
        corrected = code
        lang_lower = language.lower().strip()
        
        # Python autocorrections
        if lang_lower in ["python", "py"]:
            lines = corrected.split('\n')
            
            for issue in issues:
                if issue.code == "E722":  # Bare except
                    # Reemplazar "except:" con "except Exception as e:"
                    lines[issue.line - 1] = lines[issue.line - 1].replace(
                        "except:", "except Exception as e:"
                    )
            
            corrected = '\n'.join(lines)
        
        # JavaScript autocorrections
        elif lang_lower in ["javascript", "js", "typescript", "ts"]:
            lines = corrected.split('\n')
            
            for issue in issues:
                if issue.code == "W1001":  # Using var
                    # Reemplazar "var" con "const"
                    lines[issue.line - 1] = re.sub(
                        r'\bvar\s+', 'const ', lines[issue.line - 1]
                    )
                elif issue.code == "W1002":  # Using ==
                    # Reemplazar == con ===
                    lines[issue.line - 1] = lines[issue.line - 1].replace('==', '===')
            
            corrected = '\n'.join(lines)
        
        return corrected if corrected != code else None
    
    def format_report(self, result: LintResult) -> str:
        """Formatear reporte de linting para mostrar al usuario."""
        report = f"""
╔════════════════════════════════════════════════════════════════╗
║ LINTING REPORT: {result.language.upper()}
╚════════════════════════════════════════════════════════════════╝

Status: {'✅ VALID' if result.is_valid else '❌ HAS ISSUES'}
Total Issues: {result.total_issues}
Errors: {sum(1 for i in result.issues if i.severity == IssueSeverity.ERROR)}
Warnings: {sum(1 for i in result.issues if i.severity == IssueSeverity.WARNING)}
Info: {sum(1 for i in result.issues if i.severity == IssueSeverity.INFO)}

"""
        
        if result.issues:
            for issue in result.issues:
                emoji = {
                    IssueSeverity.ERROR: "❌",
                    IssueSeverity.CRITICAL: "🔥",
                    IssueSeverity.WARNING: "⚠️",
                    IssueSeverity.INFO: "ℹ️"
                }.get(issue.severity, "•")
                
                report += f"{emoji} Line {issue.line}: {issue.message}\n"
                report += f"   Code: {issue.code}\n"
                
                if issue.suggestion:
                    report += f"   💡 Suggestion: {issue.suggestion}\n"
                
                report += "\n"
        else:
            report += "✅ No issues found!\n"
        
        report += "╚════════════════════════════════════════════════════════════════╝\n"
        
        return report
    
    def get_issue_stats(self) -> Dict[str, Any]:
        """Obtener estadísticas de issues."""
        total_issues = sum(
            len(result.issues)
            for results in self.issue_history.values()
            for result in results
        )
        
        files_checked = len(self.issue_history)
        
        issue_types: Dict[str, int] = {}
        for results in self.issue_history.values():
            for result in results:
                for issue in result.issues:
                    issue_types[issue.code] = issue_types.get(issue.code, 0) + 1
        
        return {
            "total_issues": total_issues,
            "files_checked": files_checked,
            "issue_breakdown": issue_types,
            "most_common": max(issue_types.items(), key=lambda x: x[1])[0] if issue_types else None
        }


# Singleton instance
_linter_instance: Optional[LinterAgent] = None


def get_linter_agent() -> LinterAgent:
    """Obtener instancia singleton del linter agent."""
    global _linter_instance
    if _linter_instance is None:
        _linter_instance = LinterAgent()
    return _linter_instance
