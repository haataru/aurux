#ifndef MEMORY_H
#define MEMORY_H

#include "../kernel/stddef.h"

void memory_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
size_t memory_get_total(void);
size_t memory_get_used(void);
size_t memory_get_free(void);

#endif
