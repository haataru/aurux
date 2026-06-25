#include "pipe.h"
#include "../kernel/kernel.h"
#include "../memory/memory.h"
#include "../kernel/task.h"

static int pipe_read(const char* path, char* buf, size_t size, unsigned int offset) {
    (void)path;
    pipe_t* p = (pipe_t*)offset;
    if (!p) return -1;
    
    int bytes_read = 0;
    while (size > 0) {
        asm volatile("cli");
        if (p->bytes_available == 0) {
            if (p->writers == 0) {
                asm volatile("sti");
                break; // EOF
            }
            
            // Sleep and wait for data
            extern void sleep_on_io(void* io_obj); // Needs to be added to task.c
            sleep_on_io(p);
            
            // Woken up, loop again
            asm volatile("sti");
            continue;
        }
        
        buf[bytes_read++] = p->buffer[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
        p->bytes_available--;
        size--;
        
        // Wake up any writers blocked on full pipe
        extern void wakeup_tasks_waiting_for_io(void* io_obj);
        wakeup_tasks_waiting_for_io(p);
        
        asm volatile("sti");
    }
    return bytes_read;
}

static int pipe_write(const char* path, const char* data, size_t size, unsigned int offset) {
    (void)path;
    pipe_t* p = (pipe_t*)offset;
    if (!p) return -1;
    
    int bytes_written = 0;
    while (size > 0) {
        asm volatile("cli");
        if (p->readers == 0) {
            asm volatile("sti");
            return -1; // Broken pipe
        }
        
        if (p->bytes_available == PIPE_BUF_SIZE) {
            // Buffer full, sleep and wait for space
            extern void sleep_on_io(void* io_obj);
            sleep_on_io(p);
            asm volatile("sti");
            continue;
        }
        
        p->buffer[p->write_pos] = data[bytes_written++];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->bytes_available++;
        size--;
        
        // Wake up any readers blocked on empty pipe
        extern void wakeup_tasks_waiting_for_io(void* io_obj);
        wakeup_tasks_waiting_for_io(p);
        
        asm volatile("sti");
    }
    return bytes_written;
}

static int pipe_close(pipe_t* p, int mode) {
    asm volatile("cli");
    if (mode == 1) { // read-only
        p->readers--;
    } else if (mode == 2) { // write-only
        p->writers--;
    }
    
    extern void wakeup_tasks_waiting_for_io(void* io_obj);
    wakeup_tasks_waiting_for_io(p); // wake up anyone waiting on this pipe
    
    if (p->readers == 0 && p->writers == 0) {
        kfree(p);
    }
    asm volatile("sti");
    return 0;
}
static int pipe_close_wrapper(unsigned int offset, void* internal_data, int mode) {
    (void)offset;
    return pipe_close((pipe_t*)internal_data, mode);
}

struct fs_driver pipe_fs_driver = {
    .open = 0,
    .read = pipe_read,
    .write = pipe_write,
    .close = pipe_close_wrapper,
    .create_file = 0,
    .create_dir = 0,
    .delete_file = 0,
    .list_dir = 0,
    .get_size = 0,
    .is_dir = 0
};
