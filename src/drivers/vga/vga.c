
#include "vga.h"
#include "../../kernel/kernel.h"

static int cursor_x = 0;
static int cursor_y = 0;
static char* vga_buffer = (char*)VGA_MEMORY;
static unsigned char current_color = 0x07;


void vga_init(void) {
    cursor_x = 0;
    cursor_y = 0;
    vga_buffer = (char*)VGA_MEMORY;
    vga_clear();
}


void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        vga_buffer[i] = ' ';
        vga_buffer[i + 1] = current_color;
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}


void vga_update_cursor(void) {
    unsigned short pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}


void vga_set_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    cursor_x = x;
    cursor_y = y;
    vga_update_cursor();
}


int vga_get_cursor_x(void) {
    return cursor_x;
}


int vga_get_cursor_y(void) {
    return cursor_y;
}


void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            int pos = (cursor_y * VGA_WIDTH + cursor_x) * 2;
            vga_buffer[pos] = ' ';
            vga_buffer[pos + 1] = current_color;
        }
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;
    } else {
        int pos = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        vga_buffer[pos] = c;
        vga_buffer[pos + 1] = current_color;
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        // Scroll up
        for (int y = 0; y < VGA_HEIGHT - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                int src = ((y + 1) * VGA_WIDTH + x) * 2;
                int dst = (y * VGA_WIDTH + x) * 2;
                vga_buffer[dst] = vga_buffer[src];
                vga_buffer[dst + 1] = vga_buffer[src + 1];
            }
        }
        // Clear last line
        for (int x = 0; x < VGA_WIDTH; x++) {
            int pos = ((VGA_HEIGHT - 1) * VGA_WIDTH + x) * 2;
            vga_buffer[pos] = ' ';
            vga_buffer[pos + 1] = current_color;
        }
        cursor_y = VGA_HEIGHT - 1;
    }
    
    vga_update_cursor();
}


void vga_print(const char* str) {
    while (*str) {
        if (*str == '\033' && *(str+1) == '[') {
            str += 2;
            int code = 0;
            while (*str >= '0' && *str <= '9') {
                code = code * 10 + (*str - '0');
                str++;
            }
            if (*str == 'm') {
                str++;
                if (code == 0) current_color = 0x07;
                else if (code >= 30 && code <= 37) {
                    unsigned char colors[] = {0, 4, 2, 6, 1, 5, 3, 7};
                    current_color = (current_color & 0xF0) | colors[code - 30];
                } else if (code >= 90 && code <= 97) {
                    unsigned char colors[] = {8, 12, 10, 14, 9, 13, 11, 15};
                    current_color = (current_color & 0xF0) | colors[code - 90];
                }
                continue;
            } else if (*str == 'J' && code == 2) {
                str++;
                vga_clear();
                continue;
            } else if (*str == 'H') {
                str++;
                vga_set_cursor(0, 0);
                continue;
            } else {
                // If it wasn't a recognized code, print the original escape characters
                vga_putchar('\033');
                vga_putchar('[');
                // Note: code characters are lost here in simple parser, but it's fine for our needs
                continue;
            }
        }
        vga_putchar(*str++);
    }
}


void vga_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    const char* p = format;
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);

                    int i = 0;
                    if (val < 0) {
                        vga_putchar('-');
                        val = -val;
                    }
                    if (val == 0) {
                        vga_putchar('0');
                    } else {
                        char tmp[32];
                        while (val > 0) {
                            tmp[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                        while (i > 0) {
                            vga_putchar(tmp[--i]);
                        }
                    }
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    vga_print(s);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    vga_putchar(c);
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char buf[16];
                    int i = 0;
                    if (val == 0) {
                        vga_print("0x0");
                    } else {
                        vga_print("0x");
                        while (val > 0) {
                            int digit = val & 0xF;
                            buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                            val >>= 4;
                        }
                        while (i > 0) {
                            vga_putchar(buf[--i]);
                        }
                    }
                    break;
                }
                default:
                    vga_putchar(*p);
                    break;
            }
        } else {
            vga_putchar(*p);
        }
        p++;
    }
    
    va_end(args);
}
