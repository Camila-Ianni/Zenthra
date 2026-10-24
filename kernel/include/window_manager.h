#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "ktypes.h"

#define MAX_WINDOWS 8
#define TITLEBAR_HEIGHT 22
#define TASKBAR_HEIGHT  36

/* Temas visuales nativos del Escritorio Zenthra (Auditoría v20) */
enum wm_theme {
    WM_THEME_DARK  = 0,
    WM_THEME_LIGHT = 1
};

/* Estados del Orquestador Nativo Jarvis AI (Demostración Local sin Red en Kernel) */
enum jarvis_plan_state {
    JARVIS_IDLE              = 0,
    JARVIS_AWAITING_APPROVAL = 1,
    JARVIS_COMPLETED_SIM     = 2,
    JARVIS_ABORTED           = 3
};

#define WM_AGENT_COUNT 6

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

/* Procesar eventos de interacción del cursor del ratón (clic, arrastre, cierre, dock, botones) */
void wm_handle_mouse(int32_t mx, int32_t my, bool left_button);

/* Entregar caracter de teclado a la ventana activa */
void wm_handle_keyboard(char c);

/* Solicitar redibujado de la escena en el próximo ciclo */
void wm_request_redraw(void);
bool wm_needs_redraw(void);

/* Renderizar la escena completa (fondo, ventanas con glassmorphism, dock, barra de tareas, cursor) */
void wm_render_desktop(void);

/* Actualizar contador de tiempo del sistema para el reloj de la barra de tareas */
void wm_update_clock(uint32_t ticks);

/* Control de Modo Eco (desactiva mezcla alfa glassmorphism para hardware limitado) y Temas (Auditoría v20) */
bool wm_is_eco_mode(void);
void wm_set_eco_mode(bool enabled);
void wm_toggle_eco_mode(void);
enum wm_theme wm_get_theme(void);
void wm_set_theme(enum wm_theme theme);
void wm_toggle_theme(void);

/* Control del Orquestador Nativo Jarvis AI y Centro de 6 Agentes (Auditoría v20) */
enum jarvis_plan_state wm_get_jarvis_state(void);
void wm_jarvis_start_plan(void);
void wm_jarvis_approve_step(void);
void wm_jarvis_abort_plan(void);
uint8_t wm_get_selected_agent(void);
void wm_select_agent(uint8_t index);

/* Métricas del gestor de ventanas */
size_t wm_get_window_count(void);
struct window* wm_get_window(size_t index);
struct window* wm_get_focused_window(void);

#endif
