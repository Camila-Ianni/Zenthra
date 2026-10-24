#include "gfx.h"
#include "font8x16.h"
#include "klib.h"
#include "kheap.h"
#include "serial.h"

static struct framebuffer_info current_fb;

#if !defined(__KERNEL__)
#include <stdlib.h>
#endif

#include "vmm.h"

bool gfx_init(uint32_t magic, uintptr_t mbi_addr) {
    k_memset(&current_fb, 0, sizeof(struct framebuffer_info));

    if (magic != MULTIBOOT1_BOOTLOADER_MAGIC || mbi_addr == 0) {
        serial_write_string("[GFX] Modo grafico no inicializado: MBI no valido. Modo VGA activo.\r\n");
        return false;
    }

    struct multiboot1_info* mbi = (struct multiboot1_info*)mbi_addr;

    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        serial_write_string("[GFX] GRUB no entrego Linear Framebuffer (LFB). Modo VGA texto activo.\r\n");
        return false;
    }

    // Comprobar que la dirección física del Framebuffer sea válida y direccionable en 32 bits (i686)
    if (mbi->framebuffer_addr == 0 || mbi->framebuffer_addr > 0xFFFFFFFFULL) {
        serial_write_string("[GFX] Direccion de Framebuffer invalida o > 4 GiB no soportada en i686. Modo VGA activo.\r\n");
        return false;
    }

    current_fb.width = mbi->framebuffer_width;
    current_fb.height = mbi->framebuffer_height;
    current_fb.pitch = mbi->framebuffer_pitch;
    current_fb.bpp = mbi->framebuffer_bpp;

    // Validación estricta de parámetros para el MVP: exclusivamente 32 bpp (ARGB 8888)
    if (current_fb.bpp != 32) {
        serial_write_string("[GFX] Modo de color no soportado (se requiere bpp == 32). Modo VGA texto activo.\r\n");
        return false;
    }

    if (current_fb.width < 320 || current_fb.width > 2560 || 
        current_fb.height < 200 || current_fb.height > 1600) {
        serial_write_string("[GFX] Resolucion fuera de limites soportados (320x200 a 2560x1600).\r\n");
        return false;
    }

    if (current_fb.pitch < current_fb.width * 4 || (current_fb.pitch % 4) != 0) {
        serial_write_string("[GFX] Pitch invalido o no alineado a 4 bytes. Modo VGA texto activo.\r\n");
        return false;
    }

    // Cálculo seguro de tamaño de buffer con protección contra desbordamiento de enteros
    uint64_t total_size = (uint64_t)current_fb.height * (uint64_t)current_fb.pitch;
    if (total_size == 0 || total_size > 32 * 1024 * 1024) { // Límite de seguridad: 32 MiB
        serial_write_string("[GFX] Tamano de buffer excede limites seguros de memoria.\r\n");
        return false;
    }

    // Validar que el rango completo físico no desborde el límite de 4 GiB
    if (mbi->framebuffer_addr + total_size < mbi->framebuffer_addr || 
        mbi->framebuffer_addr + total_size > 0x100000000ULL) {
        serial_write_string("[GFX] Rango fisico de Framebuffer desborda espacio de 32 bits.\r\n");
        return false;
    }
    size_t buffer_size = (size_t)total_size;

#if defined(__KERNEL__)
    // Mapear la región física del Linear Framebuffer alineada por páginas en VMM
    uintptr_t fb_virt_ptr = 0;
    if (vmm_is_paging_enabled()) {
        int map_err = vmm_map_physical_range(vmm_get_kernel_directory(), mbi->framebuffer_addr, buffer_size, PTE_PRESENT | PTE_WRITABLE, &fb_virt_ptr);
        if (map_err != 0) {
            serial_write_string("[GFX] Error: no se pudo mapear la region MMIO del Framebuffer en VMM.\r\n");
            return false;
        }
        current_fb.address = (uint32_t*)fb_virt_ptr;
    } else {
        current_fb.address = (uint32_t*)(uintptr_t)mbi->framebuffer_addr;
    }

    current_fb.backbuffer = (uint32_t*)kmalloc(buffer_size);
#else
    current_fb.address = (uint32_t*)(uintptr_t)mbi->framebuffer_addr;
    current_fb.backbuffer = (uint32_t*)malloc(buffer_size);
#endif

    if (!current_fb.backbuffer) {
        // Si no hay memoria suficiente para doble búfer en RAM, dibujar directamente en frontbuffer
        current_fb.backbuffer = current_fb.address;
    } else {
        k_memset(current_fb.backbuffer, 0, buffer_size);
    }

    current_fb.is_active = true;

    char buf[16];
    serial_write_string("[GFX] Framebuffer VESA LFB inicializado: ");
    uitoa(current_fb.width, buf, 10);
    serial_write_string(buf);
    serial_write_string("x");
    uitoa(current_fb.height, buf, 10);
    serial_write_string(buf);
    serial_write_string(" a 32 bpp (Pitch: ");
    uitoa(current_fb.pitch, buf, 10);
    serial_write_string(buf);
    serial_write_string(" bytes, Doble bufer activo)\r\n");

    return true;
}

void gfx_init_custom(uint32_t* frontbuffer, uint32_t* backbuffer, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp) {
    current_fb.address = frontbuffer;
    current_fb.backbuffer = backbuffer ? backbuffer : frontbuffer;
    current_fb.width = width;
    current_fb.height = height;
    current_fb.pitch = pitch;
    current_fb.bpp = bpp;
    current_fb.is_active = true;
}

bool gfx_is_active(void) {
    return current_fb.is_active;
}

struct framebuffer_info* gfx_get_fb_info(void) {
    return &current_fb;
}

void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!current_fb.is_active) return;
    if (x >= current_fb.width || y >= current_fb.height) return;

    uint32_t* target = current_fb.backbuffer ? current_fb.backbuffer : current_fb.address;
    if (!target) return;

    size_t stride_pixels = current_fb.pitch / 4;
    target[y * stride_pixels + x] = color;
}

uint32_t gfx_getpixel(uint32_t x, uint32_t y) {
    if (!current_fb.is_active) return 0;
    if (x >= current_fb.width || y >= current_fb.height) return 0;

    uint32_t* target = current_fb.backbuffer ? current_fb.backbuffer : current_fb.address;
    if (!target) return 0;

    size_t stride_pixels = current_fb.pitch / 4;
    return target[y * stride_pixels + x];
}

uint32_t gfx_blend_argb(uint32_t dst, uint32_t src) {
    uint32_t alpha = (src >> 24) & 0xFF;
    if (alpha == 0) return dst;
    if (alpha == 255) return 0xFF000000 | (src & 0x00FFFFFF);

    uint32_t inv_alpha = 255 - alpha;
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t r = (sr * alpha + dr * inv_alpha) / 255;
    uint32_t g = (sg * alpha + dg * inv_alpha) / 255;
    uint32_t b = (sb * alpha + db * inv_alpha) / 255;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!current_fb.is_active || w == 0 || h == 0) return;

    // Recorte (Clipping) contra los bordes de la pantalla
    if (x >= current_fb.width || y >= current_fb.height) return;
    if (x + w > current_fb.width) w = current_fb.width - x;
    if (y + h > current_fb.height) h = current_fb.height - y;

    uint32_t* target = current_fb.backbuffer ? current_fb.backbuffer : current_fb.address;
    if (!target) return;

    size_t stride_pixels = current_fb.pitch / 4;

    for (uint32_t row = 0; row < h; row++) {
        uint32_t* line = &target[(y + row) * stride_pixels + x];
        for (uint32_t col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

void gfx_fill_rect_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t argb_color) {
    if (!current_fb.is_active || w == 0 || h == 0) return;

    uint32_t alpha = (argb_color >> 24) & 0xFF;
    if (alpha == 0) return;
    if (alpha == 255) {
        gfx_fill_rect(x, y, w, h, 0xFF000000 | (argb_color & 0x00FFFFFF));
        return;
    }

    // Recorte (Clipping) contra los bordes de la pantalla
    if (x >= current_fb.width || y >= current_fb.height) return;
    if (x + w > current_fb.width) w = current_fb.width - x;
    if (y + h > current_fb.height) h = current_fb.height - y;

    uint32_t* target = current_fb.backbuffer ? current_fb.backbuffer : current_fb.address;
    if (!target) return;

    size_t stride_pixels = current_fb.pitch / 4;

    for (uint32_t row = 0; row < h; row++) {
        uint32_t* line = &target[(y + row) * stride_pixels + x];
        for (uint32_t col = 0; col < w; col++) {
            line[col] = gfx_blend_argb(line[col], argb_color);
        }
    }
}

void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (w == 0 || h == 0) return;

    gfx_fill_rect(x, y, w, 1, color);             // Borde superior
    gfx_fill_rect(x, y + h - 1, w, 1, color);     // Borde inferior
    gfx_fill_rect(x, y, 1, h, color);             // Borde izquierdo
    gfx_fill_rect(x + w - 1, y, 1, h, color);     // Borde derecho
}

void gfx_fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color) {
    if (r <= 0) return;
    int32_t r2 = r * r;
    for (int32_t dy = -r; dy <= r; dy++) {
        int32_t py = cy + dy;
        if (py < 0 || (uint32_t)py >= current_fb.height) continue;
        for (int32_t dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int32_t px = cx + dx;
                if (px >= 0 && (uint32_t)px < current_fb.width) {
                    gfx_putpixel((uint32_t)px, (uint32_t)py, color);
                }
            }
        }
    }
}

void gfx_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t color) {
    if (!current_fb.is_active || w == 0 || h == 0) return;
    if (r == 0) {
        gfx_fill_rect(x, y, w, h, color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    gfx_fill_rect(x, y + r, w, h - 2 * r, color);
    gfx_fill_rect(x + r, y, w - 2 * r, r, color);
    gfx_fill_rect(x + r, y + h - r, w - 2 * r, r, color);

    int32_t r2 = (int32_t)(r * r);
    for (int32_t dy = 0; dy < (int32_t)r; dy++) {
        for (int32_t dx = 0; dx < (int32_t)r; dx++) {
            int32_t dist = (dx - (int32_t)r + 1) * (dx - (int32_t)r + 1) + (dy - (int32_t)r + 1) * (dy - (int32_t)r + 1);
            if (dist <= r2) {
                gfx_putpixel(x + dx, y + dy, color);
                gfx_putpixel(x + w - r + dx, y + dy, color);
                gfx_putpixel(x + dx, y + h - r + dy, color);
                gfx_putpixel(x + w - r + dx, y + h - r + dy, color);
            }
        }
    }
}

void gfx_fill_rounded_rect_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t argb_color) {
    if (!current_fb.is_active || w == 0 || h == 0) return;
    if (r == 0) {
        gfx_fill_rect_alpha(x, y, w, h, argb_color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    gfx_fill_rect_alpha(x, y + r, w, h - 2 * r, argb_color);
    gfx_fill_rect_alpha(x + r, y, w - 2 * r, r, argb_color);
    gfx_fill_rect_alpha(x + r, y + h - r, w - 2 * r, r, argb_color);

    int32_t r2 = (int32_t)(r * r);
    for (int32_t dy = 0; dy < (int32_t)r; dy++) {
        for (int32_t dx = 0; dx < (int32_t)r; dx++) {
            int32_t dist = (dx - (int32_t)r + 1) * (dx - (int32_t)r + 1) + (dy - (int32_t)r + 1) * (dy - (int32_t)r + 1);
            if (dist <= r2) {
                uint32_t px0 = x + dx;
                uint32_t py0 = y + dy;
                gfx_putpixel(px0, py0, gfx_blend_argb(gfx_getpixel(px0, py0), argb_color));

                uint32_t px1 = x + w - r + dx;
                uint32_t py1 = y + dy;
                gfx_putpixel(px1, py1, gfx_blend_argb(gfx_getpixel(px1, py1), argb_color));

                uint32_t px2 = x + dx;
                uint32_t py2 = y + h - r + dy;
                gfx_putpixel(px2, py2, gfx_blend_argb(gfx_getpixel(px2, py2), argb_color));

                uint32_t px3 = x + w - r + dx;
                uint32_t py3 = y + h - r + dy;
                gfx_putpixel(px3, py3, gfx_blend_argb(gfx_getpixel(px3, py3), argb_color));
            }
        }
    }
}

void gfx_draw_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t r, uint32_t color) {
    if (!current_fb.is_active || w == 0 || h == 0) return;
    if (r == 0) {
        gfx_draw_rect(x, y, w, h, color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    gfx_fill_rect(x + r, y, w - 2 * r, 1, color);
    gfx_fill_rect(x + r, y + h - 1, w - 2 * r, 1, color);
    gfx_fill_rect(x, y + r, 1, h - 2 * r, color);
    gfx_fill_rect(x + w - 1, y + r, 1, h - 2 * r, color);

    int32_t r2_outer = (int32_t)(r * r);
    int32_t r2_inner = (int32_t)((r > 1 ? r - 1 : 0) * (r > 1 ? r - 1 : 0));

    for (int32_t dy = 0; dy < (int32_t)r; dy++) {
        for (int32_t dx = 0; dx < (int32_t)r; dx++) {
            int32_t dist = (dx - (int32_t)r + 1) * (dx - (int32_t)r + 1) + (dy - (int32_t)r + 1) * (dy - (int32_t)r + 1);
            if (dist <= r2_outer && dist >= r2_inner) {
                gfx_putpixel(x + dx, y + dy, color);
                gfx_putpixel(x + w - r + dx, y + dy, color);
                gfx_putpixel(x + dx, y + h - r + dy, color);
                gfx_putpixel(x + w - r + dx, y + h - r + dy, color);
            }
        }
    }
}

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x0 >= 0 && (uint32_t)x0 < current_fb.width && y0 >= 0 && (uint32_t)y0 < current_fb.height) {
            gfx_putpixel((uint32_t)x0, (uint32_t)y0, color);
        }

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if ((uint8_t)c < 32 || (uint8_t)c > 127) {
        c = '?';
    }

    uint8_t idx = (uint8_t)c - 32;

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = font8x16[idx][row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                gfx_putpixel(x + col, y + row, fg);
            } else if (bg != 0) {
                gfx_putpixel(x + col, y + row, bg);
            }
        }
    }
}

void gfx_draw_string(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg) {
    if (!str) return;

    uint32_t cur_x = x;
    uint32_t cur_y = y;

    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT + 2;
        } else if (*str == '\r') {
            cur_x = x;
        } else if (*str == '\t') {
            cur_x += FONT_WIDTH * 4;
        } else {
            gfx_draw_char(cur_x, cur_y, *str, fg, bg);
            cur_x += FONT_WIDTH;
        }
        str++;
    }
}

void gfx_clear(uint32_t color) {
    if (!current_fb.is_active) return;
    gfx_fill_rect(0, 0, current_fb.width, current_fb.height, color);
}

void gfx_flip(void) {
    if (!current_fb.is_active) return;
    if (!current_fb.address || !current_fb.backbuffer || current_fb.address == current_fb.backbuffer) {
        return;
    }

    size_t total_bytes = current_fb.height * current_fb.pitch;
    k_memcpy(current_fb.address, current_fb.backbuffer, total_bytes);
}
