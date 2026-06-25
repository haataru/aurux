/* TurbanOS Command Shell */
#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../lib/lib.h"

/* Shell state */
static char input_buffer[SHELL_MAX_INPUT];
static int input_pos = 0;
static char history[SHELL_HISTORY_SIZE][SHELL_MAX_INPUT];
static int history_count = 0;
static int history_index = -1;

void shell_init(void) {
    input_pos = 0;
    history_count = 0;
    history_index = -1;
}

static void add_to_history(const char* cmd) {
    if (cmd[0] == '\0') return;
    
    /* Skip duplicates */
    if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0) {
        return;
    }
    
    if (history_count < SHELL_HISTORY_SIZE) {
        strncpy(history[history_count], cmd, SHELL_MAX_INPUT - 1);
        history[history_count][SHELL_MAX_INPUT - 1] = '\0';
        history_count++;
    } else {
        /* Shift buffer */
        for (int i = 0; i < SHELL_HISTORY_SIZE - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strncpy(history[SHELL_HISTORY_SIZE - 1], cmd, SHELL_MAX_INPUT - 1);
        history[SHELL_HISTORY_SIZE - 1][SHELL_MAX_INPUT - 1] = '\0';
    }
    history_index = -1;
}

static const char* get_prev_history(void) {
    if (history_count == 0) return "";
    
    if (history_index < history_count - 1) {
        history_index++;
    }
    return history[history_count - 1 - history_index];
}

static const char* get_next_history(void) {
    if (history_count == 0) return "";
    
    if (history_index > 0) {
        history_index--;
    }
    return history[history_count - 1 - history_index];
}

static void clear_input_line(void) {
    while (input_pos > 0) {
        vga_putchar('\b');
        vga_putchar(' ');
        vga_putchar('\b');
        input_pos--;
    }
}

static void redraw_input_line(const char* buf, int cursor_pos) {
    clear_input_line();
    vga_print(buf);
    input_pos = strlen(buf);
    
    /* Position cursor */
    int current_x = vga_get_cursor_x();
    int target_x = cursor_pos;
    
    if (current_x > target_x) {
        while (current_x > target_x) {
            vga_putchar('\b');
            current_x--;
        }
    }
}

static char process_key(char c, int* cursor_pos) {
    if (c == '\n') {
        vga_putchar('\n');
        input_buffer[input_pos] = '\0';
        return 1;  /* Signal to execute */
    } else if (c == '\b') {
        if (input_pos > 0 && *cursor_pos > 0) {
            /* Remove character */
            for (int i = *cursor_pos - 1; i < input_pos - 1; i++) {
                input_buffer[i] = input_buffer[i + 1];
            }
            input_pos--;
            input_buffer[input_pos] = '\0';
            
            /* Update display */
            vga_putchar('\b');
            for (int i = *cursor_pos - 1; i < input_pos + 1; i++) {
                if (i < input_pos) {
                    vga_putchar(input_buffer[i]);
                } else {
                    vga_putchar(' ');
                }
            }
            for (int i = *cursor_pos - 1; i < input_pos + 1; i++) {
                vga_putchar('\b');
            }
            (*cursor_pos)--;
        }
    } else if (c >= 32 && c <= 126) {
        if (input_pos < SHELL_MAX_INPUT - 1) {
            /* Insert at cursor */
            for (int i = input_pos; i > *cursor_pos; i--) {
                input_buffer[i] = input_buffer[i - 1];
            }
            input_buffer[*cursor_pos] = c;
            input_pos++;
            input_buffer[input_pos] = '\0';
            
            /* Show new char */
            for (int i = *cursor_pos; i < input_pos; i++) {
                vga_putchar(input_buffer[i]);
            }
            (*cursor_pos)++;
            
            /* Return cursor */
            for (int i = *cursor_pos; i < input_pos; i++) {
                vga_putchar('\b');
            }
        }
    }
    
    return 0;
}

void shell_run(void) {
    shell_init();
    
    vga_print("turbanOS v0.4\n");
    vga_print("Type 'help' for a list of commands.\n");
    
    int cursor_pos = 0;
    
    while (1) {
        const char* cwd = fs_get_cwd();
        vga_print(cwd);
        vga_print("> ");
        
        input_pos = 0;
        cursor_pos = 0;
        history_index = -1;
        
        while (1) {
            char c = keyboard_getchar();
            
            if (c == 27) {
                char c2 = keyboard_getchar();
                if (c2 == '[') {
                    char c3 = keyboard_getchar();
                    if (c3 == 'A') {
                        const char* hist = get_prev_history();
                        redraw_input_line(hist, strlen(hist));
                        cursor_pos = input_pos;
                    } else if (c3 == 'B') {
                        const char* hist = get_next_history();
                        redraw_input_line(hist, strlen(hist));
                        cursor_pos = input_pos;
                    } else if (c3 == 'C') {
                        if (cursor_pos < input_pos) {
                            cursor_pos++;
                        }
                    } else if (c3 == 'D') {
                        if (cursor_pos > 0) {
                            cursor_pos--;
                            vga_putchar('\b');
                        }
                    } else if (c3 == 'H') {
                        while (cursor_pos > 0) {
                            cursor_pos--;
                            vga_putchar('\b');
                        }
                    } else if (c3 == 'F') {
                        while (cursor_pos < input_pos) {
                            vga_putchar(input_buffer[cursor_pos]);
                            cursor_pos++;
                        }
                    }
                }
            } else if (process_key(c, &cursor_pos)) {
                break;
            }
        }

        if (input_buffer[0] != '\0') {
            add_to_history(input_buffer);
            shell_execute(input_buffer);
        }
    }
}

int shell_execute(const char* cmd) {
    extern int commands_execute(const char* cmd);
    return commands_execute(cmd);
}
