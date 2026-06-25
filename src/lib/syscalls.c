#include "syscalls.h"

int write(int fd, const char* data, int size) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(7), "b"(fd), "c"(data), "d"(size) : "memory");
    return ret;
}

void print(const char* str) {
    int len = 0;
    while(str[len]) len++;
    write(1, str, len);
}

void exit(int status) {
    asm volatile("int $0x80" :: "a"(2), "b"(status) : "memory");
}

int read(int fd, char* buf, int count) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(3), "b"(fd), "c"(buf), "d"(count) : "memory");
    return ret;
}

int fork(void) {
    int pid;
    asm volatile("int $0x80" : "=a"(pid) : "a"(18) : "memory");
    return pid;
}

int exec(const char* path, const char* args) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(19), "b"(path), "c"(args) : "memory");
    return ret;
}

int spawn(const char* path, const char* args) {
    int pid = fork();
    if (pid == 0) {
        if (exec(path, args) < 0) {
            print("\033[91m");
            print(path);
            print(": command not found\033[0m\n");
            exit(1);
        }
    }
    return pid;
}

int wait(int pid) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(5), "b"(pid) : "memory");
    return ret;
}

int pipe(int fd[2]) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(17), "b"(fd) : "memory");
    return ret;
}

int dup2(int oldfd, int newfd) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(21), "b"(oldfd), "c"(newfd) : "memory");
    return ret;
}

int open(const char* path) {
    int fd;
    asm volatile("int $0x80" : "=a"(fd) : "a"(6), "b"(path) : "memory");
    return fd;
}

int create_file(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(15), "b"(path) : "memory");
    return ret;
}

int close(int fd) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(8), "b"(fd) : "memory");
    return ret;
}

int mkdir(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(10), "b"(path) : "memory");
    return ret;
}

int listdir(const char* path, char* buf, int size, int detailed) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(11), "b"(path), "c"(buf), "d"(size), "S"(detailed) : "memory");
    return ret;
}

int unlink(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(12), "b"(path) : "memory");
    return ret;
}

int chdir(const char* path) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(13), "b"(path) : "memory");
    return ret;
}

int getcwd(char* buf, int size) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(14), "b"(buf), "c"(size) : "memory");
    return ret;
}

int gettime(int* time_arr) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(16), "b"(time_arr) : "memory");
    return ret;
}
