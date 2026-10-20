#ifndef SHELL_H
#define SHELL_H

#include "ktypes.h"

/* Perfiles soportados en NOVA OS */
enum nova_profile {
    PROFILE_GENERAL     = 0,
    PROFILE_STUDENT     = 1,
    PROFILE_DEVELOPER   = 2,
    PROFILE_IOT_HARDWARE= 3
};

void shell_init(void);
void shell_step(void);
void shell_execute(const char* cmdline);

#endif
