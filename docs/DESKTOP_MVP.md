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

### 5.2 Elementos Visuales del Escritorio
1. **Fondo de Pantalla**: Gradiente de color azul profundo / gris grafito corporativo (`#1A1E24`).
2. **Barra de Tareas Inferior (Taskbar)**:
   - Altura: 36 píxeles en la parte inferior (`y = 732..768`).
   - Botón *"Zenthra"* a la izquierda (Menú de Inicio / Perfiles).
   - Reloj del sistema a la derecha (actualizado por interrupciones PIT).
   - Indicador de perfil activo (ej. *"Estudiante"*, *"Backend"*).
3. **Ventanas Movibles**:
   - Barra de título de 22 píxeles con botón de cierre [X].
   - Al hacer clic en la barra de título, la ventana adquiere el foco y permite arrastre (*drag-and-drop*).
4. **Ventana de Terminal Gráfica**:
   - Ventana con emulador de consola interactiva que ejecuta la shell de Zenthra con soporte de comandos internos.
