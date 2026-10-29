#ifndef DESIGN_TOKENS_H
#define DESIGN_TOKENS_H

#include "ktypes.h"

/* ==========================================================================
   ZENTHRA OS — NATIVE DESIGN TOKENS (Glassmorphism & Cosmic Palette)
   Especificación Unificada de Tokens Semánticos para Kernel C y VESA LFB
   ========================================================================== */

/* --------------------------------------------------------------------------
   1. TOKENS GLOBALES DE COLOR (Paleta Primitiva ARGB 32-bit)
   -------------------------------------------------------------------------- */
#define ZT_COLOR_BLACK              0xFF000000
#define ZT_COLOR_WHITE              0xFFFFFFFF

/* Escala Slate / Cosmic Core */
#define ZT_COLOR_COSMIC_950         0xFF020617  /* Fondo Abisal */
#define ZT_COLOR_COSMIC_900         0xFF0F172A  /* Superficie Base */
#define ZT_COLOR_COSMIC_850         0xFF1E1B4B  /* Índigo Profundo */
#define ZT_COLOR_SLATE_800          0xFF1E293B  /* Bordes de Ventana */
#define ZT_COLOR_SLATE_700          0xFF334155  /* Divisores */
#define ZT_COLOR_SLATE_600          0xFF475569  /* Texto Silenciado */
#define ZT_COLOR_SLATE_400          0xFF94A3B8  /* Texto Secundario */
#define ZT_COLOR_SLATE_200          0xFFE2E8F0  /* Texto Primario */
#define ZT_COLOR_SLATE_50           0xFFF8FAFC  /* Texto Alto Contraste */

/* Acentos Cromáticos */
#define ZT_COLOR_INDIGO_500         0xFF6366F1  /* Acento Primario Jarvis */
#define ZT_COLOR_INDIGO_400         0xFF818CF8  /* Resaltado de Enfoque */
#define ZT_COLOR_CYAN_400           0xFF38BDF8  /* Terminal & Syscalls */
#define ZT_COLOR_EMERALD_500        0xFF10B981  /* Éxito & Estado Activo */
#define ZT_COLOR_EMERALD_400        0xFF34D399  /* Prompt Terminal */
#define ZT_COLOR_AMBER_500          0xFFF59E0B  /* Alerta & Aprobación HITL */
#define ZT_COLOR_ROSE_500           0xFFF43F5E  /* Error / Rechazo */

/* Semáforo macOS (Traffic Lights) */
#define ZT_COLOR_TRAFFIC_CLOSE      0xFFFF5F56  /* Botón Cerrar */
#define ZT_COLOR_TRAFFIC_MINIMIZE   0xFFFFBD2E  /* Botón Minimizar */
#define ZT_COLOR_TRAFFIC_MAXIMIZE   0xFF27C93F  /* Botón Maximizar */

/* --------------------------------------------------------------------------
   2. FACTORES ALFA (Translucidez Glassmorphism en VESA LFB)
   -------------------------------------------------------------------------- */
#define ZT_ALPHA_OPAQUE             0xFF        /* Modo Eco / Sin Alpha */
#define ZT_ALPHA_ACRYLIC_WINDOW     0xD8        /* ~85% Opacidad Ventanas */
#define ZT_ALPHA_CARD_SUBTLE        0xB0        /* ~69% Opacidad Tarjetas */
#define ZT_ALPHA_DOCK_GLASS         0x99        /* ~60% Opacidad Dock */
#define ZT_ALPHA_TOPBAR_GLASS       0xC0        /* ~75% Opacidad Topbar */
#define ZT_ALPHA_BORDER_FOCUSED     0xA0        /* Resplandor de Borde */
#define ZT_ALPHA_BORDER_INACTIVE    0x40        /* Borde Suave */

/* --------------------------------------------------------------------------
   3. TOKENS DE DIMENSIÓN Y ESPACIADO (Pixels)
   -------------------------------------------------------------------------- */
#define ZT_RADIUS_WINDOW            10
#define ZT_RADIUS_CARD              6
#define ZT_RADIUS_DOCK_PILL         18
#define ZT_RADIUS_TRAFFIC_LIGHT     5

#define ZT_HEIGHT_TOPBAR            28
#define ZT_HEIGHT_TITLEBAR          24
#define ZT_HEIGHT_DOCK              44

#endif /* DESIGN_TOKENS_H */
