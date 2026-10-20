#ifndef RULES_ENGINE_H
#define RULES_ENGINE_H

#include "ktypes.h"

#define MAX_RULES 8

enum rule_condition_type {
    COND_TEMP_GREATER_THAN = 1,
    COND_TEMP_LESS_THAN    = 2,
};

enum rule_action_type {
    ACT_LED_ON  = 1,
    ACT_LED_OFF = 2,
    ACT_LOG_MSG = 3,
};

struct automation_rule {
    bool enabled;
    char name[32];
    enum rule_condition_type condition;
    uint16_t threshold_decicelsius; // Ej. 300 = 30.0 °C
    enum rule_action_type action;
    uint32_t trigger_count;
};

void rules_engine_init(void);
int rules_engine_add(const char* name, enum rule_condition_type cond, uint16_t threshold, enum rule_action_type act);
void rules_engine_evaluate(uint16_t current_temp_decicelsius);
void rules_engine_list(void);

#endif
