#ifndef SHELL_H
#define SHELL_H

#include "../kernel/kernel.h"

void shell_init(void);
void shell_run(void);
int shell_execute(const char* cmd);

#endif
