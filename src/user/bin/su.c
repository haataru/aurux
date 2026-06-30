#include "../../lib/lib.h"
#include "../../lib/pwd.h"
#include "../../lib/crypto.h"

int main(int argc, char** argv) {
    if (argc > 2) {
        print("Usage: su [user]\n");
        return 1;
    }
    
    const char* target_user = (argc == 2) ? argv[1] : "root";
    struct passwd pwd;
    
    if (getpwnam(target_user, &pwd) != 0) {
        print("su: user ");
        print(target_user);
        print(" does not exist\n");
        return 1;
    }
    
    // Only ask password if not currently root
    if (getuid() != 0) {
        print("Password: ");
        char password[128];
        int pos = 0;
        while(pos < 127) {
            char c;
            read(0, &c, 1);
            if (c == '\n') {
                password[pos] = '\0';
                print("\n");
                break;
            } else if (c == '\b') {
                if (pos > 0) pos--;
            } else {
                password[pos++] = c;
            }
        }
        
        if (!verify_password(password, pwd.pw_passwd)) {
            print("su: Authentication failure\n");
            return 1;
        }
    }
    
    setuid(pwd.pw_uid);
    setgid(pwd.pw_gid);
    chdir(pwd.pw_dir);
    
    exec(pwd.pw_shell, "");
    
    print("su: failed to execute shell\n");
    return 1;
}
