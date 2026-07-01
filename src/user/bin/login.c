#include "../../lib/lib.h"
#include "../../lib/pwd.h"
#include "../../lib/crypto.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    char username[32];
    char password[128];
    
    while(1) {
        print("\033[2J\033[H");
        print("\033[96m  __ _ _   _ _ __ _   ___  __\033[0m\n");
        print("\033[96m / _` | | | | '__| | | \\ \\/ /\033[0m\n");
        print("\033[96m| (_| | |_| | |  | |_| |>  < \033[0m\n");
        print("\033[96m \\__,_|\\__,_|_|   \\__,_/_/\\_\\\033[0m\n\n");
        print("\033[93mWelcome to Aurux OS\033[0m\n\n");
        
        print("login: ");
        int pos = 0;
        while(pos < 31) {
            char c;
            read(0, &c, 1);
            if (c == '\n') {
                username[pos] = '\0';
                print("\n");
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    print("\b \b");
                }
            } else {
                username[pos++] = c;
                char s[2] = {c, 0};
                print(s);
            }
        }
        
        print("Password: ");
        pos = 0;
        while(pos < 127) {
            char c;
            read(0, &c, 1);
            if (c == '\n') {
                password[pos] = '\0';
                print("\n");
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    // For security, don't erase on screen if we didn't print it.
                    // But if we want backspace support for passwords, we just decrement pos.
                }
            } else {
                password[pos++] = c;
                // No echo
            }
        }
        
        struct passwd pwd;
        if (getpwnam(username, &pwd) == 0) {
            if (verify_password(password, pwd.pw_passwd)) {
                // Successful login
                
                // Ensure home directory exists (auto-create if first login)
                if (chdir(pwd.pw_dir) != 0) {
                    mkdir(pwd.pw_dir);
                }
                chown(pwd.pw_dir, pwd.pw_uid, pwd.pw_gid);
                chdir(pwd.pw_dir);
                
                setuid(pwd.pw_uid);
                setgid(pwd.pw_gid);
                
                // Execute shell
                if (exec(pwd.pw_shell, "") < 0) {
                    print("login: failed to execute shell\n");
                    sleep(2000);
                }
                // If exec returns, it failed
            } else {
                print("\n\033[91mLogin incorrect\033[0m\n");
                sleep(2000);
            }
        } else {
            print("\n\033[91mLogin incorrect\033[0m\n");
            sleep(2000);
        }
    }
    return 0;
}
