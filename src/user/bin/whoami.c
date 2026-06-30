#include "../../lib/lib.h"
#include "../../lib/pwd.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    int uid = getuid();
    struct passwd pwd;
    
    if (getpwuid(uid, &pwd) == 0) {
        print(pwd.pw_name);
        print("\n");
    } else {
        print("unknown\n");
    }
    
    return 0;
}
