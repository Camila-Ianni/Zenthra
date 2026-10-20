#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "ktypes.h"

void keyboard_init(void);
char keyboard_getchar_nonblocking(void);
char keyboard_getchar(void);

#endif
