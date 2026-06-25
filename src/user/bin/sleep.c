#include "../../lib/lib.h"

static int atoi(const char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            res = res * 10 + str[i] - '0';
        } else {
            break;
        }
    }
    return res;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: sleep <seconds>\n");
        return 1;
    }
    
    int seconds = atoi(argv[1]);
    if (seconds <= 0) {
        print("Invalid number of seconds\n");
        return 1;
    }
    
    sleep(seconds * 1000);
    return 0;
}
