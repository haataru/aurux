#include "malloc.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

struct block_meta {
    size_t size;
    struct block_meta *next;
    int free;
};

#define META_SIZE sizeof(struct block_meta)
static void *global_base = NULL;

static void* sbrk(int increment) {
    unsigned int current_brk;
    asm volatile("int $0x80" : "=a"(current_brk) : "a"(20), "b"(0) : "memory");
    
    if (increment == 0) return (void*)current_brk;
    
    unsigned int new_brk;
    unsigned int target = current_brk + increment;
    asm volatile("int $0x80" : "=a"(new_brk) : "a"(20), "b"(target) : "memory");
    
    if (new_brk == (unsigned int)-1) {
        return (void*)-1;
    }
    return (void*)current_brk;
}

static struct block_meta *find_free_block(struct block_meta **last, size_t size) {
    struct block_meta *current = (struct block_meta*)global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

static struct block_meta *request_space(struct block_meta* last, size_t size) {
    struct block_meta *block;
    block = (struct block_meta *)sbrk(0);
    void *request = sbrk(size + META_SIZE);
    if (request == (void*)-1) {
        return NULL; // sbrk failed.
    }
    
    if (last) { // NULL on first request.
        last->next = block;
    }
    block->size = size;
    block->next = NULL;
    block->free = 0;
    return block;
}

void *malloc(size_t size) {
    struct block_meta *block;

    if (size <= 0) {
        return NULL;
    }

    if (!global_base) { // First call.
        block = request_space(NULL, size);
        if (!block) {
            return NULL;
        }
        global_base = block;
    } else {
        struct block_meta *last = (struct block_meta*)global_base;
        block = find_free_block(&last, size);
        if (!block) { // Failed to find free block.
            block = request_space(last, size);
            if (!block) {
                return NULL;
            }
        } else { // Found free block
            block->free = 0;
        }
    }
    
    return (block + 1);
}

static struct block_meta *get_block_ptr(void *ptr) {
    return (struct block_meta*)ptr - 1;
}

void free(void *ptr) {
    if (!ptr) {
        return;
    }
    
    struct block_meta* block_ptr = get_block_ptr(ptr);
    block_ptr->free = 1;
}
