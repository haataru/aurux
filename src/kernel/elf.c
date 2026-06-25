#include "elf.h"
#include "task.h"
#include "../fs/fs.h"
#include "../memory/memory.h"
#include "../memory/paging.h"
#include "../drivers/vga/vga.h"
#include "../lib/lib.h"
#include "syscall.h"

int elf_load(const char* filename) {
    int fd = fs_open(filename);
    if (fd < 0) {
        vga_print("ELF: Failed to open file.\n");
        return -1;
    }
    
    Elf32_Ehdr ehdr;
    if (fs_read(fd, (char*)&ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
        vga_print("ELF: Failed to read ELF header.\n");
        fs_close(fd);
        return -1;
    }
    
    unsigned int magic = *(unsigned int*)ehdr.e_ident;
    if (magic != ELF_MAGIC || ehdr.e_type != 2) {
        vga_print("ELF: Invalid magic number or type.\n");
        fs_close(fd);
        return -1;
    }
    
    int phdr_size = ehdr.e_phnum * sizeof(Elf32_Phdr);
    Elf32_Phdr* phdrs = (Elf32_Phdr*)kmalloc(phdr_size);
    if (!phdrs) {
        fs_close(fd);
        return -1;
    }
    
    fs_seek(fd, ehdr.e_phoff);
    if (fs_read(fd, (char*)phdrs, phdr_size) != phdr_size) {
        kfree(phdrs);
        fs_close(fd);
        return -1;
    }
    
    vga_print("ELF: Loading ");
    vga_print(filename);
    vga_print("...\n");
    
    unsigned int* new_pd = create_address_space();
    if (!new_pd) {
        kfree(phdrs);
        fs_close(fd);
        return -1;
    }
    
    unsigned int current_pd;
    asm volatile("mov %%cr3, %0" : "=r"(current_pd));
    asm volatile("mov %0, %%cr3" :: "r"(new_pd));
    
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            unsigned int vaddr = phdrs[i].p_vaddr;
            unsigned int memsz = phdrs[i].p_memsz;
            unsigned int filesz = phdrs[i].p_filesz;
            unsigned int offset = phdrs[i].p_offset;
            
            for (unsigned int p = 0; p < memsz; p += PAGE_SIZE) {
                unsigned int phys = (unsigned int)pmm_alloc_page();
                vmm_map_page_ex(new_pd, vaddr + p, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }
            
            if (filesz > 0) {
                fs_seek(fd, offset);
                fs_read(fd, (char*)vaddr, filesz);
            }
            
            if (memsz > filesz) {
                memset((void*)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }
    
    unsigned int user_stack_top = 0xB0000000;
    unsigned int user_stack_phys = (unsigned int)pmm_alloc_page();
    vmm_map_page_ex(new_pd, user_stack_top - PAGE_SIZE, user_stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    
    unsigned int esp = user_stack_top;
    
    // Switch to new_pd to write to the stack
    asm volatile("mov %0, %%cr3" :: "r"(new_pd));
    
    // Set up argc and argv for elf_load
    int argc = 1;
    char* argv[2];
    
    int filename_len = strlen(filename) + 1;
    esp -= filename_len;
    strcpy((char*)esp, filename);
    argv[0] = (char*)esp;
    argv[1] = NULL;
    
    esp &= ~3;
    
    esp -= 2 * sizeof(char*);
    unsigned int argv_ptr = esp;
    ((char**)esp)[0] = argv[0];
    ((char**)esp)[1] = argv[1];
    
    esp -= 4;
    *(unsigned int*)esp = argv_ptr;
    
    esp -= 4;
    *(unsigned int*)esp = argc;
    
    esp -= 4;
    *(unsigned int*)esp = 0;
    
    asm volatile("mov %0, %%cr3" :: "r"(current_pd));
    
    kfree(phdrs);
    fs_close(fd);
    
    struct task* t = create_process(new_pd, ehdr.e_entry, esp);
    return t ? (int)t->id : -1;
}

int elf_exec(const char* filename, const char* args, struct registers* regs) {
    int fd = fs_open(filename);
    if (fd < 0) {
        return -1;
    }
    
    Elf32_Ehdr ehdr;
    if (fs_read(fd, (char*)&ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
        vga_print("EXEC: Failed to read ELF header.\n");
        fs_close(fd);
        return -1;
    }
    
    unsigned int magic = *(unsigned int*)ehdr.e_ident;
    if (magic != ELF_MAGIC || ehdr.e_type != 2) {
        vga_print("EXEC: Invalid magic number or type.\n");
        fs_close(fd);
        return -1;
    }
    
    int phdr_size = ehdr.e_phnum * sizeof(Elf32_Phdr);
    Elf32_Phdr* phdrs = (Elf32_Phdr*)kmalloc(phdr_size);
    if (!phdrs) {
        fs_close(fd);
        return -1;
    }
    
    fs_seek(fd, ehdr.e_phoff);
    if (fs_read(fd, (char*)phdrs, phdr_size) != phdr_size) {
        kfree(phdrs);
        fs_close(fd);
        return -1;
    }
    
    unsigned int* new_pd = create_address_space();
    if (!new_pd) {
        kfree(phdrs);
        fs_close(fd);
        return -1;
    }
    
    char temp_args[256];
    temp_args[0] = '\0';
    if (args) {
        strncpy(temp_args, args, 255);
        temp_args[255] = '\0';
    }
    
    extern struct task* current_task;
    unsigned int* old_pd = current_task->page_dir;
    
    // Switch immediately to new page directory so we can read directly into memory
    current_task->page_dir = new_pd;
    asm volatile("mov %0, %%cr3" :: "r"(new_pd));
    
    for (int i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            unsigned int vaddr = phdrs[i].p_vaddr;
            unsigned int memsz = phdrs[i].p_memsz;
            unsigned int filesz = phdrs[i].p_filesz;
            unsigned int offset = phdrs[i].p_offset;
            
            for (unsigned int p = 0; p < memsz; p += PAGE_SIZE) {
                unsigned int phys = (unsigned int)pmm_alloc_page();
                vmm_map_page_ex(new_pd, vaddr + p, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }
            
            if (filesz > 0) {
                fs_seek(fd, offset);
                fs_read(fd, (char*)vaddr, filesz);
            }
            
            if (memsz > filesz) {
                memset((void*)(vaddr + filesz), 0, memsz - filesz);
            }
            
            unsigned int segment_end = vaddr + memsz;
            if (segment_end > current_task->heap_start) {
                current_task->heap_start = (segment_end + 0xFFF) & ~0xFFF;
                current_task->heap_end = current_task->heap_start;
            }
        }
    }
    
    unsigned int user_stack_top = 0xB0000000;
    unsigned int user_stack_phys = (unsigned int)pmm_alloc_page();
    vmm_map_page_ex(new_pd, user_stack_top - PAGE_SIZE, user_stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    
    unsigned int esp = user_stack_top;
    
    // Parse arguments from temp_args (which contains the full command string)
    int argc = 0;
    char* argv[32];
    
    if (temp_args[0] != '\0') {
        char* token = temp_args;
        while (*token) {
            while (*token == ' ') token++;
            if (!*token) break;
            
            char* start = token;
            while (*token && *token != ' ') token++;
            
            if (*token == ' ') {
                *token = '\0';
                token++;
            }
            
            int len = strlen(start) + 1;
            esp -= len;
            strcpy((char*)esp, start);
            argv[argc++] = (char*)esp;
            if (argc >= 31) break;
        }
    }
    
    // Fallback if no args provided at all
    if (argc == 0) {
        int filename_len = strlen(filename) + 1;
        esp -= filename_len;
        strcpy((char*)esp, filename);
        argv[argc++] = (char*)esp;
    }
    argv[argc] = NULL;
    
    // Align stack
    esp &= ~3;
    
    // Push argv array
    esp -= (argc + 1) * sizeof(char*);
    unsigned int argv_ptr = esp;
    for (int i = 0; i <= argc; i++) {
        ((char**)esp)[i] = argv[i];
    }
    
    // Push argv
    esp -= 4;
    *(unsigned int*)esp = argv_ptr;
    
    // Push argc
    esp -= 4;
    *(unsigned int*)esp = argc;
    
    // Push dummy return address
    esp -= 4;
    *(unsigned int*)esp = 0;
    
    // Clean up old address space
    for (int i = 256; i < 1024; i++) {
        if (old_pd[i] & PAGE_PRESENT) {
            unsigned int* pt = (unsigned int*)(old_pd[i] & ~0xFFF);
            for (int j = 0; j < 1024; j++) {
                if (pt[j] & PAGE_PRESENT) {
                    pmm_free_page((void*)(pt[j] & ~0xFFF));
                }
            }
            pmm_free_page((void*)pt);
        }
    }
    pmm_free_page((void*)old_pd);
    
    kfree(phdrs);
    fs_close(fd);
    
    regs->eip = ehdr.e_entry;
    regs->useresp = esp;
    regs->eax = 0;
    
    return 0;
}
