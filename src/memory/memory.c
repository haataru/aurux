/*
 * memory.c - Physical memory allocator using bitmap
 */

#include "memory.h"
#include "../kernel/kernel.h"

#define BITMAP_SIZE (MAX_MEMORY / 4096)  /* One bit per 4KB page */
#define HEAP_START KERNEL_HEAP_START
#define HEAP_END (HEAP_START + KERNEL_HEAP_SIZE)

/* Bitmap for physical memory allocation */
static unsigned int bitmap[BITMAP_SIZE / 32];

/* Current heap pointer */
static void* heap_ptr;

/* Total memory statistics */
static size_t total_memory = 0;
static size_t used_memory = 0;

/*
 * Initialize memory management
 */
void memory_init(void) {
    /* Clear bitmap */
    for (int i = 0; i < BITMAP_SIZE / 32; i++) {
        bitmap[i] = 0;
    }
    
    /* Reserve first pages (null pointer guard, kernel, etc.) */
    bitmap[0] = 0x00000001;  /* First page reserved */
    
    /* Initialize heap pointer */
    heap_ptr = (void*)HEAP_START;
    
    /* Mark kernel pages as used */
    for (size_t addr = 0x100000; addr < HEAP_START; addr += 4096) {
        size_t page = addr / 4096;
        bitmap[page / 32] |= (1 << (page % 32));
    }
    
    total_memory = MAX_MEMORY;
    used_memory = HEAP_START;
}

/*
 * Allocate a page (4KB)
 */
static void* alloc_page(void) {
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        size_t word = i / 32;
        size_t bit = i % 32;
        
        if ((bitmap[word] & (1 << bit)) == 0) {
            /* Found free page */
            bitmap[word] |= (1 << bit);
            used_memory += 4096;
            return (void*)(i * 4096);
        }
    }
    
    /* No free pages */
    return NULL;
}

/*
 * Free a page
 */
static void free_page(void* ptr) {
    size_t page = ((size_t)ptr) / 4096;
    if (page < BITMAP_SIZE) {
        bitmap[page / 32] &= ~(1 << (page % 32));
        used_memory -= 4096;
    }
}

/*
 * Allocate memory from kernel heap
 */
void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* Align to 4 bytes */
    size = (size + 3) & ~3;
    
    /* Check if we have enough memory */
    if ((size_t)heap_ptr + size > HEAP_END) {
        /* Try to allocate from pages */
        void* ptr = alloc_page();
        if (ptr == NULL) {
            return NULL;
        }
        return ptr;
    }
    
    void* ret = heap_ptr;
    heap_ptr = (void*)((size_t)heap_ptr + size);
    used_memory += size;
    
    return ret;
}

/*
 * Free allocated memory
 * Note: Simple allocator doesn't actually free, just updates stats
 */
void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    /* Check if ptr is in heap region */
    if ((size_t)ptr >= HEAP_START && (size_t)ptr < HEAP_END) {
        /* Memory is in heap - we can't easily free it in a simple allocator */
        /* In a more sophisticated allocator, we'd track block sizes */
    } else if ((size_t)ptr >= 0x1000 && (size_t)ptr < MAX_MEMORY) {
        /* Memory is from page allocation */
        free_page(ptr);
    }
}

/*
 * Get total available memory
 */
size_t memory_get_total(void) {
    return total_memory;
}

/*
 * Get used memory
 */
size_t memory_get_used(void) {
    return used_memory;
}

/*
 * Get free memory
 */
size_t memory_get_free(void) {
    return total_memory - used_memory;
}
