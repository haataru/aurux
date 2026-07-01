#include "../../lib/lib.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        print("Usage: chmod <octal_mode> <file>\n");
        return 1;
    }
    
    const char* mode_str = argv[1];
    int mode = 0;
    
    // Parse octal
    for (int i = 0; mode_str[i] != '\0'; i++) {
        if (mode_str[i] >= '0' && mode_str[i] <= '7') {
            mode = mode * 8 + (mode_str[i] - '0');
        } else {
            print("chmod: invalid mode: '");
            print(mode_str);
            print("'\n");
            return 1;
        }
    }
    
    for (int i = 2; i < argc; i++) {
        if (chmod(argv[i], mode) < 0) {
            print("chmod: changing permissions of '");
            print(argv[i]);
            print("': Operation not permitted or No such file\n");
            return 1;
        }
    }
    
    return 0;
}
