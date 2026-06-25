#include "../lib/lib.h"

int main(int argc, char** argv) {
    print("\033[92mHello from a user process!\033[0m\n");
    if (argc > 1) {
        print("Arguments received:\n");
        for (int i = 1; i < argc; i++) {
            print(argv[i]);
            print("\n");
        }
    }
    return 0;
}
