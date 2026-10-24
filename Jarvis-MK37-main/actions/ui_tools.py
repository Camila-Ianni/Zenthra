"""
UI Tools - Herramientas de diseño visual Glassmorphism y accesibilidad para el Agente Creativo/UI
Conserva y valida la estética acordada para Zenthra OS y Jarvis.
"""

import math
from typing import Dict, Any


def get_zenthra_palette() -> Dict[str, str]:
    """Retorna la paleta cromática oficial de Zenthra OS."""
    return {
        "cosmic_top": "#18182E",
        "cosmic_bottom": "#060913",
        "obsidian_card": "#0F121C",
        "obsidian_terminal": "#0B0D13",
        "glass_border": "rgba(255, 255, 255, 0.12)",
        "glass_border_focused": "rgba(99, 102, 241, 0.65)",
        "traffic_red": "#FF5F56",
        "traffic_yellow": "#FFBD2E",
        "traffic_green": "#27C93F",
        "neon_cyan": "#00E5FF",
        "indigo_primary": "#6366F1",
        "text_primary": "#E2E8F0",
        "text_muted": "#8A94A6"
    }


def generate_glassmorphism_css(blur_px: int = 16, opacity: float = 0.65, border_radius_px: int = 14) -> Dict[str, str]:
    """Genera las declaraciones CSS y especificaciones técnicas para un panel Glassmorphism."""
    css = f"""
.glass-panel {{
    background: rgba(15, 18, 28, {opacity});
    backdrop-filter: blur({blur_px}px) saturate(180%);
    -webkit-backdrop-filter: blur({blur_px}px) saturate(180%);
    border-radius: {border_radius_px}px;
    border: 1px solid rgba(255, 255, 255, 0.12);
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.45), 0 1px 3px rgba(0, 0, 0, 0.3);
}}
.glass-panel:focus-within {{
    border-color: rgba(99, 102, 241, 0.65);
    box-shadow: 0 12px 35px rgba(0, 0, 0, 0.55), 0 0 15px rgba(99, 102, 241, 0.35);
}}
""".strip()
    return {
        "css_code": css,
        "blur_px": blur_px,
        "opacity": opacity,
        "border_radius": border_radius_px
    }


def _hex_to_rgb(hex_str: str) -> tuple[float, float, float]:
    h = hex_str.lstrip("#")
    if len(h) == 3:
        h = "".join([c*2 for c in h])
    return (int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0)


def _relative_luminance(rgb: tuple[float, float, float]) -> float:
    def adjust(c: float) -> float:
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = [adjust(c) for c in rgb]
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def calculate_wcag_contrast(fg_hex: str, bg_hex: str) -> Dict[str, Any]:
    """Calcula el contraste WCAG 2.1 entre dos colores hexadecimales."""
    try:
        lum1 = _relative_luminance(_hex_to_rgb(fg_hex))
        lum2 = _relative_luminance(_hex_to_rgb(bg_hex))
        lighter = max(lum1, lum2)
        darker = min(lum1, lum2)
        ratio = (lighter + 0.05) / (darker + 0.05)
        ratio_rounded = round(ratio, 2)
        
        pass_aa = ratio >= 4.5
        pass_aaa = ratio >= 7.0
        pass_large_text = ratio >= 3.0

        return {
            "fg": fg_hex,
            "bg": bg_hex,
            "contrast_ratio": ratio_rounded,
            "wcag_aa_normal_text": pass_aa,
            "wcag_aaa_normal_text": pass_aaa,
            "wcag_aa_large_text": pass_large_text,
            "verdict": "APROBADO (AA y AAA)" if pass_aaa else ("APROBADO (AA)" if pass_aa else "FALLA DE CONTRASTE")
        }
    except Exception as e:
        return {"error": f"Error calculando contraste: {str(e)}", "contrast_ratio": 0.0}
