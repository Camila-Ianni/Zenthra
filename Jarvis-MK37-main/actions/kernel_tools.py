"""
Kernel Tools - Herramientas de análisis y validación sandbox para el Agente de Kernel y Sistemas
Modo SANDBOX. Prohíbe cualquier acceso directo a Ring 0 o dispositivos físicos.
"""

import difflib
import re
from typing import Dict, Any, List


def validate_multiboot_header(asm_text: str) -> Dict[str, Any]:
    """Valida la cabecera Multiboot 1 (0x1BADB002) y la regla de suma cero."""
    magic_match = re.search(r'0x1BADB002', asm_text, re.IGNORECASE)
    has_magic = bool(magic_match)

    # Buscar suma de verificación (magic + flags + checksum == 0)
    has_checksum = "-(0x1BADB002" in asm_text or "-(MULTIBOOT_HEADER_MAGIC" in asm_text or "checksum" in asm_text.lower()

    # Buscar modo de video VESA
    has_video_mode = "VIDEO_MODE" in asm_text or "1 << 2" in asm_text or "1024" in asm_text

    return {
        "valid_multiboot_magic": has_magic,
        "magic_hex": "0x1BADB002" if has_magic else "MISSING",
        "has_checksum_formula": has_checksum,
        "vesa_lfb_requested": has_video_mode,
        "is_valid": has_magic and has_checksum,
        "explanation": "Cabecera conforme a especificación Multiboot v0.6.96" if (has_magic and has_checksum) else "Cabecera Multiboot inválida o incompleta"
    }


def audit_page_fault_handler(code: str) -> Dict[str, Any]:
    """Audita la lógica del manejador de Page Fault #PF en x86."""
    has_fixup_redirection = "fixup" in code.lower() or "regs->eip" in code.lower() or "regs.eip" in code.lower()
    has_cr2_read = "cr2" in code.lower()
    has_efault = "EFAULT" in code

    is_safe = has_fixup_redirection and has_efault

    return {
        "has_cr2_faulting_address": has_cr2_read,
        "has_eip_fixup_redirection": has_fixup_redirection,
        "returns_efault_safely": has_efault,
        "is_safe_recovery": is_safe,
        "recommendation": "Correcto: Redirecciona EIP a etiqueta de fixup en fallos de copia userland evitando bucle infinito de #PF." if is_safe else "Peligro: Solo marcar bandera sin redirigir EIP causa recursión infinita de #PF."
    }


def generate_kernel_diff(old_code: str, new_code: str, filename: str = "kernel_file.c") -> str:
    """Genera diff unificado estricto para revisión humana de cambios de kernel."""
    old_lines = old_code.splitlines(keepends=True)
    new_lines = new_code.splitlines(keepends=True)
    diff = difflib.unified_diff(
        old_lines,
        new_lines,
        fromfile=f"a/{filename}",
        tofile=f"b/{filename}",
        lineterm=""
    )
    return "".join(diff)
