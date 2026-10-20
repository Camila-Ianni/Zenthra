#include "rules_engine.h"
#include "klib.h"
#include "vga.h"
#include "nova_iot.h"
#include "ramfs.h"

static struct automation_rule rules[MAX_RULES];

void rules_engine_init(void) {
    k_memset(rules, 0, sizeof(rules));

    // Regla predeterminada de seguridad térmica: Si Temp > 30.0 °C, encender actuador y registrar
    rules_engine_add("AlertaTermica", COND_TEMP_GREATER_THAN, 300, ACT_LED_ON);
}

int rules_engine_add(const char* name, enum rule_condition_type cond, uint16_t threshold, enum rule_action_type act) {
    if (!name) return -1;

    for (size_t i = 0; i < MAX_RULES; i++) {
        if (!rules[i].enabled) {
            rules[i].enabled = true;
            size_t len = k_strlen(name);
            if (len >= 31) len = 31;
            k_memcpy(rules[i].name, name, len);
            rules[i].name[len] = '\0';
            rules[i].condition = cond;
            rules[i].threshold_decicelsius = threshold;
            rules[i].action = act;
            rules[i].trigger_count = 0;
            return 0; // Agregada
        }
    }
    return -2; // Límite de reglas alcanzado
}

void rules_engine_evaluate(uint16_t current_temp) {
    for (size_t i = 0; i < MAX_RULES; i++) {
        if (!rules[i].enabled) continue;

        bool triggered = false;
        if (rules[i].condition == COND_TEMP_GREATER_THAN) {
            if (current_temp > rules[i].threshold_decicelsius) {
                triggered = true;
            }
        } else if (rules[i].condition == COND_TEMP_LESS_THAN) {
            if (current_temp < rules[i].threshold_decicelsius) {
                triggered = true;
            }
        }

        if (triggered) {
            rules[i].trigger_count++;
            
            // Ejecutar acción
            if (rules[i].action == ACT_LED_ON) {
                nova_iot_set_led(true);
            } else if (rules[i].action == ACT_LED_OFF) {
                nova_iot_set_led(false);
            }

            // Registrar disparo en RAMFS
            char alert_msg[64];
            k_memset(alert_msg, 0, sizeof(alert_msg));
            const char* tag = "[ALERTA REGLA: ";
            size_t idx = 0;
            for (size_t j = 0; tag[j]; j++) alert_msg[idx++] = tag[j];
            for (size_t j = 0; rules[i].name[j]; j++) alert_msg[idx++] = rules[i].name[j];
            alert_msg[idx++] = ']';
            alert_msg[idx++] = '\n';
            alert_msg[idx] = '\0';

            ramfs_create("alertas.log");
            ramfs_write("alertas.log", (const uint8_t*)alert_msg, idx);
        }
    }
}

void rules_engine_list(void) {
    vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("Reglas de Automatizacion Activas:\n");
    int count = 0;
    for (size_t i = 0; i < MAX_RULES; i++) {
        if (rules[i].enabled) {
            const char* cond_str = (rules[i].condition == COND_TEMP_GREATER_THAN) ? "Temp >" : "Temp <";
            const char* act_str = (rules[i].action == ACT_LED_ON) ? "LED ON" : "LED OFF";
            kprintf("  #%u: '%s' | SI %s %u.%u C ENTONCES %s | Disparos: %u\n",
                    i + 1, rules[i].name, cond_str, 
                    rules[i].threshold_decicelsius / 10, rules[i].threshold_decicelsius % 10,
                    act_str, rules[i].trigger_count);
            count++;
        }
    }
    if (count == 0) {
        kprintf("  (sin reglas definidas)\n");
    }
}
