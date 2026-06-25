#include "memory.h"
#include "paging.h"
#include "../kernel/kernel.h"

#define PAGE_SIZE 4096
#define MAX_BITMAP_SIZE (128 * 1024 * 1024 / PAGE_SIZE / 32) // Maximum of 128MB supported for bitmap size

static unsigned int bitmap[MAX_BITMAP_SIZE];
static size_t total_memory = 16 * 1024 * 1024; // Default size is 16MB
static size_t used_memory = 0;
static size_t bitmap_size = (16 * 1024 * 1024) / PAGE_SIZE;

void memory_init(struct multiboot_info* mbi) {
    if (mbi && (mbi->flags & 1)) {
        // mbi->mem_upper is in KB and starts at 1MB
        total_memory = (mbi->mem_upper * 1024) + (1024 * 1024);
    }
    bitmap_size = total_memory / PAGE_SIZE;
    if (bitmap_size > MAX_BITMAP_SIZE * 32) {
        bitmap_size = MAX_BITMAP_SIZE * 32;
        total_memory = bitmap_size * PAGE_SIZE;
    }

    for (size_t i = 0; i < MAX_BITMAP_SIZE; i++) {
        bitmap[i] = 0;
    }

    // Mark kernel and pre-kernel regions as used
    unsigned int end_addr = (unsigned int)&kernel_end;
    end_addr = (end_addr + 0xFFF) & ~0xFFF;
    
    for (unsigned int addr = 0; addr < end_addr; addr += PAGE_SIZE) {
        unsigned int page = addr / PAGE_SIZE;
        bitmap[page / 32] |= (1 << (page % 32));
        used_memory += PAGE_SIZE;
    }
}

void* pmm_alloc_page(void) {
    for (size_t i = 0; i < bitmap_size; i++) {
        size_t word = i / 32;
        size_t bit = i % 32;
        
        if ((bitmap[word] & (1 << bit)) == 0) {
            bitmap[word] |= (1 << bit);
            used_memory += PAGE_SIZE;
            
            unsigned int* ptr = (unsigned int*)(i * PAGE_SIZE);
            for(int j = 0; j < 1024; j++) ptr[j] = 0;
            
            return (void*)ptr;
        }
    }
    return NULL;
}

void pmm_free_page(void* ptr) {
    unsigned int addr = (unsigned int)ptr;
    if (addr % PAGE_SIZE != 0) return;
    
    unsigned int page = addr / PAGE_SIZE;
    if (page < bitmap_size) {
        size_t word = page / 32;
        size_t bit = page % 32;
        bitmap[word] &= ~(1 << bit);
        used_memory -= PAGE_SIZE;
    }
}

struct block_header {
    size_t size;
    int is_free;
    struct block_header* next;
};

static struct block_header* heap_head = NULL;

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    size = (size + 3) & ~3;
    
    struct block_header* curr = heap_head;
    struct block_header* prev = NULL;
    
    // First fit search algorithm
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size > size + sizeof(struct block_header) + 16) {
                struct block_header* next_block = (struct block_header*)((unsigned int)curr + sizeof(struct block_header) + size);
                next_block->size = curr->size - size - sizeof(struct block_header);
                next_block->is_free = 1;
                next_block->next = curr->next;
                
                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = 0;
            return (void*)((unsigned int)curr + sizeof(struct block_header));
        }
        prev = curr;
        curr = curr->next;
    }
    
    // No free block found; allocate new page(s)
    size_t total_size = size + sizeof(struct block_header);
    size_t pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Allocate contiguous physical pages; relies on identity mapping
    // Implements a simple loop to find N contiguous bits in the bitmap
    
    unsigned int start_page = 0;
    unsigned int count = 0;
    
    for (size_t i = 0; i < bitmap_size; i++) {
        size_t word = i / 32;
        size_t bit = i % 32;
        if ((bitmap[word] & (1 << bit)) == 0) {
            if (count == 0) start_page = i;
            count++;
            if (count == pages_needed) break;
        } else {
            count = 0;
        }
    }
    
    if (count < pages_needed) return NULL;
    
    for (size_t i = start_page; i < start_page + pages_needed; i++) {
        bitmap[i / 32] |= (1 << (i % 32));
        used_memory += PAGE_SIZE;
        unsigned int phys = i * PAGE_SIZE;
        // Ensure it's mapped in the kernel page directory if it's beyond 16MB
        if (phys >= 0x01000000) {
            vmm_map_page(phys, phys, PAGE_PRESENT | PAGE_WRITE);
        }
    }
    struct block_header* new_block = (struct block_header*)(start_page * PAGE_SIZE);
    new_block->size = (pages_needed * PAGE_SIZE) - sizeof(struct block_header);
    new_block->is_free = 0;
    new_block->next = NULL;
    
    if (new_block->size > size + sizeof(struct block_header) + 16) {
        struct block_header* split = (struct block_header*)((unsigned int)new_block + sizeof(struct block_header) + size);
        split->size = new_block->size - size - sizeof(struct block_header);
        split->is_free = 1;
        split->next = NULL;
        
        new_block->size = size;
        new_block->next = split;
    }
    
    if (prev) {
        prev->next = new_block;
    } else {
        heap_head = new_block;
    }
    
    return (void*)((unsigned int)new_block + sizeof(struct block_header));
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    struct block_header* block = (struct block_header*)((unsigned int)ptr - sizeof(struct block_header));
    block->is_free = 1;
    
    // Coalesce adjacent free blocks
    struct block_header* curr = heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            // Check if adjacent free block addresses match for contiguous merging
            if ((unsigned int)curr + sizeof(struct block_header) + curr->size == (unsigned int)curr->next) {
                curr->size += sizeof(struct block_header) + curr->next->size;
                curr->next = curr->next->next;
                continue; // Re-evaluate with the newly merged next block
            }
        }
        curr = curr->next;
    }
}

size_t memory_get_total(void) {
    return total_memory;
}

size_t memory_get_used(void) {
    return used_memory;
}

size_t memory_get_free(void) {
    return total_memory - used_memory;
}
