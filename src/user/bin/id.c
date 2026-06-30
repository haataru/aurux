#include "../../lib/lib.h"
#include "../../lib/pwd.h"
#include "../../lib/grp.h"

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

int main(int argc, char** argv) {
    struct passwd pwd;
    
    if (argc > 1) {
        if (getpwnam(argv[1], &pwd) != 0) {
            print("id: user '"); print(argv[1]); print("' does not exist\n");
            return 1;
        }
    } else {
        int uid = getuid();
        if (getpwuid(uid, &pwd) != 0) {
            print("unknown\n");
            return 1;
        }
    }
    
    char uid_s[16], gid_s[16];
    int_to_str(pwd.pw_uid, uid_s);
    int_to_str(pwd.pw_gid, gid_s);
    
    struct group grp;
    int has_group = (getgrgid(pwd.pw_gid, &grp) == 0);
    
    print("uid="); print(uid_s); print("("); print(pwd.pw_name); print(") ");
    print("gid="); print(gid_s); 
    if (has_group) {
        print("("); print(grp.gr_name); print(") ");
    } else {
        print(" ");
    }
    
    int groups[32];
    int ngroups = 32;
    if (getgrouplist(pwd.pw_name, pwd.pw_gid, groups, &ngroups) == 0) {
        print("groups=");
        for (int i = 0; i < ngroups; i++) {
            char g_s[16];
            int_to_str(groups[i], g_s);
            print(g_s);
            if (getgrgid(groups[i], &grp) == 0) {
                print("("); print(grp.gr_name); print(")");
            }
            if (i < ngroups - 1) print(",");
        }
    }
    print("\n");
    
    return 0;
}
