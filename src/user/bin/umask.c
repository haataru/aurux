#include "../../lib/lib.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        int old_mask = umask(0);
        umask(old_mask);
        
        // Print in octal
        char buf[8];
        buf[0] = '0';
        buf[1] = ((old_mask >> 6) & 7) + '0';
        buf[2] = ((old_mask >> 3) & 7) + '0';
        buf[3] = (old_mask & 7) + '0';
        buf[4] = '\n';
        buf[5] = '\0';
        print(buf);
        return 0;
    }
    
    const char* mode_str = argv[1];
    int mask = 0;
    
    // Parse octal
    for (int i = 0; mode_str[i] != '\0'; i++) {
        if (mode_str[i] >= '0' && mode_str[i] <= '7') {
            mask = mask * 8 + (mode_str[i] - '0');
        } else {
            print("umask: invalid mode\n");
            return 1;
        }
    }
    
    umask(mask);
    return 0;
}
