#ifndef KERNEL_H
#define KERNEL_H

#include "stddef.h"

/* VGA */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

/* CMOS */
#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define CMOS_SECONDS 0x00
#define CMOS_MINUTES 0x02
#define CMOS_HOURS 0x04
#define CMOS_DAY 0x07
#define CMOS_MONTH 0x08
#define CMOS_YEAR 0x09
#define CMOS_STATUS_A 0x0A

/* Memory */
#define KERNEL_HEAP_START 0x100000
#define KERNEL_HEAP_SIZE (2 * 1024 * 1024)  /* 2MB for FS and heap */
#define MAX_MEMORY (16 * 1024 * 1024)       /* 16MB max */

/* Filesystem */
#define FS_MAX_FILES 64
#define FS_MAX_NAME_LENGTH 32
#define FS_MAX_FILE_SIZE 4096
#define FS_MAX_DEPTH 4

/* Shell */
#define SHELL_MAX_INPUT 256
#define SHELL_HISTORY_SIZE 20

/* I/O Ports */
static inline unsigned char inb(unsigned short port) {
    unsigned char result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(unsigned short port, unsigned char data) {
    asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline unsigned short inw(unsigned short port) {
    unsigned short result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outw(unsigned short port, unsigned short data) {
    asm volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

void OSmain(void);
void keyboard_handler_main(void);

/* stdarg */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dst, src) __builtin_va_copy(dst, src)

#endif
