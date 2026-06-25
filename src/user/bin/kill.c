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
        print("Usage: kill <pid> [signal]\n");
        return 1;
    }
    
    int pid = atoi(argv[1]);
    int signal = 9; // Default to SIGKILL
    
    if (argc >= 3) {
        signal = atoi(argv[2]);
    }
    
    if (pid <= 0) {
        print("Invalid PID\n");
        return 1;
    }
    
    int ret = kill(pid, signal);
    if (ret < 0) {
        print("Failed to kill process or process not found\n");
        return 1;
    }
    
    return 0;
}
