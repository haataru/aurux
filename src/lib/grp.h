#ifndef GRP_H
#define GRP_H

struct group {
    char gr_name[32];
    char gr_passwd[128];
    int gr_gid;
    char* gr_mem[32]; // Null-terminated array of pointers to members
};

// Find group by GID
int getgrgid(int gid, struct group* grp);

// Find group by name
int getgrnam(const char* name, struct group* grp);

// Get list of all groups a user belongs to
// Returns 0 on success, -1 on error.
// The array 'groups' will be filled with GIDs.
// 'ngroups' should be initialized to the size of 'groups' array,
// and will be updated to the actual number of groups found.
int getgrouplist(const char* user, int group, int* groups, int* ngroups);

// Add a new group to /etc/groups
int add_group(const char* name, int gid);

#endif
