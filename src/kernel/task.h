#ifndef TASK_H
#define TASK_H

#include "kernel.h"
#include "../memory/memory.h"
#include "../memory/paging.h"
#include "gdt.h"

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

struct task {
    unsigned int id;
    unsigned int esp;
    unsigned int kernel_stack;
    unsigned int* page_dir;
    task_state_t state;
    unsigned int sleep_ticks;
    unsigned int waiting_for_pid;
    void* waiting_for_io;
    struct task* next;
    void* fd_table[16]; // Pointers to global_file_descriptor_t
    unsigned int pending_signals;
    unsigned int heap_start;
    unsigned int heap_end;
    char name[32];
    unsigned int uid;
    unsigned int euid;
    unsigned int gid;
};

void tasking_init(void);
void create_task(void (*entry_point)(void));
void create_user_task(void (*entry_point)(void));
struct task* create_process(unsigned int* page_dir, unsigned int entry_point, unsigned int stack_top);
int wait_for_task(unsigned int pid);
void destroy_current_process(void);
void sleep(unsigned int ms);
void sleep_on_io(void* io_obj);
void wakeup_tasks_waiting_for_io(void* io_obj);
void yield(void);
int task_kill(unsigned int pid, int signal);
int task_get_processes(void* buf, int max_count);

unsigned int timer_handler(unsigned int esp);

#endif
