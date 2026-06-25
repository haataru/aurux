#include "../../lib/lib.h"

int main(int argc, char** argv) {
    char buf[1024];
    int detailed = 0;
    char* path = ".";
    
    // Simple argument parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            detailed = 1;
        } else {
            path = argv[i];
        }
    }
    
    int res = listdir(path, buf, 1024, detailed);
    if (res >= 0 && buf[0] != '\0') {
        print(buf); 
        print("\n");
    } else if (res < 0) {
        print("ls: cannot open directory\n");
    }
    return 0;
}
