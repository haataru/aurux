#include "../../lib/lib.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        print("mkdir: missing operand\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i]) < 0) {
            print("mkdir: cannot create directory '");
            print(argv[i]);
            print("'\n");
        }
    }
    return 0;
}
