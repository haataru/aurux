#include "../lib/lib.h"

int main(int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        print(argv[i]);
        print("\n");
    }
    return 0;
}
