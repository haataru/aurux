#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned int size_t;
typedef unsigned int uintptr_t;
typedef int ptrdiff_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define NULL ((void*)0)
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
