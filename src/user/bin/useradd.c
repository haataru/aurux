#include "../../lib/lib.h"
#include "../../lib/pwd.h"
#include "../../lib/grp.h"
#include "../../lib/crypto.h"

int main(int argc, char** argv) {
    if (getuid() != 0) {
        print("useradd: Permission denied.\n");
        return 1;
    }
    
    if (argc < 2) {
        print("Usage: useradd <username>\n");
        return 1;
    }
    
    const char* username = argv[1];
    
    struct passwd existing;
    if (getpwnam(username, &existing) == 0) {
        print("useradd: user '");
        print(username);
        print("' already exists\n");
        return 1;
    }
    
    // Find highest uid to assign new uid
    int new_uid = 1000;
    int fd = open("/etc/passwd");
    if (fd >= 0) {
        char buf[1024];
        int bytes = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (bytes > 0) {
            buf[bytes] = '\0';
            char* line = buf;
            for (int i = 0; i <= bytes; i++) {
                if (buf[i] == '\n' || buf[i] == '\0') {
                    buf[i] = '\0';
                    if (*line) {
                        // Extract uid (3rd field)
                        int col = 0;
                        char* p = line;
                        while(*p) {
                            if (*p == ':') {
                                col++;
                                if (col == 2) {
                                    int u = 0;
                                    p++;
                                    while(*p >= '0' && *p <= '9') {
                                        u = u * 10 + (*p - '0');
                                        p++;
                                    }
                                    if (u >= new_uid) new_uid = u + 1;
                                    break;
                                }
                            }
                            p++;
                        }
                    }
                    if (i == bytes) break;
                    line = &buf[i+1];
                }
            }
        }
    }
    
    struct passwd pwd;
    strcpy(pwd.pw_name, username);
    pwd.pw_uid = new_uid;
    pwd.pw_gid = new_uid; // Create same group id
    
    strcpy(pwd.pw_dir, "/home/");
    strcat(pwd.pw_dir, username);
    strcpy(pwd.pw_shell, "/bin/shell.elf");
    
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
        print("useradd: passwords do not match\n");
        return 1;
    }
    
    char salt[17];
    generate_salt(salt, 16);
    hash_password(new_pass1, salt, pwd.pw_passwd);
    
    if (add_user(&pwd) != 0) {
        print("useradd: failed to update /etc/passwd\n");
        return 1;
    }
    
    if (add_group(username, new_uid) != 0) {
        print("useradd: failed to update /etc/groups\n");
        return 1;
    }
    
    // Create home directory
    mkdir("/home"); // Ignore error if exists
    mkdir(pwd.pw_dir);
    chown(pwd.pw_dir, pwd.pw_uid, pwd.pw_gid);
    
    return 0;
}
