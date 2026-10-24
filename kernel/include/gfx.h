#ifndef GFX_H
#define GFX_H

#include "ktypes.h"
#include "multiboot.h"

/* Estructura descriptora del Framebuffer y Doble Búfer */
struct framebuffer_info {
    uint32_t* address;     // Dirección física/virtual del Linear Framebuffer (frontbuffer)
    uint32_t width;        // Ancho en píxeles (e.g. 1024)
    uint32_t height;       // Alto en píxeles (e.g. 768)
    uint32_t pitch;        // Bytes por scanline (e.g. 4096)
    uint8_t  bpp;          // Bits por píxel (e.g. 32)
    uint32_t* backbuffer;  // Búfer en RAM para doble búfer (evita tearing/parpadeo)
    bool is_active;        // true si el modo gráfico lineal está activo
};

/* Paleta de colores estándar Zenthra GUI (formato ARGB 32-bit: 0xAARRGGBB) */
#define COLOR_BLACK         0xFF000000
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_DARK_BG       0xFF1A1E24
#define COLOR_DESKTOP_TOP   0xFF1E293B
#define COLOR_DESKTOP_BOT   0xFF0F172A
#define COLOR_TASKBAR       0xFF0B0F19
#define COLOR_WIN_BG        0xFF242933
#define COLOR_WIN_HEADER    0xFF333A48
#define COLOR_WIN_BORDER    0xFF4B5563
#define COLOR_TEXT_WHITE    0xFFF8FAFC
#define COLOR_TEXT_MUTED    0xFF94A3B8
#define COLOR_ACCENT_BLUE   0xFF3B82F6
#define COLOR_ACCENT_CYAN   0xFF06B6D4
#define COLOR_ACCENT_GREEN  0xFF10B981
#define COLOR_BTN_CLOSE     0xFFEF4444
#define COLOR_BTN_CLOSE_HOV 0xFFDC2626

/* Inicialización del subsistema gráfico */
bool gfx_init(uint32_t magic, uintptr_t mbi_addr);
void gfx_init_custom(uint32_t* frontbuffer, uint32_t* backbuffer, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);
bool gfx_is_active(void);
struct framebuffer_info* gfx_get_fb_info(void);

/* Primitivas de renderizado 2D y Glassmorphism (Mezcla Alfa ARGB) */
void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t gfx_getpixel(uint32_t x, uint32_t y);
uint32_t gfx_blend_argb(uint32_t dst, uint32_t src);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_fill_rect_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t argb_color);
void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t color);
void gfx_fill_rounded_rect_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t argb_color);
void gfx_draw_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t color);
void gfx_fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color);
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void gfx_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg);
void gfx_clear(uint32_t color);
void gfx_flip(void);

#endif
