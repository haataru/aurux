/* Shell command implementations */
#include "shell.h"
#include "../drivers/vga/vga.h"
#include "../drivers/rtc/rtc.h"
#include "../fs/fs.h"
#include "../memory/memory.h"
#include "../lib/lib.h"

#define MAX_ARGS 32

/* Environment variables */
#define MAX_ENV_VARS 16
static char env_names[MAX_ENV_VARS][32];
static char env_values[MAX_ENV_VARS][128];
static int env_count = 0;

static const char* env_get(const char* name) {
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_names[i], name) == 0) {
            return env_values[i];
        }
    }
    return NULL;
}

static void env_set(const char* name, const char* value) {
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_names[i], name) == 0) {
            strncpy(env_values[i], value, 127);
            env_values[i][127] = '\0';
            return;
        }
    }
    
    if (env_count < MAX_ENV_VARS) {
        strncpy(env_names[env_count], name, 31);
        env_names[env_count][31] = '\0';
        strncpy(env_values[env_count], value, 127);
        env_values[env_count][127] = '\0';
        env_count++;
    }
}

static void env_unset(const char* name) {
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_names[i], name) == 0) {
            for (int j = i; j < env_count - 1; j++) {
                strcpy(env_names[j], env_names[j + 1]);
                strcpy(env_values[j], env_values[j + 1]);
            }
            env_count--;
            return;
        }
    }
}

/*
 * Print help
 */
static void cmd_help(void) {
    vga_print("Available commands:\n");
    vga_print("  help   - show this help\n");
    vga_print("  clear  - clear the screen\n");
    vga_print("  fetch  - system information\n");
    vga_print("  time   - show current time\n");
    vga_print("  date   - show current date\n");
    vga_print("  reboot - reboot the system\n");
    vga_print("  ls     - list directory contents\n");
    vga_print("  cd     - change directory\n");
    vga_print("  cat    - show file contents\n");
    vga_print("  echo   - print text or write to file\n");
    vga_print("  touch  - create empty file\n");
    vga_print("  mkdir  - create directory\n");
    vga_print("  rm     - remove file or directory\n");
    vga_print("  pwd    - print working directory\n");
    vga_print("  env    - show/set environment variables\n");
    vga_print("  set    - set environment variable\n");
    vga_print("  unset  - remove environment variable\n");
    vga_print("  mem    - show memory info\n");
}

/*
 * Clear screen
 */
static void cmd_clear(void) {
    vga_clear();
}

/*
 * Show system info
 */
static void cmd_fetch(void) {
    vga_print("turbanOS v0.4\n");
    vga_print("author: haataru (github.com/haataru)\n");
    vga_print("VGA: 80x25 text mode\n");
    vga_print("Memory: ");
    
    char buf[32];
    sprintf(buf, "%d", (int)(memory_get_total() / 1024));
    vga_print(buf);
    vga_print("KB total, ");
    
    sprintf(buf, "%d", (int)(memory_get_free() / 1024));
    vga_print(buf);
    vga_print(" KB free\n");
}

/*
 * Show time
 */
static void cmd_time(void) {
    int h, m, s;
    rtc_getTime(&h, &m, &s);
    
    vga_putchar((h / 10) + '0');
    vga_putchar((h % 10) + '0');
    vga_putchar(':');
    vga_putchar((m / 10) + '0');
    vga_putchar((m % 10) + '0');
    vga_putchar(':');
    vga_putchar((s / 10) + '0');
    vga_putchar((s % 10) + '0');
    vga_print(" MSK\n");
}

/*
 * Show date
 */
static void cmd_date(void) {
    int d, mo, y;
    rtc_getDate(&d, &mo, &y);
    
    vga_putchar((d / 10) + '0');
    vga_putchar((d % 10) + '0');
    vga_putchar('.');
    vga_putchar((mo / 10) + '0');
    vga_putchar((mo % 10) + '0');
    vga_print(".20");
    vga_putchar((y / 10) + '0');
    vga_putchar((y % 10) + '0');
    vga_print("\n");
}

/*
 * Reboot
 */
static void cmd_reboot(void) {
    vga_print("Rebooting system...\n");
    
    for (volatile int i = 0; i < 500000; i++);
    
    unsigned char temp;
    int attempts = 0;
    const int max_attempts = 1000;
    
    do {
        temp = inb(0x64);
        if ((temp & 0x02) == 0) break;
        attempts++;
        for (volatile int j = 0; j < 1000; j++);
    } while (attempts < max_attempts);
    
    if (attempts >= max_attempts) {
        vga_print("Keyboard controller not responding. Trying alternative methods...\n");
        
        outb(0x604, 0x2000);
        for (volatile int i = 0; i < 100000; i++);
        
        outb(0xB004, 0x2000);
        for (volatile int i = 0; i < 100000; i++);
        
        outb(0xCF9, 0x0E);
        for (volatile int i = 0; i < 100000; i++);
        
        vga_print("Forcing triple fault...\n");
        asm volatile("cli");
        asm volatile("lidt 0");
        asm volatile("int $0x00");
    } else {
        outb(0x64, 0xFE);
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    vga_print("Reboot failed! System error.\n");
    while (1) {
        asm volatile("hlt");
    }
}

/*
 * List directory
 */
static void cmd_ls(const char* path) {
    char output[512];
    if (fs_list(path, output, sizeof(output)) < 0) {
        vga_print("ls: cannot access '");
        vga_print(path);
        vga_print("': No such file or directory\n");
        return;
    }
    
    if (output[0] == '\0') {
        /* Empty directory */
        return;
    }
    
    vga_print(output);
    vga_print("\n");
}

/*
 * Change directory
 */
static void cmd_cd(const char* path) {
    if (fs_change_dir(path) < 0) {
        vga_print("cd: no such directory: ");
        vga_print(path);
        vga_print("\n");
    }
}

/*
 * Show file contents
 */
static void cmd_cat(const char* path) {
    char buf[FS_MAX_FILE_SIZE + 1];
    int len = fs_read(path, buf, FS_MAX_FILE_SIZE);
    
    if (len < 0) {
        vga_print("cat: ");
        vga_print(path);
        vga_print(": No such file\n");
        return;
    }
    
    buf[len] = '\0';
    vga_print(buf);
    if (len > 0 && buf[len - 1] != '\n') {
        vga_print("\n");
    }
}

/*
 * Echo or write to file
 */
static void cmd_echo(const char* text, int append, const char* file) {
    if (file == NULL) {
        /* Just print text */
        vga_print(text);
        vga_print("\n");
    } else {
        /* Write to file */
        if (!append) {
            /* Create new file or overwrite */
            if (fs_exists(file)) {
                fs_delete(file);
            }
        }
        
        if (fs_create(file, FS_TYPE_FILE) < 0 && !fs_exists(file)) {
            vga_print("echo: cannot create file '");
            vga_print(file);
            vga_print("'\n");
            return;
        }
        
        fs_write(file, text, strlen(text));
    }
}

/*
 * Create empty file
 */
static void cmd_touch(const char* path) {
    if (fs_exists(path)) {
        return;  /* File already exists */
    }
    
    if (fs_create(path, FS_TYPE_FILE) < 0) {
        vga_print("touch: cannot create '");
        vga_print(path);
        vga_print("'\n");
    }
}

/*
 * Create directory
 */
static void cmd_mkdir(const char* path) {
    if (fs_create(path, FS_TYPE_DIRECTORY) < 0) {
        vga_print("mkdir: cannot create directory '");
        vga_print(path);
        vga_print("'\n");
    }
}

/*
 * Remove file or directory
 */
static void cmd_rm(const char* path) {
    if (fs_delete(path) < 0) {
        vga_print("rm: cannot remove '");
        vga_print(path);
        vga_print("': No such file or directory\n");
    }
}

/*
 * Print working directory
 */
static void cmd_pwd(void) {
    const char* cwd = fs_get_cwd();
    vga_print(cwd);
    vga_print("\n");
}

/*
 * Show/set environment variables
 */
static void cmd_env(const char* name, const char* value) {
    if (name == NULL) {
        /* Show all */
        for (int i = 0; i < env_count; i++) {
            vga_print(env_names[i]);
            vga_print("=");
            vga_print(env_values[i]);
            vga_print("\n");
        }
    } else if (value == NULL) {
        /* Show one */
        const char* val = env_get(name);
        if (val) {
            vga_print(name);
            vga_print("=");
            vga_print(val);
            vga_print("\n");
        }
    } else {
        /* Set */
        env_set(name, value);
    }
}

/*
 * Set environment variable
 */
static void cmd_set(const char* name, const char* value) {
    if (name == NULL || value == NULL) {
        vga_print("set: usage: set NAME VALUE\n");
        return;
    }
    env_set(name, value);
}

/*
 * Unset environment variable
 */
static void cmd_unset(const char* name) {
    if (name == NULL) {
        vga_print("unset: usage: unset NAME\n");
        return;
    }
    env_unset(name);
}

/*
 * Show memory info
 */
static void cmd_mem(void) {
    char buf[32];
    
    vga_print("Memory Info:\n");
    vga_print("  Total: ");
    sprintf(buf, "%d", (int)(memory_get_total() / 1024));
    vga_print(buf);
    vga_print(" KB\n");
    
    vga_print("  Used: ");
    sprintf(buf, "%d", (int)(memory_get_used() / 1024));
    vga_print(buf);
    vga_print(" KB\n");
    
    vga_print("  Free: ");
    sprintf(buf, "%d", (int)(memory_get_free() / 1024));
    vga_print(buf);
    vga_print(" KB\n");
}

/*
 * Parse command line and execute
 */
int commands_execute(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return 0;
    }
    
    /* Copy command to parse */
    char buffer[SHELL_MAX_INPUT];
    strncpy(buffer, cmd, SHELL_MAX_INPUT - 1);
    buffer[SHELL_MAX_INPUT - 1] = '\0';
    
    /* Parse arguments */
    char* args[MAX_ARGS];
    int argc = 0;
    
    char* token = buffer;
    int in_quote = 0;
    char quote_char = 0;
    
    while (*token && argc < MAX_ARGS - 1) {
        /* Skip whitespace */
        while (*token == ' ' || *token == '\t') token++;
        if (*token == '\0') break;
        
        /* Check for quotes */
        if (*token == '"' || *token == '\'') {
            quote_char = *token;
            in_quote = 1;
            token++;
        }
        
        /* Find end of token */
        args[argc] = token;
        while (*token) {
            if (in_quote && *token == quote_char) {
                in_quote = 0;
                *token = '\0';
                token++;
                break;
            } else if (!in_quote && (*token == ' ' || *token == '\t')) {
                *token = '\0';
                token++;
                break;
            }
            token++;
        }
        
        if (argc > 0 || args[0][0] != '\0') {
            argc++;
        }
    }
    
    args[argc] = NULL;
    
    if (argc == 0) {
        return 0;
    }
    
    /* Handle redirect */
    char* output_file = NULL;
    int append = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (i + 1 < argc) {
                output_file = args[i + 1];
                append = 0;
                args[i] = NULL;
                args[i + 1] = NULL;
                break;
            }
        } else if (strcmp(args[i], ">>") == 0) {
            if (i + 1 < argc) {
                output_file = args[i + 1];
                append = 1;
                args[i] = NULL;
                args[i + 1] = NULL;
                break;
            }
        }
    }
    
    /* Execute command */
    const char* cmd_name = args[0];
    
    if (strcmp(cmd_name, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd_name, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(cmd_name, "fetch") == 0) {
        cmd_fetch();
    } else if (strcmp(cmd_name, "time") == 0) {
        cmd_time();
    } else if (strcmp(cmd_name, "date") == 0) {
        cmd_date();
    } else if (strcmp(cmd_name, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(cmd_name, "ls") == 0) {
        if (argc > 1 && args[1]) {
            cmd_ls(args[1]);
        } else {
            cmd_ls(".");
        }
    } else if (strcmp(cmd_name, "cd") == 0) {
        if (argc > 1 && args[1]) {
            cmd_cd(args[1]);
        } else {
            cmd_cd("/");
        }
    } else if (strcmp(cmd_name, "cat") == 0) {
        if (argc > 1 && args[1]) {
            cmd_cat(args[1]);
        } else {
            vga_print("cat: missing operand\n");
        }
    } else if (strcmp(cmd_name, "echo") == 0) {
        /* Build text from remaining args */
        char text[256];
        text[0] = '\0';
        
        for (int i = 1; i < argc; i++) {
            if (args[i] == NULL) continue;
            if (i > 1 && text[0] != '\0') {
                strcat(text, " ");
            }
            strcat(text, args[i]);
        }
        
        cmd_echo(text, append, output_file);
    } else if (strcmp(cmd_name, "touch") == 0) {
        if (argc > 1 && args[1]) {
            cmd_touch(args[1]);
        } else {
            vga_print("touch: missing operand\n");
        }
    } else if (strcmp(cmd_name, "mkdir") == 0) {
        if (argc > 1 && args[1]) {
            cmd_mkdir(args[1]);
        } else {
            vga_print("mkdir: missing operand\n");
        }
    } else if (strcmp(cmd_name, "rm") == 0) {
        if (argc > 1 && args[1]) {
            cmd_rm(args[1]);
        } else {
            vga_print("rm: missing operand\n");
        }
    } else if (strcmp(cmd_name, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(cmd_name, "env") == 0) {
        if (argc > 1 && args[1]) {
            if (argc > 2 && args[2]) {
                cmd_env(args[1], args[2]);
            } else {
                cmd_env(args[1], NULL);
            }
        } else {
            cmd_env(NULL, NULL);
        }
    } else if (strcmp(cmd_name, "set") == 0) {
        if (argc > 2 && args[1] && args[2]) {
            cmd_set(args[1], args[2]);
        } else {
            vga_print("set: usage: set NAME VALUE\n");
        }
    } else if (strcmp(cmd_name, "unset") == 0) {
        if (argc > 1 && args[1]) {
            cmd_unset(args[1]);
        } else {
            vga_print("unset: usage: unset NAME\n");
        }
    } else if (strcmp(cmd_name, "mem") == 0) {
        cmd_mem();
    } else {
        vga_print("Unknown command: '");
        vga_print(cmd_name);
        vga_print("'. Type 'help' for a list of commands.\n");
    }
    
    return 0;
}
