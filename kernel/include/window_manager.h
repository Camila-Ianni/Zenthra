#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "ktypes.h"

#define MAX_WINDOWS 8
#define TITLEBAR_HEIGHT 22
#define TASKBAR_HEIGHT  36

struct window {
    uint32_t id;
    char title[32];
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t border_color;
    uint32_t titlebar_color;
    bool is_visible;
    bool is_focused;
    bool is_dragging;
    int32_t drag_off_x;
    int32_t drag_off_y;
    void (*on_paint)(struct window* win);
    void (*on_click)(struct window* win, int rel_x, int rel_y);
};

/* Inicializar el gestor de ventanas y crear las ventanas predeterminadas del sistema */
void wm_init(void);

/* Crear una nueva ventana en el escritorio */
struct window* wm_create_window(const char* title, int32_t x, int32_t y, uint32_t w, uint32_t h, void (*on_paint)(struct window*));

/* Cerrar / ocultar una ventana */
void wm_close_window(uint32_t id);

/* Enfocar una ventana trayéndola al frente */
void wm_focus_window(uint32_t id);

/* Procesar eventos de interacción del cursor del ratón (clic, arrastre, cierre) */
void wm_handle_mouse(int32_t mx, int32_t my, bool left_button);

/* Renderizar la escena completa (fondo, ventanas, barra de tareas, cursor) y volcar a pantalla */
void wm_render_desktop(void);

/* Actualizar contador de tiempo del sistema para el reloj de la barra de tareas */
void wm_update_clock(uint32_t ticks);

/* Métricas del gestor de ventanas */
size_t wm_get_window_count(void);
struct window* wm_get_window(size_t index);
struct window* wm_get_focused_window(void);

#endif
