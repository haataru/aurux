#ifndef PIT_H
#define PIT_H

#include "../../kernel/kernel.h"

void pit_init(unsigned int frequency);
unsigned int get_ticks(void);

#endif
