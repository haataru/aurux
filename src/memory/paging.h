#ifndef PAGING_H
#define PAGING_H

#include "../kernel/kernel.h"
#include "memory.h"

#define PAGE_SIZE 4096

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04

extern unsigned int* kernel_page_dir;

void paging_init(void);
void vmm_map_page(unsigned int virt_addr, unsigned int phys_addr, unsigned int flags);
void vmm_map_page_ex(unsigned int* page_dir, unsigned int virt_addr, unsigned int phys_addr, unsigned int flags);
unsigned int* create_address_space(void);
unsigned int* clone_address_space(unsigned int* current_pd);
void vmm_unmap_page(unsigned int virt_addr);
void page_fault_handler(unsigned int error_code, unsigned int faulting_address, unsigned int eip);

#endif
