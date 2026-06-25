#ifndef VGA_H
#define VGA_H

void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_printf(const char* format, ...);
void vga_update_cursor(void);
void vga_set_cursor(int x, int y);
int vga_get_cursor_x(void);
int vga_get_cursor_y(void);

#endif
