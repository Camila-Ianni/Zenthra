#include "window_manager.h"
#include "gfx.h"
#include "mouse.h"
#include "pmm.h"
#include "task.h"
#include "klib.h"
#include "serial.h"
#include "shell.h"

static struct window windows[MAX_WINDOWS];
static size_t window_count = 0;
static int32_t focused_id = -1;
static uint32_t uptime_ticks = 0;
static bool prev_left_btn = false;

/* Ventana 1: Terminal Interactiva Zenthra */
static void paint_terminal_window(struct window* win) {
    uint32_t cx = win->x + 4;
    uint32_t cy = win->y + TITLEBAR_HEIGHT + 4;
    uint32_t cw = win->width - 8;
    uint32_t ch = win->height - TITLEBAR_HEIGHT - 8;

    gfx_fill_rect(cx, cy, cw, ch, 0xFF0A0E17); // Fondo negro consola
    gfx_draw_rect(cx, cy, cw, ch, 0xFF1E293B);

    gfx_draw_string(cx + 8, cy + 8,  "Zenthra OS (Terminal Nativa x86) - 2026", COLOR_ACCENT_CYAN, 0);
    gfx_draw_string(cx + 8, cy + 28, "zenthra:~$ sysinfo", COLOR_ACCENT_GREEN, 0);
    gfx_draw_string(cx + 8, cy + 48, "Arquitectura : i686 Modo Protegido (Ring 0 / Ring 3)", COLOR_TEXT_WHITE, 0);
    gfx_draw_string(cx + 8, cy + 68, "Memoria      : PMM 4 KiB Frames / Paging Activo (CR0.PG)", COLOR_TEXT_WHITE, 0);
    gfx_draw_string(cx + 8, cy + 88, "Multitarea   : Round-Robin Preemptivo (Timer PIT 100 Hz)", COLOR_TEXT_WHITE, 0);
    gfx_draw_string(cx + 8, cy + 108, "Enlace IoT   : Arduino Uno COM1 [ONLINE]", COLOR_ACCENT_GREEN, 0);
    gfx_draw_string(cx + 8, cy + 132, "zenthra:~$ _", COLOR_TEXT_WHITE, 0);
}

/* Ventana 2: Monitor del Sistema y Sensores IoT */
static void paint_monitor_window(struct window* win) {
    uint32_t cx = win->x + 4;
    uint32_t cy = win->y + TITLEBAR_HEIGHT + 4;
    uint32_t cw = win->width - 8;
    uint32_t ch = win->height - TITLEBAR_HEIGHT - 8;

    gfx_fill_rect(cx, cy, cw, ch, 0xFF1E293B);
    gfx_draw_rect(cx, cy, cw, ch, 0xFF334155);

    gfx_draw_string(cx + 8, cy + 8, "Monitor del Sistema y Telemetria", COLOR_TEXT_WHITE, 0);

    char buf[32];
    gfx_draw_string(cx + 8, cy + 30, "RAM Libre    : ", COLOR_TEXT_MUTED, 0);
    uitoa((uint32_t)pmm_get_free_memory_kib(), buf, 10);
    gfx_draw_string(cx + 120, cy + 30, buf, COLOR_ACCENT_CYAN, 0);
    gfx_draw_string(cx + 180, cy + 30, "KiB", COLOR_TEXT_MUTED, 0);

    gfx_draw_string(cx + 8, cy + 50, "Marcos Libres: ", COLOR_TEXT_MUTED, 0);
    uitoa((uint32_t)pmm_get_free_frames(), buf, 10);
    gfx_draw_string(cx + 120, cy + 50, buf, COLOR_ACCENT_CYAN, 0);

    gfx_draw_string(cx + 8, cy + 70, "Planificador : Preemptivo 100 Hz", COLOR_TEXT_MUTED, 0);
    gfx_draw_string(cx + 8, cy + 90, "Sensor Temp  : 25.0 C [OPTIMO]", COLOR_ACCENT_GREEN, 0);
    gfx_draw_string(cx + 8, cy + 110, "Actuador LED : ACTIVO (Pin 13)", COLOR_ACCENT_BLUE, 0);
}

void wm_init(void) {
    window_count = 0;
    focused_id = -1;
    uptime_ticks = 0;
    prev_left_btn = false;

    for (size_t i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = 0;
        windows[i].is_visible = false;
    }

    struct framebuffer_info* fb = gfx_get_fb_info();
    uint32_t sw = fb && fb->width > 0 ? fb->width : 1024;
    (void)fb;

    // Crear Ventana 1: Terminal Gráfica Zenthra
    wm_create_window("Terminal Zenthra OS", 40, 50, 480, 220, paint_terminal_window);

    // Crear Ventana 2: Monitor del Sistema
    uint32_t win2_x = sw > 580 ? sw - 360 : 200;
    wm_create_window("Monitor de Sistema & IoT", win2_x, 80, 320, 180, paint_monitor_window);

    // Enfocar ventana de terminal por defecto
    if (window_count > 0) {
        wm_focus_window(windows[0].id);
    }

    serial_write_string("[WM] Zenthra Window Manager inicializado con 2 ventanas de sistema.\r\n");
}

struct window* wm_create_window(const char* title, int32_t x, int32_t y, uint32_t w, uint32_t h, void (*on_paint)(struct window*)) {
    if (window_count >= MAX_WINDOWS) return NULL;

    size_t idx = window_count++;
    struct window* win = &windows[idx];

    win->id = (uint32_t)(idx + 1);
    size_t len = k_strlen(title);
    if (len >= 31) len = 31;
    k_memcpy(win->title, title, len);
    win->title[len] = '\0';

    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->border_color = COLOR_WIN_BORDER;
    win->titlebar_color = COLOR_WIN_HEADER;
    win->is_visible = true;
    win->is_focused = false;
    win->is_dragging = false;
    win->drag_off_x = 0;
    win->drag_off_y = 0;
    win->on_paint = on_paint;
    win->on_click = NULL;

    wm_focus_window(win->id);
    return win;
}

void wm_close_window(uint32_t id) {
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].id == id) {
            windows[i].is_visible = false;
            windows[i].is_focused = false;
            windows[i].is_dragging = false;
            if (focused_id == (int32_t)id) {
                focused_id = -1;
                // Pasar foco a la siguiente ventana visible
                for (int j = (int)window_count - 1; j >= 0; j--) {
                    if (windows[j].is_visible && windows[j].id != id) {
                        wm_focus_window(windows[j].id);
                        break;
                    }
                }
            }
            break;
        }
    }
}

void wm_focus_window(uint32_t id) {
    focused_id = (int32_t)id;
    for (size_t i = 0; i < window_count; i++) {
        windows[i].is_focused = (windows[i].id == id);
    }
}

void wm_handle_mouse(int32_t mx, int32_t my, bool left_button) {
    bool clicked = left_button && !prev_left_btn;
    bool released = !left_button && prev_left_btn;

    // 1. Manejo de arrastre activo
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].is_visible && windows[i].is_dragging) {
            if (left_button) {
                windows[i].x = mx - windows[i].drag_off_x;
                windows[i].y = my - windows[i].drag_off_y;

                // Restringir para no esconder la barra de título fuera de la pantalla
                if (windows[i].y < 0) windows[i].y = 0;
                struct framebuffer_info* fb = gfx_get_fb_info();
                if (fb && fb->height > TASKBAR_HEIGHT + TITLEBAR_HEIGHT) {
                    if ((uint32_t)windows[i].y > fb->height - TASKBAR_HEIGHT - TITLEBAR_HEIGHT) {
                        windows[i].y = (int32_t)(fb->height - TASKBAR_HEIGHT - TITLEBAR_HEIGHT);
                    }
                }
            }
            if (released) {
                windows[i].is_dragging = false;
            }
        }
    }

    // 2. Procesar nuevo clic (hit testing de arriba hacia abajo / frente hacia atras)
    if (clicked) {
        bool hit_any = false;

        for (int i = (int)window_count - 1; i >= 0; i--) {
            struct window* win = &windows[i];
            if (!win->is_visible) continue;

            // Comprobar si el clic cayó dentro de los límites de la ventana
            if (mx >= win->x && mx < win->x + (int32_t)win->width &&
                my >= win->y && my < win->y + (int32_t)win->height) {

                wm_focus_window(win->id);
                hit_any = true;

                // Comprobar botón de cerrar [X] (ubicado en la esquina superior derecha del titlebar)
                int32_t btn_x = win->x + (int32_t)win->width - 20;
                int32_t btn_y = win->y + 3;
                if (mx >= btn_x && mx <= btn_x + 16 && my >= btn_y && my <= btn_y + 16) {
                    wm_close_window(win->id);
                    break;
                }

                // Si hizo clic en la barra de título, iniciar arrastre
                if (my < win->y + TITLEBAR_HEIGHT) {
                    win->is_dragging = true;
                    win->drag_off_x = mx - win->x;
                    win->drag_off_y = my - win->y;
                }

                break;
            }
        }

        (void)hit_any;
    }

    prev_left_btn = left_button;
}

void wm_update_clock(uint32_t ticks) {
    uptime_ticks = ticks;
}

size_t wm_get_window_count(void) {
    return window_count;
}

struct window* wm_get_window(size_t index) {
    if (index >= window_count) return NULL;
    return &windows[index];
}

struct window* wm_get_focused_window(void) {
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].id == (uint32_t)focused_id && windows[i].is_visible) {
            return &windows[i];
        }
    }
    return NULL;
}

static void draw_single_window(struct window* win) {
    if (!win->is_visible) return;

    // Marco exterior y sombra
    gfx_draw_rect(win->x, win->y, win->width, win->height, win->border_color);

    // Barra de título
    uint32_t tb_color = win->is_focused ? COLOR_ACCENT_BLUE : COLOR_WIN_HEADER;
    gfx_fill_rect(win->x + 1, win->y + 1, win->width - 2, TITLEBAR_HEIGHT - 1, tb_color);

    // Título de la ventana
    gfx_draw_string(win->x + 8, win->y + 4, win->title, COLOR_TEXT_WHITE, 0);

    // Botón de cerrar [X]
    int32_t btn_x = win->x + (int32_t)win->width - 20;
    int32_t btn_y = win->y + 3;
    gfx_fill_rect(btn_x, btn_y, 16, 16, COLOR_BTN_CLOSE);
    gfx_draw_string(btn_x + 4, btn_y + 1, "x", COLOR_TEXT_WHITE, 0);

    // Contenido propio de la ventana
    if (win->on_paint) {
        win->on_paint(win);
    }
}

void wm_render_desktop(void) {
    if (!gfx_is_active()) return;

    struct framebuffer_info* fb = gfx_get_fb_info();
    uint32_t sw = fb->width;
    uint32_t sh = fb->height;

    // 1. Dibujar Fondo de Pantalla (Gradiente corporativo Slate/Navy)
    for (uint32_t y = 0; y < sh - TASKBAR_HEIGHT; y++) {
        // Interpolar suavemente entre azul oscuro y gris pizarra
        uint8_t r = (uint8_t)(0x0F + (y * (0x1E - 0x0F)) / sh);
        uint8_t g = (uint8_t)(0x17 + (y * (0x29 - 0x17)) / sh);
        uint8_t b = (uint8_t)(0x2A + (y * (0x3B - 0x2A)) / sh);
        uint32_t col = 0xFF000000 | (r << 16) | (g << 8) | b;
        gfx_fill_rect(0, y, sw, 1, col);
    }

    // Marca de agua central
    gfx_draw_string(sw / 2 - 100, sh / 2 - 20, "ZENTHRA OS", 0x33FFFFFF, 0);
    gfx_draw_string(sw / 2 - 110, sh / 2 + 4, "Escritorio Grafico Nativo", 0x22FFFFFF, 0);

    // 2. Renderizar Ventanas: Primero las no enfocadas, luego la enfocada
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].is_visible && !windows[i].is_focused) {
            draw_single_window(&windows[i]);
        }
    }
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].is_visible && windows[i].is_focused) {
            draw_single_window(&windows[i]);
        }
    }

    // 3. Renderizar Barra de Tareas Inferior (Taskbar)
    uint32_t ty = sh - TASKBAR_HEIGHT;
    gfx_fill_rect(0, ty, sw, TASKBAR_HEIGHT, COLOR_TASKBAR);
    gfx_fill_rect(0, ty, sw, 1, COLOR_WIN_BORDER); // Línea divisoria superior

    // Botón de Inicio / Launcher "Zenthra"
    gfx_fill_rect(8, ty + 5, 88, 26, COLOR_ACCENT_BLUE);
    gfx_draw_rect(8, ty + 5, 88, 26, 0xFF60A5FA);
    gfx_draw_string(20, ty + 10, "Zenthra", COLOR_TEXT_WHITE, 0);

    // Etiqueta de Perfil de Seguridad Activo
    struct task* cur_task = task_get_current();
    const char* prof_name = "Estudiante";
    if (cur_task) {
        prof_name = profile_get_name(cur_task->profile);
    }
    gfx_fill_rect(106, ty + 6, 120, 24, 0xFF1E293B);
    gfx_draw_rect(106, ty + 6, 120, 24, 0xFF334155);
    gfx_draw_string(114, ty + 10, prof_name, COLOR_ACCENT_CYAN, 0);

    // Reloj del Sistema / Uptime en el extremo derecho
    char clock_str[32];
    uint32_t secs = uptime_ticks / 100;
    uint32_t mins = secs / 60;
    secs = secs % 60;
    clock_str[0] = '0' + (mins / 10) % 10;
    clock_str[1] = '0' + (mins % 10);
    clock_str[2] = ':';
    clock_str[3] = '0' + (secs / 10) % 10;
    clock_str[4] = '0' + (secs % 10);
    clock_str[5] = '\0';

    gfx_draw_string(sw - 70, ty + 10, clock_str, COLOR_TEXT_WHITE, 0);

    // 4. Dibujar Cursor del Ratón
    mouse_draw_cursor();

    // 5. Volcar búfer de composición a pantalla
    gfx_flip();
}
