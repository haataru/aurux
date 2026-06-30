#include "pwd.h"
#include "lib.h"
#include "syscalls.h"
#define PATH_PASSWD "/etc/passwd"
#define PATH_GROUP "/etc/groups"

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

static int parse_passwd_line(const char* line, struct passwd* pwd) {
    char buf[256];
    strncpy(buf, line, sizeof(buf));
    buf[255] = '\0';
    
    char* tokens[6];
    int t = 0;
    tokens[t++] = buf;
    
    for (int i = 0; buf[i]; i++) {
        if (buf[i] == ':') {
            buf[i] = '\0';
            if (t < 6) tokens[t++] = &buf[i+1];
        }
    }
    
    if (t < 6) return -1;
    
    strcpy(pwd->pw_name, tokens[0]);
    strcpy(pwd->pw_passwd, tokens[1]);
    
    // Simple atoi
    int uid = 0;
    for (int i = 0; tokens[2][i] >= '0' && tokens[2][i] <= '9'; i++) {
        uid = uid * 10 + (tokens[2][i] - '0');
    }
    pwd->pw_uid = uid;
    
    int gid = 0;
    for (int i = 0; tokens[3][i] >= '0' && tokens[3][i] <= '9'; i++) {
        gid = gid * 10 + (tokens[3][i] - '0');
    }
    pwd->pw_gid = gid;
    
    strcpy(pwd->pw_dir, tokens[4]);
    strcpy(pwd->pw_shell, tokens[5]);
    
    return 0;
}

int getpwuid(int uid, struct passwd* pwd) {
    int fd = open(PATH_PASSWD);
    if (fd < 0) return -1;
    
    char buf[1024];
    int bytes = read(fd, buf, sizeof(buf)-1);
    close(fd);
    
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    char* line = buf;
    for (int i = 0; i < bytes; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            struct passwd tmp;
            if (parse_passwd_line(line, &tmp) == 0) {
                if (tmp.pw_uid == uid) {
                    *pwd = tmp;
                    return 0;
                }
            }
            line = &buf[i+1];
        }
    }
    // Check last line if no trailing newline
    if (*line) {
        struct passwd tmp;
        if (parse_passwd_line(line, &tmp) == 0) {
            if (tmp.pw_uid == uid) {
                *pwd = tmp;
                return 0;
            }
        }
    }
    return -1;
}

int getpwnam(const char* name, struct passwd* pwd) {
    int fd = open(PATH_PASSWD);
    if (fd < 0) return -1;
    
    char buf[1024];
    int bytes = read(fd, buf, sizeof(buf)-1);
    close(fd);
    
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    char* line = buf;
    for (int i = 0; i < bytes; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            struct passwd tmp;
            if (parse_passwd_line(line, &tmp) == 0) {
                if (strcmp(tmp.pw_name, name) == 0) {
                    *pwd = tmp;
                    return 0;
                }
            }
            line = &buf[i+1];
        }
    }
    if (*line) {
        struct passwd tmp;
        if (parse_passwd_line(line, &tmp) == 0) {
            if (strcmp(tmp.pw_name, name) == 0) {
                *pwd = tmp;
                return 0;
            }
        }
    }
    return -1;
}

int add_user(const struct passwd* pwd) {
    char buf[256];
    int fd = open(PATH_PASSWD);
    if (fd < 0) {
        // Create file if not exist
        if (create_file(PATH_PASSWD) < 0) return -1;
        fd = open(PATH_PASSWD);
        if (fd < 0) return -1;
    }
    
    // Seek to end
    // Read all to find end is also possible if seek is not fully supported, but sys_seek exists.
    char temp[1024];
    int len = read(fd, temp, sizeof(temp));
    close(fd);
    
    fd = open(PATH_PASSWD);
    // Just write at offset=len by re-writing? Actually sys_write writes at current offset. But open() starts at 0.
    // Let's use read to seek.
    read(fd, temp, len); 
    
    strcpy(buf, pwd->pw_name); strcat(buf, ":");
    strcat(buf, pwd->pw_passwd); strcat(buf, ":");
    char uid_s[16], gid_s[16];
    int_to_str(pwd->pw_uid, uid_s);
    int_to_str(pwd->pw_gid, gid_s);
    strcat(buf, uid_s); strcat(buf, ":");
    strcat(buf, gid_s); strcat(buf, ":");
    strcat(buf, pwd->pw_dir); strcat(buf, ":");
    strcat(buf, pwd->pw_shell); strcat(buf, "\n");
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}



int update_password(const char* name, const char* new_hash) {
    int fd = open(PATH_PASSWD);
    if (fd < 0) return -1;
    char buf[1024];
    int bytes = read(fd, buf, sizeof(buf)-1);
    close(fd);
    
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    // We rewrite the file in place. It's safe enough for this simplified OS.
    unlink(PATH_PASSWD);
    create_file(PATH_PASSWD);
    fd = open(PATH_PASSWD);
    if (fd < 0) return -1;
    
    char* line = buf;
    char out_buf[256];
    
    for (int i = 0; i < bytes; i++) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
            struct passwd tmp;
            if (parse_passwd_line(line, &tmp) == 0) {
                if (strcmp(tmp.pw_name, name) == 0) {
                    strcpy(tmp.pw_passwd, new_hash);
                }
                strcpy(out_buf, tmp.pw_name); strcat(out_buf, ":");
                strcat(out_buf, tmp.pw_passwd); strcat(out_buf, ":");
                char uid_s[16], gid_s[16];
                int_to_str(tmp.pw_uid, uid_s);
                int_to_str(tmp.pw_gid, gid_s);
                strcat(out_buf, uid_s); strcat(out_buf, ":");
                strcat(out_buf, gid_s); strcat(out_buf, ":");
                strcat(out_buf, tmp.pw_dir); strcat(out_buf, ":");
                strcat(out_buf, tmp.pw_shell); strcat(out_buf, "\n");
                write(fd, out_buf, strlen(out_buf));
            }
            line = &buf[i+1];
        }
    }
    if (*line) {
        struct passwd tmp;
        if (parse_passwd_line(line, &tmp) == 0) {
            if (strcmp(tmp.pw_name, name) == 0) {
                strcpy(tmp.pw_passwd, new_hash);
            }
            strcpy(out_buf, tmp.pw_name); strcat(out_buf, ":");
            strcat(out_buf, tmp.pw_passwd); strcat(out_buf, ":");
            char uid_s[16], gid_s[16];
            int_to_str(tmp.pw_uid, uid_s);
            int_to_str(tmp.pw_gid, gid_s);
            strcat(out_buf, uid_s); strcat(out_buf, ":");
            strcat(out_buf, gid_s); strcat(out_buf, ":");
            strcat(out_buf, tmp.pw_dir); strcat(out_buf, ":");
            strcat(out_buf, tmp.pw_shell); strcat(out_buf, "\n");
            write(fd, out_buf, strlen(out_buf));
        }
    }
    close(fd);
    return 0;
}
