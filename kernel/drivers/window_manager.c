#include "window_manager.h"
#include "design_tokens.h"
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
static uint32_t last_clock_sec = 0xFFFFFFFF;
static bool prev_left_btn = false;
static bool needs_redraw = true;
static bool launcher_open = false;

static bool eco_mode = false;
static enum wm_theme active_theme = WM_THEME_DARK;
static enum jarvis_plan_state jarvis_state = JARVIS_IDLE;
static uint8_t selected_agent = 0;

static const char* agent_names[WM_AGENT_COUNT] = {
    "1. Backend & APIs   [SIM]",
    "2. Ciberseguridad   [SIM]",
    "3. Kernel & OS      [SIM]",
    "4. Datos & IA       [SIM]",
    "5. Creativo & UI    [SIM]",
    "6. Automatizacion   [SIM]"
};

static const char* agent_roles[WM_AGENT_COUNT] = {
    "Rol: Servicios REST y persistencia [SIMULADO]",
    "Rol: Auditoria ACL Ring 3 y Page Faults [SIMULADO]",
    "Rol: Telemetria PMM/VMM y Scheduler [SIMULADO]",
    "Rol: Inferencia local asistida [SIMULADO]",
    "Rol: Compositor VESA LFB Glassmorphism [SIMULADO]",
    "Rol: Pipelines CI/CD y verificacion QEMU [SIMULADO]"
};

#define TERM_MAX_LINES 5
#define TERM_LINE_LEN  46
static char term_history[TERM_MAX_LINES][TERM_LINE_LEN];
static size_t term_hist_count = 0;
static char term_input[TERM_LINE_LEN] = {0};
static size_t term_input_len = 0;

void wm_request_redraw(void) {
    needs_redraw = true;
}

bool wm_needs_redraw(void) {
    return needs_redraw;
}

bool wm_is_eco_mode(void) {
    return eco_mode;
}

void wm_set_eco_mode(bool enabled) {
    eco_mode = enabled;
    wm_request_redraw();
}

void wm_toggle_eco_mode(void) {
    eco_mode = !eco_mode;
    wm_request_redraw();
}

enum wm_theme wm_get_theme(void) {
    return active_theme;
}

void wm_set_theme(enum wm_theme theme) {
    if (theme == WM_THEME_DARK || theme == WM_THEME_LIGHT) {
        active_theme = theme;
        wm_request_redraw();
    }
}

void wm_toggle_theme(void) {
    active_theme = (active_theme == WM_THEME_DARK) ? WM_THEME_LIGHT : WM_THEME_DARK;
    wm_request_redraw();
}

enum jarvis_plan_state wm_get_jarvis_state(void) {
    return jarvis_state;
}

void wm_jarvis_start_plan(void) {
    jarvis_state = JARVIS_AWAITING_APPROVAL;
    wm_request_redraw();
}

void wm_jarvis_approve_step(void) {
    if (jarvis_state == JARVIS_AWAITING_APPROVAL) {
        jarvis_state = JARVIS_COMPLETED_SIM;
        wm_request_redraw();
    }
}

void wm_jarvis_abort_plan(void) {
    if (jarvis_state == JARVIS_AWAITING_APPROVAL || jarvis_state == JARVIS_IDLE) {
        jarvis_state = JARVIS_ABORTED;
        wm_request_redraw();
    }
}

uint8_t wm_get_selected_agent(void) {
    return selected_agent;
}

void wm_select_agent(uint8_t index) {
    if (index < WM_AGENT_COUNT) {
        selected_agent = index;
        wm_request_redraw();
    }
}

static void wm_draw_surface(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t argb_glass, uint32_t rgb_opaque) {
    if (eco_mode) {
        gfx_fill_rect(x, y, w, h, rgb_opaque);
    } else {
        gfx_fill_rect_alpha(x, y, w, h, argb_glass);
    }
}

static void term_add_line(const char* text) {
    if (!text) return;
    if (term_hist_count >= TERM_MAX_LINES) {
        for (size_t i = 1; i < TERM_MAX_LINES; i++) {
            k_memcpy(term_history[i - 1], term_history[i], TERM_LINE_LEN);
        }
        term_hist_count = TERM_MAX_LINES - 1;
    }
    size_t len = k_strlen(text);
    if (len >= TERM_LINE_LEN - 1) len = TERM_LINE_LEN - 2;
    k_memcpy(term_history[term_hist_count], text, len);
    term_history[term_hist_count][len] = '\0';
    term_hist_count++;
}

static void term_execute_command(const char* cmd) {
    if (!cmd || k_strlen(cmd) == 0) return;

    char echo_buf[TERM_LINE_LEN];
    echo_buf[0] = '>'; echo_buf[1] = ' ';
    size_t cmd_len = k_strlen(cmd);
    if (cmd_len > TERM_LINE_LEN - 3) cmd_len = TERM_LINE_LEN - 3;
    k_memcpy(&echo_buf[2], cmd, cmd_len);
    echo_buf[2 + cmd_len] = '\0';
    term_add_line(echo_buf);

    if (k_strcmp(cmd, "help") == 0) {
        term_add_line("Cmds: help sysinfo mem tasks jarvis eco theme");
    } else if (k_strcmp(cmd, "sysinfo") == 0) {
        term_add_line("Zenthra OS v20.0 (i686 VESA LFB Nativo)");
        char buf[32];
        char line[TERM_LINE_LEN];
        size_t pos = 0;
        k_memcpy(&line[pos], "RAM: ", 5); pos += 5;
        uitoa((uint32_t)pmm_get_free_memory_kib(), buf, 10);
        size_t blen = k_strlen(buf);
        k_memcpy(&line[pos], buf, blen); pos += blen;
        k_memcpy(&line[pos], " / ", 3); pos += 3;
        uitoa((uint32_t)pmm_get_total_memory_kib(), buf, 10);
        blen = k_strlen(buf);
        k_memcpy(&line[pos], buf, blen); pos += blen;
        k_memcpy(&line[pos], " KiB [REAL]", 11); pos += 11;
        line[pos] = '\0';
        term_add_line(line);
    } else if (k_strcmp(cmd, "mem") == 0) {
        char buf[32];
        uitoa((uint32_t)pmm_get_free_memory_kib(), buf, 10);
        char out[44];
        k_memcpy(out, "RAM Libre: ", 11);
        size_t blen = k_strlen(buf);
        k_memcpy(&out[11], buf, blen);
        k_memcpy(&out[11 + blen], " KiB [REAL]", 12);
        term_add_line(out);
    } else if (k_strcmp(cmd, "tasks") == 0) {
        int count = 0;
        for (int i = 0; i < MAX_TASKS; i++) {
            struct task_info info;
            if (task_get_info_at(i, &info)) {
                count++;
                const char* st = "UNK";
                if (info.state == TASK_READY) st = "RDY";
                else if (info.state == TASK_RUNNING) st = "RUN";
                else if (info.state == TASK_SLEEPING) st = "SLP";
                else if (info.state == TASK_TERMINATED) st = "TRM";

                char line[TERM_LINE_LEN];
                char pid_buf[16];
                uitoa(info.id, pid_buf, 10);
                size_t pos = 0;
                k_memcpy(&line[pos], "T", 1); pos += 1;
                size_t plen = k_strlen(pid_buf);
                k_memcpy(&line[pos], pid_buf, plen); pos += plen;
                line[pos++] = ':'; line[pos++] = ' ';
                size_t nlen = k_strlen(info.name);
                if (nlen > 18) nlen = 18;
                k_memcpy(&line[pos], info.name, nlen); pos += nlen;
                line[pos++] = ' '; line[pos++] = '[';
                size_t slen = k_strlen(st);
                k_memcpy(&line[pos], st, slen); pos += slen;
                line[pos++] = ']';
                line[pos] = '\0';
                term_add_line(line);
            }
        }
        if (count == 0) {
            term_add_line("Sin tareas activas.");
        }
    } else if (k_strcmp(cmd, "jarvis") == 0) {
        wm_jarvis_start_plan();
        term_add_line("Jarvis [DEMO LOCAL]: Plan iniciado.");
    } else if (k_strcmp(cmd, "agents") == 0) {
        if (window_count > 3) {
            windows[3].is_visible = true;
            wm_focus_window(windows[3].id);
        }
        term_add_line("Centro de 6 Agentes [SIMULADO] enfocado.");
    } else if (k_strcmp(cmd, "eco") == 0) {
        wm_toggle_eco_mode();
        term_add_line(eco_mode ? "Modo Eco: ACTIVADO (Sin Alpha)" : "Modo Eco: DESACTIVADO (Glass)");
    } else if (k_strcmp(cmd, "theme") == 0) {
        wm_toggle_theme();
        term_add_line(active_theme == WM_THEME_LIGHT ? "Tema: CLARO activo" : "Tema: OSCURO activo");
    } else if (k_strcmp(cmd, "clear") == 0) {
        term_hist_count = 0;
    } else {
        term_add_line("Comando no reconocido. Escriba 'help'.");
    }
}

void wm_handle_keyboard(char c) {
    if (focused_id != 1) return;

    if (c == '\n' || c == '\r') {
        term_input[term_input_len] = '\0';
        term_execute_command(term_input);
        term_input_len = 0;
        term_input[0] = '\0';
        wm_request_redraw();
    } else if (c == '\b') {
        if (term_input_len > 0) {
            term_input_len--;
            term_input[term_input_len] = '\0';
            wm_request_redraw();
        }
    } else if (c >= 32 && c <= 126) {
        if (term_input_len < TERM_LINE_LEN - 2) {
            term_input[term_input_len++] = c;
            term_input[term_input_len] = '\0';
            wm_request_redraw();
        }
    }
}

/* Ventana 1: Terminal Interactiva Zenthra [REAL] */
static void paint_terminal_window(struct window* win) {
    uint32_t cx = win->x + 6;
    uint32_t cy = win->y + TITLEBAR_HEIGHT + 6;
    uint32_t cw = win->width - 12;
    uint32_t ch = win->height - TITLEBAR_HEIGHT - 12;

    // Interior de consola obsidiana con esquinas redondeadas
    if (eco_mode) {
        gfx_fill_rounded_rect(cx, cy, cw, ch, 6, 0xFF080C14);
    } else {
        gfx_fill_rounded_rect_alpha(cx, cy, cw, ch, 6, 0xEE080C14);
    }
    gfx_draw_rounded_rect(cx, cy, cw, ch, 6, 0xFF1E293B);

    // Barra de estado interna de la shell
    gfx_fill_rounded_rect(cx + 4, cy + 4, cw - 8, 20, 4, 0xFF0F172A);
    gfx_fill_circle(cx + 14, cy + 14, 3, 0xFF10B981);
    gfx_draw_string(cx + 24, cy + 6, "zenthra-shell x86 -- Ring 0/3 [REAL]", 0xFF38BDF8, 0);

    // Historial de comandos
    uint32_t line_y = cy + 30;
    for (size_t i = 0; i < term_hist_count; i++) {
        uint32_t col = (term_history[i][0] == '>') ? 0xFF38BDF8 : 0xFFE2E8F0;
        gfx_draw_string(cx + 10, line_y, term_history[i], col, 0);
        line_y += 18;
    }

    // Prompt interactivo estilo Linux/macOS
    char prompt_buf[TERM_LINE_LEN + 18];
    const char* prefix = "zenthra@os:~$ ";
    k_memcpy(prompt_buf, prefix, 14);
    k_memcpy(&prompt_buf[14], term_input, term_input_len);
    prompt_buf[14 + term_input_len] = '_';
    prompt_buf[14 + term_input_len + 1] = '\0';

    gfx_draw_string(cx + 10, cy + ch - 22, prompt_buf, 0xFF34D399, 0);
}

/* Ventana 2: Monitor del Sistema [REAL] y Sensores IoT [SIMULADO] */
static void paint_monitor_window(struct window* win) {
    uint32_t cx = win->x + 6;
    uint32_t cy = win->y + TITLEBAR_HEIGHT + 6;
    uint32_t cw = win->width - 12;
    uint32_t ch = win->height - TITLEBAR_HEIGHT - 12;

    uint32_t card_bg = (active_theme == WM_THEME_LIGHT) ? 0xCCF8FAFC : 0xD80F172A;
    uint32_t card_solid = (active_theme == WM_THEME_LIGHT) ? 0xFFF8FAFC : 0xFF0F172A;
    uint32_t txt_main = (active_theme == WM_THEME_LIGHT) ? 0xFF0F172A : 0xFFF8FAFC;
    uint32_t txt_muted = (active_theme == WM_THEME_LIGHT) ? 0xFF475569 : 0xFF94A3B8;

    // Card 1: Memoria Física PMM [REAL]
    uint32_t c1_h = 76;
    if (eco_mode) {
        gfx_fill_rounded_rect(cx, cy, cw, c1_h, 6, card_solid);
    } else {
        gfx_fill_rounded_rect_alpha(cx, cy, cw, c1_h, 6, card_bg);
    }
    gfx_draw_rounded_rect(cx, cy, cw, c1_h, 6, 0xFF334155);

    gfx_fill_circle(cx + 14, cy + 14, 3, 0xFF38BDF8);
    gfx_draw_string(cx + 24, cy + 6, "Memoria Fisica PMM [REAL]", txt_main, 0);

    char buf[32];
    gfx_draw_string(cx + 10, cy + 28, "RAM Libre :", txt_muted, 0);
    uitoa((uint32_t)pmm_get_free_memory_kib(), buf, 10);
    gfx_draw_string(cx + 105, cy + 28, buf, 0xFF38BDF8, 0);
    gfx_draw_string(cx + 170, cy + 28, "KiB", txt_muted, 0);

    // Barra de progreso visual de RAM
    uint32_t bar_w = cw - 20;
    gfx_fill_rounded_rect(cx + 10, cy + 48, bar_w, 8, 4, 0xFF1E293B);
    gfx_fill_rounded_rect(cx + 10, cy + 48, bar_w * 96 / 100, 8, 4, 0xFF38BDF8);

    gfx_draw_string(cx + 10, cy + 60, "Marcos    :", txt_muted, 0);
    uitoa((uint32_t)pmm_get_free_frames(), buf, 10);
    gfx_draw_string(cx + 105, cy + 60, buf, 0xFF38BDF8, 0);
    gfx_draw_string(cx + 170, cy + 60, "libres (4 KiB)", txt_muted, 0);

    // Card 2: Scheduler y Telemetría IoT [SIMULADO]
    uint32_t c2_y = cy + c1_h + 8;
    uint32_t c2_h = ch - c1_h - 8;
    if (eco_mode) {
        gfx_fill_rounded_rect(cx, c2_y, cw, c2_h, 6, card_solid);
    } else {
        gfx_fill_rounded_rect_alpha(cx, c2_y, cw, c2_h, 6, card_bg);
    }
    gfx_draw_rounded_rect(cx, c2_y, cw, c2_h, 6, 0xFF334155);

    gfx_fill_circle(cx + 14, c2_y + 14, 3, 0xFF10B981);
    gfx_draw_string(cx + 24, c2_y + 6, "Scheduler [REAL] & Telemetria [SIM]", txt_main, 0);

    gfx_draw_string(cx + 10, c2_y + 26, "Planificador : PIT 100 Hz (IRQ 0) [REAL]", txt_muted, 0);
    gfx_draw_string(cx + 10, c2_y + 44, "Sensor Temp  : 25.0 C [SIMULADO IoT]", 0xFF34D399, 0);
    gfx_draw_string(cx + 10, c2_y + 62, "Actuador Pin : Pin 13 ACTIVO", 0xFF60A5FA, 0);
}

/* Ventana 3: Jarvis AI Copiloto [DEMO LOCAL / SIMULADO] */
static void paint_jarvis_window(struct window* win) {
    uint32_t cx = win->x + 6;
    uint32_t cy = win->y + TITLEBAR_HEIGHT + 6;
    uint32_t cw = win->width - 12;

    uint32_t card_bg = (active_theme == WM_THEME_LIGHT) ? 0xCCF8FAFC : 0xD80F172A;
    uint32_t txt_main = (active_theme == WM_THEME_LIGHT) ? 0xFF0F172A : 0xFFF8FAFC;
    uint32_t txt_muted = (active_theme == WM_THEME_LIGHT) ? 0xFF475569 : 0xFF94A3B8;

    // 1. Burbuja de mensaje de Jarvis
    if (eco_mode) {
        gfx_fill_rounded_rect(cx, cy, cw, 42, 6, 0xFF1E293B);
    } else {
        gfx_fill_rounded_rect_alpha(cx, cy, cw, 42, 6, card_bg);
    }
    gfx_draw_rounded_rect(cx, cy, cw, 42, 6, 0xFF4338CA);

    gfx_fill_circle(cx + 14, cy + 14, 4, 0xFF818CF8);
    gfx_draw_string(cx + 24, cy + 6, "Jarvis AI Core [DEMO LOCAL - SIN RED]", 0xFF818CF8, 0);
    gfx_draw_string(cx + 10, cy + 24, "Asistente multi-agente supervisado para control de OS.", txt_muted, 0);

    // 2. Estado del Plan Activo
    uint32_t st_y = cy + 48;
    const char* state_str = "Estado: LISTO PARA INICIAR PLAN";
    uint32_t state_col = 0xFF38BDF8;
    if (jarvis_state == JARVIS_AWAITING_APPROVAL) {
        state_str = "Estado: ESPERANDO APROBACION HUMANA";
        state_col = 0xFFF59E0B;
    } else if (jarvis_state == JARVIS_COMPLETED_SIM) {
        state_str = "Estado: PLAN DEMO COMPLETADO [SIM]";
        state_col = 0xFF34D399;
    } else if (jarvis_state == JARVIS_ABORTED) {
        state_str = "Estado: PLAN ABORTADO POR USUARIO";
        state_col = 0xFFF43F5E;
    }

    gfx_fill_rounded_rect(cx, st_y, cw, 22, 4, 0xFF111827);
    gfx_draw_rounded_rect(cx, st_y, cw, 22, 4, 0xFF334155);
    gfx_draw_string(cx + 10, st_y + 4, state_str, state_col, 0);

    // 3. Pasos del Plan de Acción
    uint32_t step_y = st_y + 28;
    gfx_draw_string(cx + 8, step_y,      "1. Auditar PMM/VMM y Ring 3      [OK]", txt_main, 0);
    gfx_draw_string(cx + 8, step_y + 20, "2. Supervisar accion de agente   [GATE]", txt_main, 0);
    gfx_draw_string(cx + 8, step_y + 40, "3. Desplegar reporte local       [SIM]", txt_muted, 0);

    // 4. Botones de Acción Redondeados
    uint32_t btn_y = win->y + win->height - 30;
    gfx_fill_rounded_rect(win->x + 12, btn_y, 108, 22, 6, 0xFF4F46E5);
    gfx_draw_string(win->x + 22, btn_y + 3, "> Ejecutar", 0xFFFFFFFF, 0);

    gfx_fill_rounded_rect(win->x + 130, btn_y, 108, 22, 6, 0xFF059669);
    gfx_draw_string(win->x + 140, btn_y + 3, "# Aprobar", 0xFFFFFFFF, 0);

    gfx_fill_rounded_rect(win->x + 248, btn_y, 104, 22, 6, 0xFFE11D48);
    gfx_draw_string(win->x + 258, btn_y + 3, "x Abortar", 0xFFFFFFFF, 0);
}

/* Ventana 4: Centro de 6 Agentes Especializados [SIMULADO] */
static void paint_agents_window(struct window* win) {
    uint32_t cx = win->x + 6;
    uint32_t cy = win->y + TITLEBAR_HEIGHT + 6;
    uint32_t cw = win->width - 12;
    uint32_t ch = win->height - TITLEBAR_HEIGHT - 12;

    uint32_t txt_main = (active_theme == WM_THEME_LIGHT) ? 0xFF0F172A : 0xFFF8FAFC;

    // Título de sección
    gfx_draw_string(cx + 8, cy + 2, "6 Agentes Especializados [SIMULADO]", 0xFF38BDF8, 0);

    // Lista de agentes con tarjetas redondeadas
    uint32_t row_y = cy + 22;
    for (uint8_t i = 0; i < WM_AGENT_COUNT; i++) {
        if (i == selected_agent) {
            gfx_fill_rounded_rect(cx + 4, row_y - 2, cw - 8, 20, 5, 0xFF2563EB);
            gfx_draw_rounded_rect(cx + 4, row_y - 2, cw - 8, 20, 5, 0xFF93C5FD);
            gfx_draw_string(cx + 12, row_y, agent_names[i], 0xFFFFFFFF, 0);
        } else {
            if (eco_mode) {
                gfx_fill_rounded_rect(cx + 4, row_y - 2, cw - 8, 20, 4, 0xFF111827);
            } else {
                gfx_fill_rounded_rect_alpha(cx + 4, row_y - 2, cw - 8, 20, 4, 0x801E293B);
            }
            gfx_draw_string(cx + 12, row_y, agent_names[i], txt_main, 0);
        }
        row_y += 22;
    }

    // Tarjeta del rol seleccionado
    uint32_t role_y = cy + ch - 24;
    gfx_fill_rounded_rect(cx + 4, role_y - 2, cw - 8, 22, 4, 0xFF0B132B);
    gfx_draw_rounded_rect(cx + 4, role_y - 2, cw - 8, 22, 4, 0xFF1E293B);
    gfx_draw_string(cx + 10, role_y + 2, agent_roles[selected_agent], 0xFF34D399, 0);
}

void wm_init(void) {
    window_count = 0;
    focused_id = -1;
    uptime_ticks = 0;
    prev_left_btn = false;
    launcher_open = false;
    eco_mode = false;
    active_theme = WM_THEME_DARK;
    jarvis_state = JARVIS_IDLE;
    selected_agent = 0;
    term_hist_count = 0;
    term_input_len = 0;
    term_input[0] = '\0';

    for (size_t i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = 0;
        windows[i].is_visible = false;
    }

    struct framebuffer_info* fb = gfx_get_fb_info();
    uint32_t sw = fb && fb->width > 0 ? fb->width : 1024;
    uint32_t sh = fb && fb->height > 0 ? fb->height : 768;

    // Crear Ventana 1: Terminal Gráfica Zenthra [REAL]
    wm_create_window("Terminal Zenthra OS", 24, 44, 460, 215, paint_terminal_window);

    // Crear Ventana 2: Monitor del Sistema & IoT [REAL + SIMULADO]
    uint32_t win2_x = sw > 620 ? sw - 364 : 240;
    wm_create_window("Monitor de Sistema & IoT", win2_x, 44, 340, 180, paint_monitor_window);

    // Crear Ventana 3: Jarvis AI [DEMO LOCAL]
    uint32_t win3_y = sh > 540 ? 275 : 200;
    wm_create_window("Jarvis AI [DEMO LOCAL]", 24, win3_y, 460, 185, paint_jarvis_window);

    // Crear Ventana 4: Centro de 6 Agentes [SIM]
    uint32_t win4_y = sh > 540 ? 236 : 210;
    wm_create_window("Centro de 6 Agentes [SIM]", win2_x, win4_y, 340, 224, paint_agents_window);

    if (window_count > 0) {
        wm_focus_window(windows[0].id);
    }

    term_add_line("Bienvenido a Zenthra OS v20.0 [REAL]");
    term_add_line("Escriba 'help' para ver comandos.");

    serial_write_string("[WM] Zenthra Window Manager inicializado con 4 ventanas nativas Glassmorphism.\r\n");
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
    win->border_color = 0xFF475569;
    win->titlebar_color = 0xFF1E293B;
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
                for (int j = (int)window_count - 1; j >= 0; j--) {
                    if (windows[j].is_visible && windows[j].id != id) {
                        wm_focus_window(windows[j].id);
                        break;
                    }
                }
            }
            wm_request_redraw();
            break;
        }
    }
}

void wm_focus_window(uint32_t id) {
    focused_id = (int32_t)id;
    for (size_t i = 0; i < window_count; i++) {
        windows[i].is_focused = (windows[i].id == id);
    }
    wm_request_redraw();
}

void wm_handle_mouse(int32_t mx, int32_t my, bool left_button) {
    bool clicked = left_button && !prev_left_btn;
    bool released = !left_button && prev_left_btn;

    struct framebuffer_info* fb = gfx_get_fb_info();
    uint32_t sh = fb && fb->height > 0 ? fb->height : 768;
    uint32_t ty = sh - TASKBAR_HEIGHT;

    // 1. Manejo de arrastre activo
    for (size_t i = 0; i < window_count; i++) {
        if (windows[i].is_visible && windows[i].is_dragging) {
            if (left_button) {
                windows[i].x = mx - windows[i].drag_off_x;
                windows[i].y = my - windows[i].drag_off_y;

                if (windows[i].y < 32) windows[i].y = 32;
                if (fb && fb->height > TASKBAR_HEIGHT + TITLEBAR_HEIGHT) {
                    if ((uint32_t)windows[i].y > fb->height - TASKBAR_HEIGHT - TITLEBAR_HEIGHT) {
                        windows[i].y = (int32_t)(fb->height - TASKBAR_HEIGHT - TITLEBAR_HEIGHT);
                    }
                }
                wm_request_redraw();
            }
            if (released) {
                windows[i].is_dragging = false;
                wm_request_redraw();
            }
        }
    }

    // 2. Procesar nuevo clic
    if (clicked) {
        wm_request_redraw();

        // Comprobar clic en Menú Launcher "Zenthra" en la barra de tareas
        if (mx >= 8 && mx <= 96 && (uint32_t)my >= ty + 5 && (uint32_t)my <= ty + 31) {
            launcher_open = !launcher_open;
            prev_left_btn = left_button;
            return;
        }

        // Comprobar clic en Barra de Tareas Inferior / Dock Flotante
        if ((uint32_t)my >= ty + 4 && (uint32_t)my <= ty + 34) {
            if (mx >= 226 && mx < 310 && window_count > 0) {
                windows[0].is_visible = true;
                wm_focus_window(windows[0].id);
                prev_left_btn = left_button;
                return;
            } else if (mx >= 314 && mx < 398 && window_count > 1) {
                windows[1].is_visible = true;
                wm_focus_window(windows[1].id);
                prev_left_btn = left_button;
                return;
            } else if (mx >= 402 && mx < 486 && window_count > 2) {
                windows[2].is_visible = true;
                wm_focus_window(windows[2].id);
                prev_left_btn = left_button;
                return;
            } else if (mx >= 490 && mx < 578 && window_count > 3) {
                windows[3].is_visible = true;
                wm_focus_window(windows[3].id);
                prev_left_btn = left_button;
                return;
            } else if (mx >= 584 && mx < 656) {
                wm_toggle_eco_mode();
                prev_left_btn = left_button;
                return;
            } else if (mx >= 660 && mx < 732) {
                wm_toggle_theme();
                prev_left_btn = left_button;
                return;
            }
        }

        // Si el launcher está abierto, comprobar interacción con sus 4 elementos
        if (launcher_open) {
            int32_t ly = (int32_t)ty - 92;
            if (mx >= 8 && mx <= 200 && my >= ly && (uint32_t)my < ty) {
                if (my >= ly && my < ly + 23) {
                    if (window_count > 0) {
                        windows[0].is_visible = true;
                        wm_focus_window(windows[0].id);
                    }
                } else if (my >= ly + 23 && my < ly + 46) {
                    if (window_count > 1) {
                        windows[1].is_visible = true;
                        wm_focus_window(windows[1].id);
                    }
                } else if (my >= ly + 46 && my < ly + 69) {
                    if (window_count > 2) {
                        windows[2].is_visible = true;
                        wm_focus_window(windows[2].id);
                    }
                } else if (my >= ly + 69 && my < ly + 92) {
                    if (window_count > 3) {
                        windows[3].is_visible = true;
                        wm_focus_window(windows[3].id);
                    }
                }
                launcher_open = false;
                prev_left_btn = left_button;
                return;
            } else {
                launcher_open = false;
            }
        }

        // Hit testing en ventanas (de frente hacia atrás)
        for (int i = (int)window_count - 1; i >= 0; i--) {
            struct window* win = &windows[i];
            if (!win->is_visible) continue;

            if (mx >= win->x && mx < win->x + (int32_t)win->width &&
                my >= win->y && my < win->y + (int32_t)win->height) {

                wm_focus_window(win->id);

                // Comprobar botón de cerrar [X]: semáforo rojo en (x+8..x+20, y+4..y+20) o en extremo derecho
                bool hit_close = false;
                if (mx >= win->x + 8 && mx <= win->x + 22 && my >= win->y + 4 && my <= win->y + 22) {
                    hit_close = true;
                }
                int32_t btn_x = win->x + (int32_t)win->width - 24;
                if (mx >= btn_x && mx <= btn_x + 20 && my >= win->y + 2 && my <= win->y + 22) {
                    hit_close = true;
                }
                if (hit_close) {
                    wm_close_window(win->id);
                    break;
                }

                // Clic en la barra de título -> iniciar arrastre
                if (my < win->y + TITLEBAR_HEIGHT) {
                    win->is_dragging = true;
                    win->drag_off_x = mx - win->x;
                    win->drag_off_y = my - win->y;
                    break;
                }

                // Interacción interna con Ventana 3: Jarvis AI [DEMO LOCAL]
                if (win->id == 3) {
                    int32_t btn_y_jarvis = win->y + (int32_t)win->height - 30;
                    if (my >= btn_y_jarvis - 4 && my <= btn_y_jarvis + 24) {
                        if (mx >= win->x + 12 && mx <= win->x + 124) {
                            wm_jarvis_start_plan();
                        } else if (mx >= win->x + 128 && mx <= win->x + 242) {
                            wm_jarvis_approve_step();
                        } else if (mx >= win->x + 246 && mx <= win->x + 356) {
                            wm_jarvis_abort_plan();
                        }
                    }
                }

                // Interacción interna con Ventana 4: Centro de 6 Agentes [SIM]
                if (win->id == 4) {
                    int32_t list_top = win->y + TITLEBAR_HEIGHT + 24;
                    if (my >= list_top && my < list_top + (int32_t)(WM_AGENT_COUNT * 22)) {
                        uint8_t idx = (uint8_t)((my - list_top) / 22);
                        wm_select_agent(idx);
                    }
                }

                break;
            }
        }
    }

    prev_left_btn = left_button;
}

void wm_update_clock(uint32_t ticks) {
    uptime_ticks = ticks;
    uint32_t current_sec = ticks / 100;
    if (current_sec != last_clock_sec) {
        last_clock_sec = current_sec;
        wm_request_redraw();
    }
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

    // 1. Sombra suave de elevación 3D
    if (!eco_mode) {
        gfx_fill_rounded_rect_alpha(win->x - 4, win->y - 3, win->width + 8, win->height + 8, 14, 0x18000000);
        gfx_fill_rounded_rect_alpha(win->x - 2, win->y - 1, win->width + 4, win->height + 4, 12, 0x2A000000);
    }

    // 2. Fondo de la ventana (Frosted Glass translúcido o sólido)
    uint32_t win_glass = (active_theme == WM_THEME_LIGHT) ? 0xD8EFF6FF : 0xD80D1527;
    uint32_t win_solid = (active_theme == WM_THEME_LIGHT) ? 0xFFF8FAFC : 0xFF0D1527;
    if (eco_mode) {
        gfx_fill_rounded_rect(win->x, win->y, win->width, win->height, 10, win_solid);
    } else {
        gfx_fill_rounded_rect_alpha(win->x, win->y, win->width, win->height, 10, win_glass);
    }

    // 3. Barra de Título (Header unificado)
    uint32_t tb_glass = win->is_focused ? 0xEE1E293B : 0xCC111827;
    uint32_t tb_solid = win->is_focused ? 0xFF1E293B : 0xFF111827;
    if (eco_mode) {
        gfx_fill_rounded_rect(win->x + 1, win->y + 1, win->width - 2, TITLEBAR_HEIGHT + 2, 9, tb_solid);
    } else {
        gfx_fill_rounded_rect_alpha(win->x + 1, win->y + 1, win->width - 2, TITLEBAR_HEIGHT + 2, 9, tb_glass);
    }

    // Brillo superior specular de cristal
    if (!eco_mode) {
        gfx_fill_rect_alpha(win->x + 12, win->y + 1, win->width - 24, 1, 0x40FFFFFF);
    }

    // 4. Semáforo macOS en la esquina superior izquierda
    gfx_fill_circle(win->x + 14, win->y + 12, 5, 0xFFFF5F56); // Cerrar (Rojo)
    gfx_fill_circle(win->x + 28, win->y + 12, 5, 0xFFFFBD2E); // Minimizar (Amarillo)
    gfx_fill_circle(win->x + 42, win->y + 12, 5, 0xFF27C93F); // Maximizar (Verde)

    // Título de la ventana
    uint32_t title_col = win->is_focused ? 0xFFF8FAFC : 0xFF94A3B8;
    gfx_draw_string(win->x + 58, win->y + 5, win->title, title_col, 0);

    // Botón de cierre sutil en el extremo derecho
    int32_t btn_x = win->x + (int32_t)win->width - 20;
    gfx_draw_string(btn_x, win->y + 4, "x", 0xFF64748B, 0);

    // Borde exterior redondeado (con resplandor índigo si está enfocada)
    uint32_t border_col = win->is_focused ? 0xFF6366F1 : 0xFF334155;
    gfx_draw_rounded_rect(win->x, win->y, win->width, win->height, 10, border_col);

    // Contenido propio de la ventana
    if (win->on_paint) {
        win->on_paint(win);
    }
}

void wm_render_desktop(void) {
    if (!gfx_is_active()) return;
    if (!needs_redraw) return;

    struct framebuffer_info* fb = gfx_get_fb_info();
    uint32_t sw = fb->width;
    uint32_t sh = fb->height;

    // 1. Fondo de Pantalla: Gradiente Cósmico Profundo
    for (uint32_t y = 0; y < sh; y++) {
        uint8_t r, g, b;
        if (active_theme == WM_THEME_LIGHT) {
            r = (uint8_t)(0xE0 - (y * 0x18) / sh);
            g = (uint8_t)(0xE7 - (y * 0x12) / sh);
            b = (uint8_t)(0xFF - (y * 0x10) / sh);
        } else {
            // Gradiente cósmico profundo de #18182E a #060913
            r = (uint8_t)(0x18 - (y * 0x12) / sh);
            g = (uint8_t)(0x18 - (y * 0x0F) / sh);
            b = (uint8_t)(0x2E - (y * 0x1B) / sh);
        }
        uint32_t col = 0xFF000000 | (r << 16) | (g << 8) | b;
        gfx_fill_rect(0, y, sw, 1, col);
    }

    // Orbes de luz ambiental sutiles en el fondo para percibir el Glassmorphism
    if (!eco_mode && active_theme == WM_THEME_DARK) {
        gfx_fill_circle(180, 160, 110, 0x186366F1); // Resplandor índigo
        gfx_fill_circle(sw - 200, 320, 130, 0x140284C7); // Resplandor cian
    }

    // 2. Barra Superior (Top Menu Bar estilo macOS)
    uint32_t top_h = 28;
    if (eco_mode) {
        gfx_fill_rect(0, 0, sw, top_h, 0xFF0B0F19);
    } else {
        gfx_fill_rect_alpha(0, 0, sw, top_h, 0xD80B0F19);
        gfx_fill_rect_alpha(0, top_h - 1, sw, 1, 0x30FFFFFF);
    }

    // Logo y Nombre de Marca
    gfx_draw_string(14, 6, "ZENTHRA OS", 0xFFF8FAFC, 0);
    gfx_fill_rounded_rect(106, 5, 54, 18, 4, 0x40F59E0B);
    gfx_draw_string(112, 6, "[DEMO]", 0xFFF59E0B, 0);

    // Título de la app activa
    struct window* foc = wm_get_focused_window();
    const char* cur_title = foc ? foc->title : "Escritorio";
    gfx_draw_string(172, 6, cur_title, 0xFF94A3B8, 0);

    // Reloj central en la barra superior
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
    gfx_draw_string(sw / 2 - 20, 6, clock_str, 0xFFF8FAFC, 0);

    // Indicador de Gateway en el extremo derecho
    gfx_fill_circle(sw - 210, 14, 4, 0xFF10B981);
    gfx_draw_string(sw - 200, 6, "Gateway Local [DEMO]", 0xFF94A3B8, 0);

    // 3. Renderizar Ventanas: Primero las no enfocadas, luego la enfocada
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

    // 4. Barra de Tareas y Dock Flotante Inferior
    uint32_t ty = sh - TASKBAR_HEIGHT;

    // Fondo de la barra inferior con gradiente de cristal
    if (eco_mode) {
        gfx_fill_rect(0, ty, sw, TASKBAR_HEIGHT, 0xFF090D16);
    } else {
        gfx_fill_rect_alpha(0, ty, sw, TASKBAR_HEIGHT, 0xCC090D16);
        gfx_fill_rect_alpha(0, ty, sw, 1, 0x20FFFFFF);
    }

    // Botón Launcher "Zenthra" (preserva coordenadas para tests: x in [8, 96], y in [ty+5, ty+31])
    gfx_fill_rounded_rect(8, ty + 5, 88, 26, 6, 0xFF2563EB);
    gfx_draw_rounded_rect(8, ty + 5, 88, 26, 6, 0xFF60A5FA);
    gfx_draw_string(20, ty + 10, "Zenthra", 0xFFFFFFFF, 0);

    // Etiqueta de Perfil de Seguridad Activo
    struct task* cur_task = task_get_current();
    const char* prof_name = "Estudiante";
    if (cur_task) {
        prof_name = profile_get_name(cur_task->profile);
    }
    gfx_fill_rounded_rect(102, ty + 6, 116, 24, 6, 0xFF1E293B);
    gfx_draw_rounded_rect(102, ty + 6, 116, 24, 6, 0xFF334155);
    gfx_draw_string(108, ty + 10, prof_name, 0xFF38BDF8, 0);

    // Dock Flotante con las 4 Aplicaciones Nativas (preserva coordenadas para tests)
    const char* dock_labels[4] = { "1:Shell", "2:Mon", "3:Jarvis", "4:Agents" };
    uint32_t dock_x = 226;
    for (size_t i = 0; i < 4 && i < window_count; i++) {
        uint32_t dcol = windows[i].is_focused ? 0xFF2563EB : 0xFF1E293B;
        gfx_fill_rounded_rect(dock_x, ty + 6, 84, 24, 6, dcol);
        gfx_draw_rounded_rect(dock_x, ty + 6, 84, 24, 6, windows[i].is_focused ? 0xFF93C5FD : 0xFF475569);
        gfx_draw_string(dock_x + 8, ty + 10, dock_labels[i], 0xFFFFFFFF, 0);

        if (windows[i].is_focused) {
            gfx_fill_circle(dock_x + 42, ty + 31, 2, 0xFFFFFFFF);
        }
        dock_x += 88;
    }

    // Botones Modo Eco y Tema en la Barra Inferior
    gfx_fill_rounded_rect(584, ty + 6, 72, 24, 6, eco_mode ? 0xFF059669 : 0xFF1E293B);
    gfx_draw_rounded_rect(584, ty + 6, 72, 24, 6, 0xFF475569);
    gfx_draw_string(590, ty + 10, eco_mode ? "Eco:ON" : "Eco:OFF", 0xFFFFFFFF, 0);

    gfx_fill_rounded_rect(660, ty + 6, 72, 24, 6, active_theme == WM_THEME_LIGHT ? 0xFF2563EB : 0xFF1E293B);
    gfx_draw_rounded_rect(660, ty + 6, 72, 24, 6, 0xFF475569);
    gfx_draw_string(666, ty + 10, active_theme == WM_THEME_LIGHT ? "Tema:L" : "Tema:D", 0xFFFFFFFF, 0);

    // Reloj inferior derecho
    gfx_draw_string(sw - 56, ty + 10, clock_str, 0xFF94A3B8, 0);

    // 5. Menú Popup del Launcher (si está desplegado)
    if (launcher_open) {
        uint32_t ly = ty - 92;
        wm_draw_surface(8, ly, 192, 92, 0xF00F172A, 0xFF0F172A);
        gfx_draw_rounded_rect(8, ly, 192, 92, 6, 0xFF3B82F6);
        gfx_draw_string(14, ly + 4,  "> 1. Terminal [REAL]", 0xFFFFFFFF, 0);
        gfx_draw_string(14, ly + 27, "> 2. Monitor  [REAL/SIM]", 0xFFFFFFFF, 0);
        gfx_draw_string(14, ly + 50, "> 3. Jarvis   [DEMO]", 0xFF38BDF8, 0);
        gfx_draw_string(14, ly + 73, "> 4. Agentes  [SIM]", 0xFF34D399, 0);
    }

    // 6. Dibujar Cursor del Ratón
    mouse_draw_cursor();

    // 7. Volcar búfer de composición a pantalla
    gfx_flip();
    needs_redraw = false;
}
