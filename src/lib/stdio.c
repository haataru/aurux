/*
 * stdio.c - Standard I/O functions
 */

#include "lib.h"
#include <stdarg.h>

/* VGA output function (defined elsewhere) */
extern void vga_putchar(char c);
extern void vga_print(const char* str);

/*
 * Simple printf - supports %d, %s, %c, %x, %%
 */
int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    const char* p = format;
    int count = 0;
    
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    char buf[32];
                    int i = 0;
                    if (val < 0) {
                        vga_putchar('-');
                        count++;
                        val = -val;
                    }
                    if (val == 0) {
                        vga_putchar('0');
                        count++;
                    } else {
                        char tmp[32];
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            vga_putchar(tmp[--i]);
                            count++;
                        }
                    }
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[32];
                    int i = 0;
                    if (val == 0) {
                        vga_putchar('0');
                        count++;
                    } else {
                        char tmp[32];
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            vga_putchar(tmp[--i]);
                            count++;
                        }
                    }
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (s) {
                        vga_print(s);
                        while (*s) count++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    vga_putchar(c);
                    count++;
                    break;
                }
                case 'x':
                case 'p': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[16];
                    int i = 0;
                    if (val == 0) {
                        vga_print("0x0");
                        count += 3;
                    } else {
                        vga_print("0x");
                        count += 2;
                        while (val > 0) {
                            int digit = val & 0xF;
                            buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                            val >>= 4;
                        }
                        while (i > 0) {
                            vga_putchar(buf[--i]);
                            count++;
                        }
                    }
                    break;
                }
                case 'X': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[16];
                    int i = 0;
                    if (val == 0) {
                        vga_print("0x0");
                        count += 3;
                    } else {
                        vga_print("0x");
                        count += 2;
                        while (val > 0) {
                            int digit = val & 0xF;
                            buf[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
                            val >>= 4;
                        }
                        while (i > 0) {
                            vga_putchar(buf[--i]);
                            count++;
                        }
                    }
                    break;
                }
                case '%': {
                    vga_putchar('%');
                    count++;
                    break;
                }
                default:
                    vga_putchar(*p);
                    count++;
                    break;
            }
        } else {
            vga_putchar(*p);
            count++;
        }
        p++;
    }
    
    va_end(args);
    return count;
}

/*
 * sprintf - write formatted string to buffer
 */
int sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    const char* p = format;
    int count = 0;
    
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    if (val < 0) {
                        buffer[count++] = '-';
                        val = -val;
                    }
                    if (val == 0) {
                        buffer[count++] = '0';
                    } else {
                        char tmp[32];
                        int i = 0;
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            buffer[count++] = tmp[--i];
                        }
                    }
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    if (val == 0) {
                        buffer[count++] = '0';
                    } else {
                        char tmp[32];
                        int i = 0;
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            buffer[count++] = tmp[--i];
                        }
                    }
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (s) {
                        while (*s) {
                            buffer[count++] = *s++;
                        }
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    buffer[count++] = c;
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[16];
                    int i = 0;
                    if (val == 0) {
                        buffer[count++] = '0';
                    } else {
                        while (val > 0) {
                            int digit = val & 0xF;
                            buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                            val >>= 4;
                        }
                        while (i > 0) {
                            buffer[count++] = buf[--i];
                        }
                    }
                    break;
                }
                case 'X': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[16];
                    int i = 0;
                    if (val == 0) {
                        buffer[count++] = '0';
                    } else {
                        while (val > 0) {
                            int digit = val & 0xF;
                            buf[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
                            val >>= 4;
                        }
                        while (i > 0) {
                            buffer[count++] = buf[--i];
                        }
                    }
                    break;
                }
                case '%': {
                    buffer[count++] = '%';
                    break;
                }
                default:
                    buffer[count++] = *p;
                    break;
            }
        } else {
            buffer[count++] = *p;
        }
        p++;
    }
    
    buffer[count] = '\0';
    va_end(args);
    return count;
}

/*
 * kprintf - kernel printf (alias for printf)
 */
int kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    const char* p = format;
    int count = 0;
    
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    char buf[32];
                    int i = 0;
                    if (val < 0) {
                        vga_putchar('-');
                        count++;
                        val = -val;
                    }
                    if (val == 0) {
                        vga_putchar('0');
                        count++;
                    } else {
                        char tmp[32];
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            vga_putchar(tmp[--i]);
                            count++;
                        }
                    }
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (s) {
                        vga_print(s);
                        while (*s) count++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    vga_putchar(c);
                    count++;
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[16];
                    int i = 0;
                    if (val == 0) {
                        vga_print("0x0");
                        count += 3;
                    } else {
                        vga_print("0x");
                        count += 2;
                        while (val > 0) {
                            int digit = val & 0xF;
                            buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                            val >>= 4;
                        }
                        while (i > 0) {
                            vga_putchar(buf[--i]);
                            count++;
                        }
                    }
                    break;
                }
                case '%': {
                    vga_putchar('%');
                    count++;
                    break;
                }
                default:
                    vga_putchar(*p);
                    count++;
                    break;
            }
        } else {
            vga_putchar(*p);
            count++;
        }
        p++;
    }
    
    va_end(args);
    return count;
}
