#include "../../lib/lib.h"
#include "../../lib/process.h"

static void itoa(int n, char s[], int base) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        int d = n % base;
        s[i++] = (d < 10) ? (d + '0') : (d - 10 + 'a');
    } while ((n /= base) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char c = s[j];
        s[j] = s[k];
        s[k] = c;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    struct process_info processes[64];
    int count = get_processes(processes, 64);
    
    if (count < 0) {
        print("Failed to get processes\n");
        return 1;
    }
    
    print("PID\tSTATE\t\tNAME\n");
    print("---\t-----\t\t----\n");
    for (int i = 0; i < count; i++) {
        char pid_str[16];
        itoa(processes[i].id, pid_str, 10);
        print(pid_str);
        print("\t");
        
        switch (processes[i].state) {
            case TASK_RUNNING:
                print("RUNNING");
                break;
            case TASK_READY:
                print("READY");
                break;
            case TASK_SLEEPING:
                print("SLEEPING");
                break;
            case TASK_DEAD:
                print("DEAD");
                break;
            default:
                print("UNKNOWN");
                break;
        }
        
        if (processes[i].state == TASK_READY || processes[i].state == TASK_DEAD) {
            print("\t\t");
        } else {
            print("\t");
        }
        
        print(processes[i].name);
        print("\n");
    }
    
    return 0;
}
