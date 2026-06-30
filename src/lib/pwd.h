#ifndef PWD_H
#define PWD_H

struct passwd {
    char pw_name[32];
    char pw_passwd[128];
    int pw_uid;
    int pw_gid;
    char pw_dir[64];
    char pw_shell[64];
};

// Find user by UID
int getpwuid(int uid, struct passwd* pwd);

// Find user by name
int getpwnam(const char* name, struct passwd* pwd);

// Add a new user to /etc/passwd
int add_user(const struct passwd* pwd);

// Update password for existing user
int update_password(const char* name, const char* new_hash);

#endif
