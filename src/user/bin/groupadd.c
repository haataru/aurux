#include "../../lib/lib.h"
#include "../../lib/grp.h"

int main(int argc, char** argv) {
    if (getuid() != 0) {
        print("groupadd: Permission denied.\n");
        return 1;
    }
    
    if (argc < 2) {
        print("Usage: groupadd <groupname>\n");
        return 1;
    }
    
    const char* groupname = argv[1];
    
    struct group existing;
    if (getgrnam(groupname, &existing) == 0) {
        print("groupadd: group '");
        print(groupname);
        print("' already exists\n");
        return 1;
    }
    
    // Find highest gid to assign new gid
    int new_gid = 1000;
    int fd = open("/etc/groups");
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
                                    if (u >= new_gid) new_gid = u + 1;
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
    
    if (add_group(groupname, new_gid) != 0) {
        print("groupadd: failed to update /etc/groups\n");
        return 1;
    }
    
    return 0;
}
