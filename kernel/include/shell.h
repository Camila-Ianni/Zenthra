#ifndef SHELL_H
#define SHELL_H

#include "ktypes.h"

/* 10+ Perfiles soportados en NOVA OS */
enum nova_profile {
    PROFILE_GENERAL         = 0,
    PROFILE_STUDENT         = 1,
    PROFILE_FRONTEND        = 2,
    PROFILE_BACKEND         = 3,
    PROFILE_FULLSTACK       = 4,
    PROFILE_DESIGNER        = 5,
    PROFILE_IOT_HARDWARE    = 6,
    PROFILE_DATA_SCIENTIST  = 7,
    PROFILE_CYBERSECURITY   = 8,
    PROFILE_OFFICE_ADMIN    = 9,
    PROFILE_GAMER_STREAMER  = 10,
    PROFILE_COUNT           = 11
};

void shell_init(void);
void shell_step(void);
void shell_execute(const char* cmdline);
const char* profile_get_name(enum nova_profile prof);
const char* profile_get_description(enum nova_profile prof);
enum nova_profile profile_get_active(void);
void profile_set_active(enum nova_profile prof);

#endif
