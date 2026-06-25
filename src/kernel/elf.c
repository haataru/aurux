#include "elf.h"
#include "task.h"
#include "../fs/fs.h"
#include "../memory/memory.h"
#include "../memory/paging.h"
#include "../drivers/vga/vga.h"
#include "../lib/lib.h"

int elf_load(const char* filename) {
    char* file_buffer = (char*)pmm_alloc_page(); // Temporary 4KB buffer allocated; needs adjustment for larger files.
    // TODO: Allocate sufficient contiguous physical pages or read headers initially.
    int fd = fs_open(filename);
    if (fd < 0) {
        vga_print("ELF: Failed to open file.\n");
        return -1;
    }
    
    char* elf_buf = (char*)kmalloc(32768); 
    
    int bytes = fs_read(fd, elf_buf, 32768);
    fs_close(fd);
    if (bytes <= 0) {
        vga_print("ELF: Failed to read file.\n");
        return -1;
    }
    
    Elf32_Ehdr* ehdr = (Elf32_Ehdr*)elf_buf;
    
    unsigned int magic = *(unsigned int*)ehdr->e_ident;
    if (magic != ELF_MAGIC) {
        vga_print("ELF: Invalid magic number.\n");
        return -1;
    }
    
    if (ehdr->e_type != 2) {
        vga_print("ELF: Not an executable.\n");
        return -1;
    }
    
    vga_print("ELF: Loading ");
    vga_print(filename);
    vga_print("...\n");
    
    unsigned int* new_pd = create_address_space();
    if (!new_pd) {
        vga_print("ELF: Failed to create address space.\n");
        return -1;
    }
    
    // Map program segments by temporarily switching CR3 to the new address space,
    // copying data from the accessible identity-mapped buffer, and restoring CR3.
       
    unsigned int current_pd;
    asm volatile("mov %%cr3, %0" : "=r"(current_pd));
    asm volatile("mov %0, %%cr3" :: "r"(new_pd));
    
    Elf32_Phdr* phdr = (Elf32_Phdr*)(elf_buf + ehdr->e_phoff);
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            unsigned int vaddr = phdr[i].p_vaddr;
            unsigned int memsz = phdr[i].p_memsz;
            unsigned int filesz = phdr[i].p_filesz;
            unsigned int offset = phdr[i].p_offset;
            
            for (unsigned int p = 0; p < memsz; p += PAGE_SIZE) {
                unsigned int phys = (unsigned int)pmm_alloc_page();
                vmm_map_page_ex(new_pd, vaddr + p, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }
            
            memcpy((void*)vaddr, elf_buf + offset, filesz);
            
            // Zero-initialize the BSS segment to account for uninitialized memory allocation.
            if (memsz > filesz) {
                memset((void*)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }
    
    unsigned int user_stack_top = 0xB0000000;
    unsigned int user_stack_phys = (unsigned int)pmm_alloc_page();
    vmm_map_page_ex(new_pd, user_stack_top - PAGE_SIZE, user_stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    
    asm volatile("mov %0, %%cr3" :: "r"(current_pd));
    
    struct task* t = create_process(new_pd, ehdr->e_entry, user_stack_top - 4);
    
    return t ? t->id : -1;
}
