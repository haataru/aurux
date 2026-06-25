#include "../lib/lib.h"

// Syscalls are now included via lib.h -> syscalls.h

static int shell_exec(char* cmd, char* arg, int in_fd, int out_fd) {
    char* redirect_out = 0;
    char* redirect_in = 0;
    
    // Process < and > (lazy simple parsing)
    for (int i = 0; arg[i]; i++) {
        if (arg[i] == '>') {
            arg[i] = '\0';
            redirect_out = arg + i + 1;
            while (*redirect_out == ' ') redirect_out++;
        } else if (arg[i] == '<') {
            arg[i] = '\0';
            redirect_in = arg + i + 1;
            while (*redirect_in == ' ') redirect_in++;
        }
    }
    
    // Clean trailing spaces in arg
    int len = strlen(arg);
    while (len > 0 && arg[len-1] == ' ') {
        arg[len-1] = '\0';
        len--;
    }

    int pid = fork();
    if (pid == 0) {
        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }
        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }
        if (redirect_in && *redirect_in) {
            int fd = open(redirect_in);
            if (fd >= 0) {
                dup2(fd, 0);
                close(fd);
            } else {
                print("shell: input file not found\n");
                exit(1);
            }
        }
        if (redirect_out && *redirect_out) {
            create_file(redirect_out);
            int fd = open(redirect_out);
            if (fd >= 0) {
                dup2(fd, 1);
                close(fd);
            } else {
                print("shell: output file error\n");
                exit(1);
            }
        }
        
        char path_buf[128];
        char* exec_path = cmd;
        
        // Simple PATH resolution
        if (cmd[0] != '/' && cmd[0] != '.') {
            strcpy(path_buf, "/bin/");
            strcat(path_buf, cmd);
            
            // Auto append .elf
            int len = strlen(path_buf);
            if (len < 4 || strcmp(path_buf + len - 4, ".elf") != 0 && strcmp(path_buf + len - 4, ".ELF") != 0) {
                strcat(path_buf, ".elf");
            }
            exec_path = path_buf;
        }
        
        // Re-construct the full arguments string for our exec format
        char full_args[256];
        strcpy(full_args, cmd);
        if (arg[0] != '\0') {
            strcat(full_args, " ");
            strcat(full_args, arg);
        }
        
        if (exec(exec_path, full_args) < 0) {
            // Try without .elf if it failed and we added it
            if (exec(cmd, full_args) < 0) {
                print("\033[91m");
                print(cmd);
                print(": command not found\033[0m\n");
                exit(1);
            }
        }
    }
    return pid;
}

int main(int argc, char** argv) {
    // Optionally use argv later if needed
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
        char *pipe_symbol = 0;
        
        // Find pipe symbol first
        for (int i = 0; input[i]; i++) {
            if (input[i] == '|') {
                input[i] = '\0';
                pipe_symbol = input + i + 1;
                while (*pipe_symbol == ' ') pipe_symbol++;
                // Clean trailing space before pipe
                int j = i - 1;
                while (j >= 0 && input[j] == ' ') {
                    input[j] = '\0';
                    j--;
                }
                break;
            }
        }
        
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
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else {
            if (pipe_symbol && *pipe_symbol) {
                int p[2];
                if (pipe(p) < 0) {
                    print("pipe failed\n");
                    continue;
                }
                
                int pid1 = shell_exec(cmd, arg, 0, p[1]);
                close(p[1]);
                
                char* cmd2 = pipe_symbol;
                char* arg2 = "";
                for (int i = 0; cmd2[i]; i++) {
                    if (cmd2[i] == ' ') {
                        cmd2[i] = '\0';
                        arg2 = cmd2 + i + 1;
                        break;
                    }
                }
                
                int pid2 = shell_exec(cmd2, arg2, p[0], 1);
                close(p[0]);
                
                wait(pid1);
                wait(pid2);
            } else {
                int pid = shell_exec(cmd, arg, 0, 1);
                if (pid > 0) {
                    wait(pid);
                } else {
                    print("fork failed\n");
                }
            }
        }
    }
    
    return 0;
}
