#include "../../lib/lib.h"
#include "../../lib/pwd.h"
#include "../../lib/crypto.h"

int main(int argc, char** argv) {
    const char* target_user;
    if (argc > 1) {
        if (getuid() != 0) {
            print("passwd: You may not change the password for other users.\n");
            return 1;
        }
        target_user = argv[1];
    } else {
        struct passwd current_pwd;
        if (getpwuid(getuid(), &current_pwd) != 0) {
            print("passwd: You don't exist!\n");
            return 1;
        }
        target_user = current_pwd.pw_name;
    }
    
    struct passwd pwd;
    if (getpwnam(target_user, &pwd) != 0) {
        print("passwd: User not found.\n");
        return 1;
    }
    
    print("New password: ");
    char new_pass1[128];
    int pos = 0;
    while(pos < 127) {
        char c;
        read(0, &c, 1);
        if (c == '\n') {
            new_pass1[pos] = '\0';
            print("\n");
            break;
        } else if (c == '\b') {
            if (pos > 0) pos--;
        } else {
            new_pass1[pos++] = c;
        }
    }
    
    print("Retype new password: ");
    char new_pass2[128];
    pos = 0;
    while(pos < 127) {
        char c;
        read(0, &c, 1);
        if (c == '\n') {
            new_pass2[pos] = '\0';
            print("\n");
            break;
        } else if (c == '\b') {
            if (pos > 0) pos--;
        } else {
            new_pass2[pos++] = c;
        }
    }
    
    if (strcmp(new_pass1, new_pass2) != 0) {
        print("passwd: passwords do not match\n");
        return 1;
    }
    
    char salt[17];
    generate_salt(salt, 16);
    
    char hash[100];
    hash_password(new_pass1, salt, hash);
    
    if (update_password(target_user, hash) != 0) {
        print("passwd: error updating password file\n");
        return 1;
    }
    
    print("passwd: password updated successfully\n");
    return 0;
}
