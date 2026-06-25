#ifndef SYSCALLS_H
#define SYSCALLS_H

int write(int fd, const char* data, int size);
void print(const char* str);
void exit(int status);
int read(int fd, char* buf, int count);
int fork(void);
int exec(const char* path, const char* args);
int spawn(const char* path, const char* args);
int wait(int pid);
int pipe(int fd[2]);
int dup2(int oldfd, int newfd);
int open(const char* path);
int create_file(const char* path);
int close(int fd);
int mkdir(const char* path);
int listdir(const char* path, char* buf, int size, int detailed);
int unlink(const char* path);
int chdir(const char* path);
int getcwd(char* buf, int size);
int gettime(int* time_arr);

#endif
