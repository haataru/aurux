#ifndef LIB_H
#define LIB_H

#include "../kernel/stddef.h"

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
size_t strlen(const char* str);
size_t strnlen(const char* str, size_t maxlen);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);

/* I/O */
int printf(const char* format, ...);
int sprintf(char* buffer, const char* format, ...);
int kprintf(const char* format, ...);

/* Utils */
void hex_to_str(unsigned int val, char* buf);

#endif
