#include "paging.h"
#include "memory.h"
#include "../drivers/vga/vga.h"

unsigned int* kernel_page_dir = NULL;

void paging_init(void) {
    kernel_page_dir = (unsigned int*)pmm_alloc_page();
    if (!kernel_page_dir) {
        vga_print("PANIC: Could not allocate page directory!\n");
        while(1);
    }
    
    for (int i = 0; i < 1024; i++) {
        // Initialize as not present, read/write, supervisor
        kernel_page_dir[i] = 0x02; 
    }
    
    // Identity map the first 16MB of memory to preserve kernel execution
    // 16MB equals 4096 pages
    for (unsigned int i = 0; i < 4096 * PAGE_SIZE; i += PAGE_SIZE) {
        vmm_map_page(i, i, PAGE_PRESENT | PAGE_WRITE);
    }
    
    // Load page directory into CR3 and enable paging bit in CR0
    asm volatile(
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :: "r"(kernel_page_dir) : "eax"
    );
    
    vga_print("Paging enabled.\n");
}

void vmm_map_page_ex(unsigned int* page_dir, unsigned int virt_addr, unsigned int phys_addr, unsigned int flags) {
    unsigned int pd_index = virt_addr >> 22;
    unsigned int pt_index = (virt_addr >> 12) & 0x03FF;
    
    if ((page_dir[pd_index] & PAGE_PRESENT) == 0) {
        unsigned int* new_pt = (unsigned int*)pmm_alloc_page();
        for (int i = 0; i < 1024; i++) {
            new_pt[i] = 0;
        }
        page_dir[pd_index] = ((unsigned int)new_pt) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    
    unsigned int* pt = (unsigned int*)(page_dir[pd_index] & ~0xFFF);
    pt[pt_index] = (phys_addr & ~0xFFF) | flags;
}

void vmm_map_page(unsigned int virt_addr, unsigned int phys_addr, unsigned int flags) {
    vmm_map_page_ex(kernel_page_dir, virt_addr, phys_addr, flags);
}

unsigned int* create_address_space(void) {
    unsigned int* new_pd = (unsigned int*)pmm_alloc_page();
    if (!new_pd) return NULL;
    
    for (int i = 0; i < 1024; i++) {
        new_pd[i] = 0x02; // Not present, R/W, supervisor
    }
    
    // Copy kernel mappings; 16MB is 4 page tables where each covers 4MB
    // Copy the first 4 Page Directory Entries
    for (int i = 0; i < 4; i++) {
        new_pd[i] = kernel_page_dir[i];
    }
    
    // Note: VGA buffer at 0xB8000 is within the first 4MB and is copied automatically
    return new_pd;
}

void vmm_unmap_page(unsigned int virt_addr) {
    unsigned int pd_index = virt_addr >> 22;
    unsigned int pt_index = (virt_addr >> 12) & 0x03FF;
    
    if (kernel_page_dir[pd_index] & PAGE_PRESENT) {
        unsigned int* pt = (unsigned int*)(kernel_page_dir[pd_index] & ~0xFFF);
        pt[pt_index] = 0;
        // Invalidate Translation Lookaside Buffer
        asm volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
    }
}

void page_fault_handler(unsigned int error_code, unsigned int faulting_address) {
    vga_print("\nPAGE FAULT! Address: 0x");
    // TODO: Implement hex printing using string library
    // Currently assumes a simple halt
    vga_print(" Halt.\n");
    while(1) {
        asm volatile("hlt");
    }
}
