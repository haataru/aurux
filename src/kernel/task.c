#include "task.h"
#include "../drivers/pit/pit.h"
#include "../fs/fs.h"

extern volatile unsigned int timer_ticks;

struct task* current_task = NULL;
static struct task* ready_queue = NULL;
static unsigned int next_task_id = 1;
static struct task* task_to_free = NULL;

void tasking_init(void) {
    // Initialize the currently executing main thread.
    struct task* main_task = (struct task*)kmalloc(sizeof(struct task));
    main_task->id = 0;
    main_task->esp = 0; // Value assigned during the first context switch interrupt.
    main_task->kernel_stack = 0; // Ring 0 execution bypasses initial kernel stack requirement.
    main_task->page_dir = NULL; // Maintain kernel identity map without switching the page directory.
    main_task->state = TASK_RUNNING;
    main_task->sleep_ticks = 0;
    main_task->waiting_for_pid = 0;
    main_task->waiting_for_io = NULL;
    main_task->pending_signals = 0;
    main_task->uid = 0;
    main_task->euid = 0;
    main_task->gid = 0;
    for (int i = 0; i < 16; i++) main_task->fd_table[i] = NULL;
    main_task->next = main_task; // Circular queue implementation for round-robin scheduling.
    
    current_task = main_task;
    ready_queue = main_task;
    
    // Initialize standard file descriptors (0, 1, 2)
    extern int fs_open(const char* path);
    fs_open("/dev/tty0"); // 0: stdin
    fs_open("/dev/tty0"); // 1: stdout
    fs_open("/dev/tty0"); // 2: stderr
}

void create_task(void (*entry_point)(void)) {
    // Prevent race conditions by disabling interrupts during task creation.
    asm volatile("cli");
    
    struct task* new_task = (struct task*)kmalloc(sizeof(struct task));
    new_task->id = next_task_id++;
    new_task->page_dir = NULL;
    new_task->state = TASK_READY;
    new_task->sleep_ticks = 0;
    new_task->waiting_for_pid = 0;
    new_task->waiting_for_io = NULL;
    new_task->pending_signals = 0;
    if (current_task) {
        new_task->uid = current_task->uid;
        new_task->euid = current_task->euid;
        new_task->gid = current_task->gid;
    } else {
        new_task->uid = 0;
        new_task->euid = 0;
        new_task->gid = 0;
    }
    for (int i = 0; i < 16; i++) new_task->fd_table[i] = NULL;
    
    unsigned int* stack = (unsigned int*)pmm_alloc_page();
    
    unsigned int* stack_top = (unsigned int*)((unsigned int)stack + 4096);
    
    // Simulate an interrupt context by pushing processor state and registers onto the stack.
    
    *(--stack_top) = 0x202;         // Enable interrupts via EFLAGS IF bit.
    *(--stack_top) = 0x08;         
    *(--stack_top) = (unsigned int)entry_point;
    
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    
    new_task->esp = (unsigned int)stack_top;
    new_task->kernel_stack = (unsigned int)stack + 4096;
    
    struct task* last = ready_queue;
    while (last->next != ready_queue) {
        last = last->next;
    }
    last->next = new_task;
    new_task->next = ready_queue;
    
    asm volatile("sti");
}

void create_user_task(void (*entry_point)(void)) {
    asm volatile("cli");
    
    struct task* new_task = (struct task*)kmalloc(sizeof(struct task));
    new_task->id = next_task_id++;
    new_task->page_dir = NULL;
    new_task->state = TASK_READY;
    new_task->sleep_ticks = 0;
    new_task->waiting_for_pid = 0;
    new_task->waiting_for_io = NULL;
    new_task->pending_signals = 0;
    if (current_task) {
        new_task->uid = current_task->uid;
        new_task->euid = current_task->euid;
        new_task->gid = current_task->gid;
    } else {
        new_task->uid = 0;
        new_task->euid = 0;
        new_task->gid = 0;
    }
    for (int i = 0; i < 16; i++) new_task->fd_table[i] = NULL;
    
    unsigned int* kstack = (unsigned int*)pmm_alloc_page();
    unsigned int* ustack = (unsigned int*)pmm_alloc_page();
    
    unsigned int* stack_top = (unsigned int*)((unsigned int)kstack + 4096);
    
    // User-mode data segment configuration.
    *(--stack_top) = 0x23;         
    *(--stack_top) = (unsigned int)ustack + 4096;
    *(--stack_top) = 0x202;        
    *(--stack_top) = 0x1B;          // User-mode code segment configuration.
    *(--stack_top) = (unsigned int)entry_point;
    
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    
    new_task->esp = (unsigned int)stack_top;
    new_task->kernel_stack = (unsigned int)kstack + 4096;
    
    struct task* last = ready_queue;
    while (last->next != ready_queue) {
        last = last->next;
    }
    last->next = new_task;
    new_task->next = ready_queue;
    
    asm volatile("sti");
}

struct task* create_process(unsigned int* page_dir, unsigned int entry_point, unsigned int ustack_top) {
    asm volatile("cli");
    
    struct task* new_task = (struct task*)kmalloc(sizeof(struct task));
    new_task->id = next_task_id++;
    new_task->page_dir = page_dir;
    new_task->state = TASK_READY;
    new_task->sleep_ticks = 0;
    new_task->waiting_for_pid = 0;
    new_task->waiting_for_io = NULL;
    new_task->pending_signals = 0;
    extern struct task* current_task;
    if (current_task) {
        new_task->uid = current_task->uid;
        new_task->euid = current_task->euid;
        new_task->gid = current_task->gid;
    } else {
        new_task->uid = 0;
        new_task->euid = 0;
        new_task->gid = 0;
    }
    if (current_task) {
        for (int i = 0; i < 16; i++) {
            new_task->fd_table[i] = current_task->fd_table[i];
            if (new_task->fd_table[i]) {
                ((global_file_descriptor_t*)new_task->fd_table[i])->refcount++;
            }
        }
    } else {
        for (int i = 0; i < 16; i++) new_task->fd_table[i] = NULL;
    }
    
    unsigned int* kstack = (unsigned int*)pmm_alloc_page();
    unsigned int* stack_top = (unsigned int*)((unsigned int)kstack + 4096);
    
    *(--stack_top) = 0x23;         
    *(--stack_top) = ustack_top;   
    *(--stack_top) = 0x202;        
    *(--stack_top) = 0x1B;
    *(--stack_top) = entry_point;  
    
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    *(--stack_top) = 0;
    
    new_task->esp = (unsigned int)stack_top;
    new_task->kernel_stack = (unsigned int)kstack + 4096;
    
    struct task* last = ready_queue;
    while (last->next != ready_queue) {
        last = last->next;
    }
    last->next = new_task;
    new_task->next = ready_queue;
    
    asm volatile("sti");
    return new_task;
}

unsigned int timer_handler(unsigned int esp) {
    timer_ticks++;
    
    if (task_to_free && task_to_free != current_task) {
        // Execute task cleanup safely from an independent stack.
        unsigned int* pd = task_to_free->page_dir;
        if (pd) {
            for (int i = 32; i < 1024; i++) {
                if (pd[i] & PAGE_PRESENT) {
                    unsigned int* pt = (unsigned int*)(pd[i] & ~0xFFF);
                    for (int j = 0; j < 1024; j++) {
                        if (pt[j] & PAGE_PRESENT) {
                            pmm_free_page((void*)(pt[j] & ~0xFFF));
                        }
                    }
                    pmm_free_page((void*)pt);
                }
            }
            pmm_free_page((void*)pd);
        }
        if (task_to_free->kernel_stack) {
            pmm_free_page((void*)(task_to_free->kernel_stack - 4096));
        }
        kfree(task_to_free);
        task_to_free = NULL;
    }
    
    if (!current_task) {
        // Acknowledge hardware interrupt early if the scheduler is uninitialized.
        outb(0x20, 0x20);
        return esp;
    }
    
    if (current_task->pending_signals & 2) { // SIGINT
        current_task->state = TASK_DEAD;
        task_to_free = current_task;
        
        struct task* iter2 = current_task->next;
        do {
            if (iter2->waiting_for_pid == current_task->id) {
                iter2->waiting_for_pid = 0;
                iter2->state = TASK_READY;
            }
            iter2 = iter2->next;
        } while (iter2 != current_task);
    } else {
        current_task->esp = esp;
    }
    
    struct task* next_task = current_task->next;
    
    struct task* iter = current_task->next;
    do {
        if (iter->state == TASK_SLEEPING) {
            if (iter->waiting_for_pid == 0) {
                if (iter->sleep_ticks > 0) iter->sleep_ticks--;
                if (iter->sleep_ticks == 0) iter->state = TASK_READY;
            }
        }
        iter = iter->next;
    } while (iter != current_task->next);
    
    while (next_task->state != TASK_READY && next_task->state != TASK_RUNNING) {
        next_task = next_task->next;
        // Single runnable task detection ensures loop termination.
    }
    
    // State modification is omitted as kernel threads default to a ready state unless sleeping.
    
    current_task = next_task;
    
    // Update the TSS stack pointer for privilege level transitions.
    if (current_task->kernel_stack != 0) {
        set_kernel_stack(current_task->kernel_stack);
    }
    
    unsigned int current_pd;
    asm volatile("mov %%cr3, %0" : "=r"(current_pd));
    
    unsigned int target_pd = current_task->page_dir ? (unsigned int)current_task->page_dir : (unsigned int)kernel_page_dir;
    
    if (current_pd != target_pd) {
        asm volatile("mov %0, %%cr3" :: "r"(target_pd));
    }
    
    outb(0x20, 0x20);
    
    return current_task->esp;
}

void sleep(unsigned int ms) {
    // Calculate sleep ticks based on a 100 Hz timer frequency.
    unsigned int ticks = ms / 10;
    if (ticks == 0) ticks = 1;
    
    asm volatile("cli");
    current_task->sleep_ticks = ticks;
    current_task->state = TASK_SLEEPING;
    asm volatile("sti");
    
    yield();
}

void yield(void) {
    asm volatile("int $32"); // Trigger a software interrupt to invoke the scheduler.
}

void destroy_current_process(void) {
    asm volatile("cli");
    current_task->state = TASK_DEAD;
    task_to_free = current_task;
    
    extern int fs_close(int fd);
    for (int i = 0; i < 16; i++) {
        if (current_task->fd_table[i]) {
            fs_close(i);
        }
    }
    
    // Unblock any tasks currently waiting for this process to terminate.
    struct task* iter = ready_queue;
    if (iter) {
        do {
            if (iter->state == TASK_SLEEPING && iter->waiting_for_pid == current_task->id) {
                iter->state = TASK_READY;
                iter->waiting_for_pid = 0;
            }
            iter = iter->next;
        } while (iter != ready_queue);
    }
    
    struct task* prev = current_task;
    while (prev->next != current_task) {
        prev = prev->next;
    }
    prev->next = current_task->next;
    if (ready_queue == current_task) {
        ready_queue = current_task->next;
    }
    
    asm volatile("mov %0, %%cr3" :: "r"(kernel_page_dir));
    asm volatile("int $32");
    while(1);
}

int wait_for_task(unsigned int pid) {
    asm volatile("cli");
    int exists = 0;
    struct task* iter = ready_queue;
    if (iter) {
        do {
            if (iter->id == pid && iter->state != TASK_DEAD) { exists = 1; break; }
            iter = iter->next;
        } while (iter != ready_queue);
    }
    if (!exists) {
        asm volatile("sti");
        return -1;
    }
    
    current_task->state = TASK_SLEEPING;
    current_task->waiting_for_pid = pid;
    asm volatile("sti");
    yield();
    return 0;
}

void sleep_on_io(void* io_obj) {
    asm volatile("cli");
    current_task->waiting_for_io = io_obj;
    current_task->state = TASK_SLEEPING;
    asm volatile("sti");
    yield();
}

void wakeup_tasks_waiting_for_io(void* io_obj) {
    if (!ready_queue) return;
    asm volatile("cli");
    struct task* iter = ready_queue;
    do {
        if (iter->state == TASK_SLEEPING && iter->waiting_for_io == io_obj) {
            iter->state = TASK_READY;
            iter->waiting_for_io = NULL;
        }
        iter = iter->next;
    } while (iter != ready_queue);
    asm volatile("sti");
}

int task_fork(unsigned int esp) {
    asm volatile("cli");
    struct task* parent = current_task;
    
    extern unsigned int* clone_address_space(unsigned int* current_pd);
    unsigned int* new_pd = clone_address_space(parent->page_dir);
    if (!new_pd) {
        asm volatile("sti");
        return -1;
    }
    
    struct task* child = (struct task*)kmalloc(sizeof(struct task));
    child->id = next_task_id++;
    child->page_dir = new_pd;
    child->state = TASK_READY;
    child->sleep_ticks = 0;
    child->waiting_for_pid = 0;
    child->waiting_for_io = NULL;
    child->pending_signals = 0;
    child->uid = parent->uid;
    child->euid = parent->euid;
    child->gid = parent->gid;
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    
    for (int i = 0; i < 16; i++) {
        child->fd_table[i] = parent->fd_table[i];
        if (child->fd_table[i]) {
            int* refcount = (int*)child->fd_table[i] + 1; // Assuming refcount is the second field (after int used)
            (*refcount)++;
        }
    }
    
    unsigned int new_kstack_phys = (unsigned int)pmm_alloc_page();
    unsigned int scratch_vaddr = 0xE0000000;
    vmm_map_page_ex(parent->page_dir, scratch_vaddr, new_kstack_phys, PAGE_PRESENT | PAGE_WRITE);
    asm volatile("invlpg (%0)" :: "r"(scratch_vaddr) : "memory");
    
    unsigned int parent_kstack_start = (parent->kernel_stack - 4096);
    
    if (parent_kstack_start >= (128 * 1024 * 1024)) {
        extern void vga_print(const char*);
        vga_print("task_fork: parent_kstack_start is invalid or corrupted!\n");
        kfree(child);
        pmm_free_page((void*)new_kstack_phys);
        for (int i = 32; i < 1024; i++) {
            if (new_pd[i] & PAGE_PRESENT) {
                unsigned int* pt = (unsigned int*)(new_pd[i] & ~0xFFF);
                for (int j = 0; j < 1024; j++) {
                    if (pt[j] & PAGE_PRESENT) pmm_free_page((void*)(pt[j] & ~0xFFF));
                }
                pmm_free_page((void*)pt);
            }
        }
        pmm_free_page((void*)new_pd);
        asm volatile("sti");
        return -1;
    }
    
    extern void* memcpy(void* dest, const void* src, size_t n);
    memcpy((void*)scratch_vaddr, (void*)parent_kstack_start, 4096);
    
    unsigned int esp_offset = esp - parent_kstack_start;
    child->kernel_stack = new_kstack_phys + 4096;
    child->esp = new_kstack_phys + esp_offset;
    
    unsigned int* child_regs_eax = (unsigned int*)(scratch_vaddr + esp_offset + 28);
    *child_regs_eax = 0;
    
    vmm_map_page_ex(parent->page_dir, scratch_vaddr, 0, 0);
    asm volatile("invlpg (%0)" :: "r"(scratch_vaddr) : "memory");
    
    struct task* last = ready_queue;
    while (last->next != ready_queue) {
        last = last->next;
    }
    last->next = child;
    child->next = ready_queue;
    
    asm volatile("sti");
    return child->id;
}

int task_kill(unsigned int pid, int signal) {
    if (pid == 0) return -1;
    struct task* iter = ready_queue;
    if (!iter) return -1;
    do {
        if (iter->id == pid) {
            iter->pending_signals |= (1 << signal);
            return 0;
        }
        iter = iter->next;
    } while (iter != ready_queue);
    return -1;
}

struct process_info {
    unsigned int id;
    int state;
    char name[32];
};

int task_get_processes(void* buffer, int max_count) {
    struct process_info* info = (struct process_info*)buffer;
    if (!info || max_count <= 0) return -1;
    
    struct task* iter = ready_queue;
    if (!iter) return 0;
    
    int count = 0;
    do {
        info[count].id = iter->id;
        info[count].state = iter->state;
        
        int i;
        for (i = 0; i < 31 && iter->name[i]; i++) {
            info[count].name[i] = iter->name[i];
        }
        info[count].name[i] = '\0';
        
        count++;
        iter = iter->next;
    } while (iter != ready_queue && count < max_count);
    
    return count;
}
