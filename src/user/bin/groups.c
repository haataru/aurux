#include "../../lib/lib.h"
#include "../../lib/pwd.h"
#include "../../lib/grp.h"

int main(int argc, char** argv) {
    struct passwd pwd;
    
    if (argc > 1) {
        if (getpwnam(argv[1], &pwd) != 0) {
            print("groups: user '"); print(argv[1]); print("' does not exist\n");
            return 1;
        }
    } else {
        int uid = getuid();
        if (getpwuid(uid, &pwd) != 0) {
            print("unknown\n");
            return 1;
        }
    }
    
    int groups[32];
    int ngroups = 32;
    if (getgrouplist(pwd.pw_name, pwd.pw_gid, groups, &ngroups) == 0) {
        for (int i = 0; i < ngroups; i++) {
            struct group grp;
            if (getgrgid(groups[i], &grp) == 0) {
                print(grp.gr_name);
            } else {
                char gid_s[16];
                int g = groups[i], k = 0;
                if (g == 0) gid_s[k++] = '0';
                while(g > 0) { gid_s[k++] = '0' + (g % 10); g /= 10; }
                for(int j=0; j<k/2; j++) { char t = gid_s[j]; gid_s[j] = gid_s[k-1-j]; gid_s[k-1-j] = t; }
                gid_s[k] = '\0';
                print(gid_s);
            }
            if (i < ngroups - 1) print(" ");
        }
        print("\n");
    }
    
    return 0;
}
