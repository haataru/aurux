#ifndef MALLOC_H
#define MALLOC_H

typedef unsigned int size_t;

void* malloc(size_t size);
void free(void* ptr);

#endif
