#include "../../lib/lib.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        // Read from stdin if no arguments
        char c;
        while (read(0, &c, 1) > 0) {
            write(1, &c, 1);
        }
        return 0;
    }
    
    int fd = open(argv[1]);
    if (fd >= 0) {
        char buf[512];
        int n;
        while ((n = read(fd, buf, 512)) > 0) {
            write(1, buf, n);
        }
        close(fd);
    } else if (fd == -2) {
        print("cat: "); print(argv[1]); print(": Permission denied\n");
    } else {
        print("cat: "); print(argv[1]); print(": No such file or directory\n");
    }
    return 0;
}
