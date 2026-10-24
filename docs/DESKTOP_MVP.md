# Zenthra OS — Especificación Técnica del MVP Gráfico (Escritorio PC)

> **Documento de Diseño Arquitectural — Fase 5**  
> Objetivo: Transformar la consola de texto VGA de Zenthra OS en un entorno de escritorio gráfico propio para computadoras personales, ejecutado directamente sobre el hardware bare-metal (sin navegador web ni capas artificiales).

---

## 1. Principios de Diseño
1. **Nativo Bare-Metal**: La interfaz gráfica es un subsistema en C y ensamblador x86 que dibuja directamente sobre el búfer lineal de vídeo (LFB).
2. **Consola VGA como Modo de Recuperación**: Si la inicialización del modo gráfico falla o el usuario presiona una tecla de escape en el arranque, el sistema inicia en la shell de texto VGA actual para diagnóstico y reparación.
3. **Composición con Doble Búfer (Double Buffering)**: Todo el redibujado se realiza en un búfer en RAM (`backbuffer`) y se copia al framebuffer de la GPU (`frontbuffer`) en un único volcado para evitar parpadeos (*flicker*).

---

## 2. Adquisición del Framebuffer de Vídeo

### 2.1 Configuración Multiboot
En `boot/boot.s`, se extienden los flags de Multiboot 1 para solicitar un modo gráfico lineal a GRUB:
- **Flag bit 2** (`MULTIBOOT_VIDEO_MODE = 1 << 2`).
- Campos del encabezado:
  - `mode_type = 0` (Modo gráfico lineal).
  - `width = 1024` píxeles.
  - `height = 768` píxeles.
  - `depth = 32` bits por píxel (ARGB / RGBA).

### 2.2 Estructura del Framebuffer en Kernel
```c
struct framebuffer_info {
    uint32_t* address;     // Dirección física/virtual del Linear Framebuffer
    uint32_t width;       // 1024
    uint32_t height;      // 768
    uint32_t pitch;       // Bytes por línea de escaneo (1024 * 4 = 4096)
    uint8_t  bpp;         // 32 bits
    uint32_t* backbuffer; // Búfer de memoria en RAM para doble búfer
};
```

---

## 3. Capa de Primitivas Gráficas 2D

El subsistema `kernel/gui/gfx.c` proveerá las siguientes funciones fundamentales:

```c
// Dibuja un píxel individual con color ARGB (0xAARRGGBB)
void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color);

// Rellena un rectángulo (ventanas, botones, fondo de escritorio)
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

// Dibuja el borde de un rectángulo
void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

// Algoritmo de Bresenham para trazado de líneas
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);

// Renderizado de texto con fuente de mapa de bits (8x16)
void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg_color, uint32_t bg_color);
void gfx_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg_color, uint32_t bg_color);

// Vuelca el backbuffer al framebuffer de vídeo de la pantalla
void gfx_flip(void);
```

---

## 4. Driver de Ratón PS/2 (Mouse)

### 4.1 Inicialización y Puerto I/O
- Controlador 8042 (Puertos `0x60` y `0x64`).
- IRQ 12 (Vector 44 en la IDT tras el remapeo PIC).
- Paquetes de datos estándar de 3 bytes:
  1. **Byte 1**: Estado de botones (Izquierdo, Derecho, Central) y signos de desplazamiento.
  2. **Byte 2**: Desplazamiento relativo en el eje X ($\Delta X$).
  3. **Byte 3**: Desplazamiento relativo en el eje Y ($\Delta Y$).

### 4.2 Estado y Cursor de Ratón
```c
struct mouse_state {
    int32_t x;
    int32_t y;
    bool left_button;
    bool right_button;
    bool middle_button;
};
```
- El cursor se dibuja en el búfer como un puntero de flecha de 12x18 píxeles con sombra y contorno de alto contraste.

---

## 5. Gestor de Ventanas (Zenthra Window Manager)

### 5.1 Estructura de Ventana
```c
#define MAX_WINDOWS 8

struct window {
    uint32_t id;
    char title[32];
    int32_t x, y;
    uint32_t width, height;
    uint32_t border_color;
    uint32_t titlebar_color;
    bool visible;
    bool is_focused;
    bool is_dragging;
    void (*on_paint)(struct window* win);
    void (*on_click)(struct window* win, int rel_x, int rel_y);
};
```

### 5.2 Elementos Visuales del Escritorio Nativo (Versión 20.0)
1. **Fondo de Pantalla con Temas Claro / Oscuro**: Gradiente corporativo (`WM_THEME_DARK` Slate/Navy o `WM_THEME_LIGHT` Ice/Azure) conmutables en vivo con `wm_toggle_theme()`.
2. **Glassmorphism Real en Framebuffer de 32 bpp y Modo Eco**:
   - Primitivas `gfx_blend_argb` y `gfx_fill_rect_alpha` en [`kernel/drivers/gfx.c`](file:///Users/cami/Desktop/NOVA/kernel/drivers/gfx.c) que calculan mezcla alfa por canal `(R, G, B)` sobre el `backbuffer` de `1024x768x32bpp` (`3 MiB` en RAM).
   - **Modo Eco** (`wm_toggle_eco_mode()`): Sustituye las superficies translúcidas por relleno opaco directo (`gfx_fill_rect`) para minimizar el uso de CPU en hardware modesto.
3. **Barra Inferior y Dock Flotante**:
   - Altura: 36 píxeles en la parte inferior (`y = 732..768`).
   - Botón `"Zenthra"` a la izquierda (Menú Launcher con acceso a las 4 aplicaciones nativas).
   - Indicador del perfil de seguridad activo de la tarea actual.
   - **Dock Flotante** con botones de acceso directo a las 4 ventanas (`1:Shell`, `2:Mon`, `3:Jarvis`, `4:Agents`).
   - Botones interactivos `Eco:ON/OFF` y `Tema:D/L`, más reloj del sistema `MM:SS` sincronizado con el PIT a 100 Hz.
4. **Las 4 Ventanas Nativas del Diseño Definitivo (con distinción explícita `[REAL]` vs. `[SIMULADO]`)**:
   - **Ventana 1 — `Terminal Zenthra OS` (`[REAL]`)**: Consola interactiva conectada al teclado PS/2, PMM y Scheduler real (`help`, `sysinfo`, `mem`, `tasks`, `jarvis`, `agents`, `eco`, `theme`, `clear`).
   - **Ventana 2 — `Monitor de Sistema & IoT` (`[REAL + SIMULADO IoT]`)**: Muestra memoria libre y marcos reales del PMM (`[REAL]`), estado del planificador PIT 100 Hz (`[REAL]`) y telemetría de sensor etiquetada explícitamente como `[SIMULADO IoT]`.
   - **Ventana 3 — `Jarvis AI [DEMO LOCAL]` (`[SIMULADO - SIN RED]`)**: Copiloto local demostrativo con visualización de pasos del plan, estados (`JARVIS_IDLE`, `JARVIS_AWAITING_APPROVAL`, `JARVIS_COMPLETED_SIM`, `JARVIS_ABORTED`) y botones clicables con el ratón (`[Ejecutar]`, `[Aprobar]`, `[Abortar]`).
   - **Ventana 4 — `Centro de 6 Agentes [SIM]` (`[SIMULADO]`)**: Panel interactivo con los 6 agentes especializados (`Backend & APIs`, `Ciberseguridad`, `Kernel & OS`, `Datos & IA`, `Creativo & UI`, `Automatizacion`) seleccionables mediante clic de ratón o API (`wm_select_agent`).
