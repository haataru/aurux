#include "grp.h"
#include "lib.h"
#include "syscalls.h"
#define PATH_GROUP "/etc/groups"

// We use static buffers for group members parsing
static char grp_mem_buf[1024];

static void int_to_str(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int i = 0;
    char temp[16];
    while(val > 0) {
        temp[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = 0; j < i; j++) {
        buf[j] = temp[i - 1 - j];
    }
    buf[i] = '\0';
}

static int parse_group_line(const char* line, struct group* grp) {
    char buf[1024];
    strncpy(buf, line, sizeof(buf));
    buf[1023] = '\0';
    
    char* tokens[4];
    int t = 0;
    tokens[t++] = buf;
    
    for (int i = 0; buf[i]; i++) {
        if (buf[i] == ':') {
            buf[i] = '\0';
            if (t < 4) tokens[t++] = &buf[i+1];
        }
    }
    
    if (t < 3) return -1;
    
    strcpy(grp->gr_name, tokens[0]);
    strcpy(grp->gr_passwd, tokens[1]);
    
    int gid = 0;
    for (int i = 0; tokens[2][i] >= '0' && tokens[2][i] <= '9'; i++) {
        gid = gid * 10 + (tokens[2][i] - '0');
    }
    grp->gr_gid = gid;
    
    // Parse members
    int m = 0;
    if (t == 4 && tokens[3][0] != '\0') {
        char* mems = tokens[3];
        strncpy(grp_mem_buf, mems, sizeof(grp_mem_buf)-1);
        grp_mem_buf[sizeof(grp_mem_buf)-1] = '\0';
        
        char* p = grp_mem_buf;
        grp->gr_mem[m++] = p;
        for (int i = 0; grp_mem_buf[i]; i++) {
            if (grp_mem_buf[i] == ',') {
                grp_mem_buf[i] = '\0';
                if (m < 31) grp->gr_mem[m++] = &grp_mem_buf[i+1];
            }
        }
    }
    grp->gr_mem[m] = NULL;
    
    return 0;
}

int getgrgid(int gid, struct group* grp) {
    int fd = open(PATH_GROUP);
    if (fd < 0) return -1;
    
    char buf[2048];
    int bytes = read(fd, buf, sizeof(buf)-1);
    close(fd);
    
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    char* line = buf;
    for (int i = 0; i < bytes; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            struct group tmp;
            if (parse_group_line(line, &tmp) == 0) {
                if (tmp.gr_gid == gid) {
                    *grp = tmp;
                    return 0;
                }
            }
            line = &buf[i+1];
        }
    }
    if (*line) {
        struct group tmp;
        if (parse_group_line(line, &tmp) == 0) {
            if (tmp.gr_gid == gid) {
                *grp = tmp;
                return 0;
            }
        }
    }
    return -1;
}

int getgrnam(const char* name, struct group* grp) {
    int fd = open(PATH_GROUP);
    if (fd < 0) return -1;
    
    char buf[2048];
    int bytes = read(fd, buf, sizeof(buf)-1);
    close(fd);
    
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    char* line = buf;
    for (int i = 0; i < bytes; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            struct group tmp;
            if (parse_group_line(line, &tmp) == 0) {
                if (strcmp(tmp.gr_name, name) == 0) {
                    *grp = tmp;
                    return 0;
                }
            }
            line = &buf[i+1];
        }
    }
    if (*line) {
        struct group tmp;
        if (parse_group_line(line, &tmp) == 0) {
            if (strcmp(tmp.gr_name, name) == 0) {
                *grp = tmp;
                return 0;
            }
        }
    }
    return -1;
}

int getgrouplist(const char* user, int group, int* groups, int* ngroups) {
    int count = 0;
    if (count < *ngroups) {
        groups[count++] = group;
    }
    
    int fd = open(PATH_GROUP);
    if (fd < 0) {
        *ngroups = count;
        return -1;
    }
    
    char buf[4096];
    int bytes = read(fd, buf, sizeof(buf)-1);
    close(fd);
    
    if (bytes > 0) {
        buf[bytes] = '\0';
        char* line = buf;
        for (int i = 0; i <= bytes; i++) {
            if (buf[i] == '\n' || buf[i] == '\0') {
                char old_c = buf[i];
                buf[i] = '\0';
                
                if (*line) {
                    struct group tmp;
                    if (parse_group_line(line, &tmp) == 0) {
                        // Check if user is a member
                        for (int j = 0; tmp.gr_mem[j] != NULL; j++) {
                            if (strcmp(tmp.gr_mem[j], user) == 0) {
                                // Add to list if not already there and there's space
                                int found = 0;
                                for (int k = 0; k < count; k++) {
                                    if (groups[k] == tmp.gr_gid) found = 1;
                                }
                                if (!found && count < *ngroups) {
                                    groups[count++] = tmp.gr_gid;
                                }
                                break;
                            }
                        }
                    }
                }
                
                if (old_c == '\0') break;
                line = &buf[i+1];
            }
        }
    }
    
    *ngroups = count;
    return 0;
}

int add_group(const char* name, int gid) {
    char buf[256];
    int fd = open(PATH_GROUP);
    if (fd < 0) {
        if (create_file(PATH_GROUP) < 0) return -1;
        fd = open(PATH_GROUP);
        if (fd < 0) return -1;
    }
    
    char temp[1024];
    int len = read(fd, temp, sizeof(temp));
    close(fd);
    
    fd = open(PATH_GROUP);
    read(fd, temp, len); 
    
    strcpy(buf, name); strcat(buf, ":x:");
    char gid_s[16];
    int_to_str(gid, gid_s);
    strcat(buf, gid_s); strcat(buf, ":\n");
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}
