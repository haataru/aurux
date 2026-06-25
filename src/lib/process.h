#ifndef PROCESS_H
#define PROCESS_H

typedef enum {
    TASK_RUNNING = 0,
    TASK_READY = 1,
    TASK_SLEEPING = 2,
    TASK_DEAD = 3
} process_state_t;

struct process_info {
    unsigned int id;
    process_state_t state;
    char name[32];
};

#endif
