#include "../lib/lib.h"

// User-space system call wrapper functions
static void print(const char* str) {
    asm volatile(
        "int $0x80"
        :: "a"(1), "b"(str) : "memory"
    );
}

static void exit(int status) {
    asm volatile(
        "int $0x80"
        :: "a"(2), "b"(status) : "memory"
    );
}

static int read(int fd, char* buf, int count) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret) : "a"(3), "b"(fd), "c"(buf), "d"(count) : "memory"
    );
    return ret;
}

static int fork(void) {
    int pid;
    asm volatile(
        "int $0x80"
        : "=a"(pid) : "a"(18) : "memory"
    );
    return pid;
}

static int exec(const char* path) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret) : "a"(19), "b"(path) : "memory"
    );
    return ret;
}

static int spawn(const char* path) {
    int pid = fork();
    if (pid == 0) {
        exec(path);
        exit(1);
    }
    return pid;
}

static int wait(int pid) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(5), "b"(pid) : "memory");
    return ret;
}

static int open(const char* path) {
    int fd;
    asm volatile("int $0x80" : "=a"(fd) : "a"(6), "b"(path) : "memory");
    return fd;
}

static int create_file(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(15), "b"(path) : "memory");
    return ret;
}

static int write(int fd, const char* data, int size) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(7), "b"(fd), "c"(data), "d"(size) : "memory");
    return ret;
}

static int close(int fd) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(8), "b"(fd) : "memory");
    return ret;
}

static int mkdir(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(10), "b"(path) : "memory");
    return ret;
}

static int listdir(const char* path, char* buf, int size, int detailed) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(11), "b"(path), "c"(buf), "d"(size), "S"(detailed) : "memory");
    return ret;
}

static int unlink(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(12), "b"(path) : "memory");
    return ret;
}

static int chdir(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(13), "b"(path) : "memory");
    return ret;
}

static int getcwd(char* buf, int size) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(14), "b"(buf), "c"(size) : "memory");
    return ret;
}

static int gettime(int* time_arr) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(16), "b"(time_arr) : "memory");
    return ret;
}

void _start() {
    print("\033[2J\033[H");
    print("\033[96m  __ _ _   _ _ __ _   ___  __\033[0m\n");
    print("\033[96m / _` | | | | '__| | | \\ \\/ /\033[0m\n");
    print("\033[96m| (_| | |_| | |  | |_| |>  < \033[0m\n");
    print("\033[96m \\__,_|\\__,_|_|   \\__,_/_/\\_\\\033[0m\n\n");
    
    int time_arr[6]; // Array to store year, month, day, hour, minute, second
    if (gettime(time_arr) == 0) {
        char y_str[8], m_str[4], d_str[4], h_str[4], min_str[4], s_str[4];
        
        int ty = time_arr[0], i = 3; y_str[4] = 0;
        while(i >= 0) { y_str[i--] = '0' + (ty % 10); ty /= 10; }
        
        m_str[0] = '0' + (time_arr[1] / 10); m_str[1] = '0' + (time_arr[1] % 10); m_str[2] = 0;
        d_str[0] = '0' + (time_arr[2] / 10); d_str[1] = '0' + (time_arr[2] % 10); d_str[2] = 0;
        h_str[0] = '0' + (time_arr[3] / 10); h_str[1] = '0' + (time_arr[3] % 10); h_str[2] = 0;
        min_str[0] = '0' + (time_arr[4] / 10); min_str[1] = '0' + (time_arr[4] % 10); min_str[2] = 0;
        s_str[0] = '0' + (time_arr[5] / 10); s_str[1] = '0' + (time_arr[5] % 10); s_str[2] = 0;

        print("\033[95m");
        print("Date: "); print(y_str); print("-"); print(m_str); print("-"); print(d_str);
        print(" Time: "); print(h_str); print(":"); print(min_str); print(":"); print(s_str);
        print("\n\n");
        print("\033[0m");
    }
    
    print("Type \033[93m'help'\033[0m for a list of commands.\n\n");
    
    char input[256];
    char cwd[256];
    
    while (1) {
        if (getcwd(cwd, 256) != 0) {
            cwd[0] = '/';
            cwd[1] = '\0';
        }
        
        print("\033[92muser@aurux\033[0m:\033[94m");
        print(cwd);
        print("\033[0m$ ");
        
        int pos = 0;
        while (pos < 255) {
            char c;
            read(0, &c, 1);
            
            if (c == '\n') {
                print("\n");
                input[pos] = '\0';
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    print("\b \b");
                }
            } else {
                input[pos++] = c;
                char s[2] = {c, 0};
                print(s);
            }
        }
        
        if (pos == 0) continue;
        
        char *cmd = input;
        char *arg = "";
        for (int i = 0; input[i]; i++) {
            if (input[i] == ' ') {
                input[i] = '\0';
                arg = input + i + 1;
                break;
            }
        }
        
        if (strcmp(cmd, "help") == 0) {
            print("\033[93mBuilt-in Commands:\033[0m\n");
            print("Available commands:\n");
            print("  help      - Show this message\n");
            print("  clear     - Clear the screen\n");
            print("  pwd       - Print working directory\n");
            print("  cd [dir]  - Change directory\n");
            print("  ls [dir]  - List directory contents\n");
            print("  mkdir [d] - Create directory\n");
            print("  rm [file] - Remove file or directory\n");
            print("  cat [file]- View file contents\n");
            print("  echo      - Echo text or write to file (e.g., echo hi > file.txt)\n");
            print("  exit      - Exit shell\n");
        } else if (strcmp(cmd, "clear") == 0) {
            print("\033[2J\033[H");
        } else if (strcmp(cmd, "exec") == 0) {
            int pid = spawn(arg);
            if (pid > 0) {
                print("\033[90m[Shell] Spawned process. Waiting...\033[0m\n");
                wait(pid);
                print("\033[90m[Shell] Process finished.\033[0m\n");
            } else {
                print("\033[91m[Shell] Failed to spawn process.\033[0m\n");
            }
        } else if (strcmp(cmd, "ls") == 0) {
            char buf[1024];
            int detailed = 0;
            char* path = arg;
            if (arg[0] == '-' && arg[1] == 'l' && (arg[2] == ' ' || arg[2] == '\0')) {
                detailed = 1;
                path = arg + 2;
                while (*path == ' ') path++;
            }
            int res = listdir(path[0] == '\0' ? "." : path, buf, 1024, detailed);
            if (res >= 0 && buf[0] != '\0') {
                print(buf); print("\n");
            } else if (res < 0) {
                print("ls: cannot open directory\n");
            }
        } else if (strcmp(cmd, "cat") == 0) {
            int fd = open(arg);
            if (fd >= 0) {
                char buf[513];
                int n = read(fd, buf, 512);
                if (n > 0) {
                    buf[n] = 0;
                    print(buf); print("\n");
                }
                close(fd);
            } else {
                print("cat: file not found\n");
            }
        } else if (strcmp(cmd, "pwd") == 0) {
            char buf[256];
            if (getcwd(buf, 256) == 0) {
                print(buf);
                print("\n");
            } else {
                print("pwd failed\n");
            }
        } else if (strcmp(cmd, "cd") == 0) {
            if (arg[0] == '\0') {
                print("cd: missing argument\n");
            } else {
                if (chdir(arg) != 0) {
                    print("cd: ");
                    print(arg);
                    print(": No such file or directory\n");
                }
            }
        } else if (strcmp(cmd, "mkdir") == 0) {
            int res = mkdir(arg);
            if (res < 0) print("mkdir: failed\n");
        } else if (strcmp(cmd, "rm") == 0) {
            int res = unlink(arg);
            if (res < 0) print("rm: failed\n");
        } else if (strcmp(cmd, "echo") == 0) {
            char* text = arg;
            char* redirect = 0;
            for (int i = 0; text[i]; i++) {
                if (text[i] == '>') {
                    text[i] = 0;
                    redirect = text + i + 1;
                    while (*redirect == ' ') redirect++;
                    break;
                }
            }
            if (redirect && *redirect) {
                create_file(redirect);
                int fd = open(redirect);
                if (fd >= 0) {
                    write(fd, text, strlen(text));
                    close(fd);
                } else {
                    print("echo: cannot open file for writing\n");
                }
            } else {
                print(text); print("\n");
            }
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else {
            print("\033[91mUnknown command: \033[0m");
            print(input);
            print("\n");
        }
    }
    
    exit(0);
}
