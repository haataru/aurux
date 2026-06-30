#ifndef MALLOC_H
#define MALLOC_H

#include "../kernel/stddef.h"

void* malloc(size_t size);
void free(void* ptr);

#endif
