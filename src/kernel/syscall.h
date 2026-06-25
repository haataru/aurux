#ifndef SYSCALL_H
#define SYSCALL_H

#include "kernel.h"

void syscall_handler(unsigned int esp);

struct registers {
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    // Hardware-pushed registers during interrupt.
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int useresp;
    unsigned int ss;
};

void sys_exit(int status);
int sys_mkdir(const char* path);
int sys_listdir(const char* path, char* buf, int size);
int sys_unlink(const char* path);
int sys_chdir(const char* path);
int sys_getcwd(char* buf, int size);

#endif
