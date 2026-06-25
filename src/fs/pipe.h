#ifndef PIPE_H
#define PIPE_H

#include "fs.h"

#define PIPE_BUF_SIZE 4096

typedef struct pipe {
    char buffer[PIPE_BUF_SIZE];
    unsigned int read_pos;
    unsigned int write_pos;
    unsigned int bytes_available;
    int readers;
    int writers;
} pipe_t;

void pipe_init(void);

#endif
