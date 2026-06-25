#ifndef SYSCALL_H
#define SYSCALL_H

#include "kernel.h"

void syscall_handler(unsigned int esp);

void sys_exit(int status);
int sys_mkdir(const char* path);
int sys_listdir(const char* path, char* buf, int size);
int sys_unlink(const char* path);
int sys_chdir(const char* path);
int sys_getcwd(char* buf, int size);

#endif
