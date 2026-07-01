#ifndef SYSCALLS_H
#define SYSCALLS_H

int write(int fd, const char* data, int size);
void print(const char* str);
void exit(int status);
int read(int fd, char* buf, int count);
// Process management
int fork(void);
int exec(const char* filename, const char* args);
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
void sleep(unsigned int ms);
int kill(unsigned int pid, int signal);
int get_processes(void* buffer, int max_count);
int getuid(void);
int setuid(unsigned int uid);
int getgid(void);
int setgid(unsigned int gid);
int chown(const char* path, int uid, int gid);
int chmod(const char* path, int mode);
int umask(int mask);

#ifndef FS_STAT_DEFINED
#define FS_STAT_DEFINED
struct fs_stat {
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
};
#endif

int stat(const char* path, struct fs_stat* st);
int geteuid(void);

#endif
