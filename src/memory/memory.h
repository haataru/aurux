#ifndef MEMORY_H
#define MEMORY_H

#include "../kernel/kernel.h"

void memory_init(struct multiboot_info* mbi);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);

void* kmalloc(size_t size);
void kfree(void* ptr);

size_t memory_get_total(void);
size_t memory_get_used(void);
size_t memory_get_free(void);

#endif
