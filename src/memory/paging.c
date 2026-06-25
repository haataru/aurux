#include "paging.h"
#include "memory.h"
#include "../drivers/vga/vga.h"
#include "../lib/lib.h"

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
    
    // Identity map the first 128MB of memory to preserve kernel execution
    // 128MB equals 32768 pages
    for (unsigned int i = 0; i < 32768 * PAGE_SIZE; i += PAGE_SIZE) {
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
    
    // Copy kernel PDEs (first 128MB = 32 PDEs)
    for (int i = 0; i < 32; i++) {
        new_pd[i] = kernel_page_dir[i];
    }
    
    // Note: VGA buffer at 0xB8000 is within the first 4MB and is copied automatically
    return new_pd;
}

unsigned int* clone_address_space(unsigned int* current_pd) {
    unsigned int* new_pd = (unsigned int*)pmm_alloc_page();
    if (!new_pd) return 0;
    
    for (int i = 0; i < 1024; i++) {
        new_pd[i] = 0x02; 
    }
    
    // Copy kernel PDEs
    for (int i = 0; i < 32; i++) {
        new_pd[i] = current_pd[i];
    }
    
    // Copy user PDEs
    for (int i = 32; i < 1024; i++) {
        if (current_pd[i] & PAGE_PRESENT) {
            unsigned int* parent_pt = (unsigned int*)(current_pd[i] & ~0xFFF);
            unsigned int* new_pt = (unsigned int*)pmm_alloc_page();
            
            for (int j = 0; j < 1024; j++) {
                if (parent_pt[j] & PAGE_PRESENT) {
                    unsigned int phys = (unsigned int)pmm_alloc_page();
                    unsigned int flags = parent_pt[j] & 0xFFF;
                    new_pt[j] = phys | flags;
                    
                    unsigned int parent_phys = parent_pt[j] & ~0xFFF;
                    extern void* memcpy(void* dest, const void* src, size_t n);
                    memcpy((void*)phys, (void*)parent_phys, 4096);
                } else {
                    new_pt[j] = 0;
                }
            }
            new_pd[i] = ((unsigned int)new_pt) | (current_pd[i] & 0xFFF);
        }
    }
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

static void serial_print(const char* str) {
    while (*str) {
        while ((inb(0x3F8 + 5) & 0x20) == 0);
        outb(0x3F8, *str++);
    }
}

void page_fault_handler(unsigned int error_code, unsigned int faulting_address, unsigned int eip) {
    vga_print("\nPAGE FAULT! Address: 0x");
    serial_print("\nPAGE FAULT! Address: 0x");
    
    char hex[9];
    hex[8] = 0;
    unsigned int temp = faulting_address;
    for(int i=7; i>=0; i--) {
        int nibble = temp & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        temp >>= 4;
    }
    vga_print(hex);
    serial_print(hex);
    
    vga_print(" Error Code: 0x");
    serial_print(" Error Code: 0x");
    temp = error_code;
    for(int i=7; i>=0; i--) {
        int nibble = temp & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        temp >>= 4;
    }
    vga_print(hex);
    serial_print(hex);
    
    vga_print(" EIP: 0x");
    serial_print(" EIP: 0x");
    temp = eip;
    for(int i=7; i>=0; i--) {
        int nibble = temp & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        temp >>= 4;
    }
    vga_print(hex);
    serial_print(hex);
    serial_print(" Halt.\n");
    
    vga_print(" EIP: 0x");
    temp = eip;
    for(int i=7; i>=0; i--) {
        int nibble = temp & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        temp >>= 4;
    }
    vga_print(hex);
    
    vga_print(" Halt.\n");
    while(1) {
        asm volatile("hlt");
    }
}
