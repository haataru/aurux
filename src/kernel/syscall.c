#include "syscall.h"
#include "../drivers/vga/vga.h"
#include "task.h"
#include "../memory/paging.h"
#include "../fs/fs.h"
#include "../drivers/rtc/rtc.h"

// struct registers moved to syscall.h

static int validate_user_ptr(const void* ptr, unsigned int size) {
    unsigned int addr = (unsigned int)ptr;
    if (addr < 0x01000000) return 0;
    if (addr + size < addr) return 0;
    
    unsigned int start_page = addr / 4096;
    unsigned int end_page = size == 0 ? start_page : (addr + size - 1) / 4096;
    
    unsigned int current_pd;
    asm volatile("mov %%cr3, %0" : "=r"(current_pd));
    unsigned int* pd = (unsigned int*)current_pd;
    
    for (unsigned int p = start_page; p <= end_page; p++) {
        unsigned int pd_idx = p / 1024;
        unsigned int pt_idx = p % 1024;
        
        if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
        if (!(pd[pd_idx] & PAGE_USER)) return 0;
        
        unsigned int* pt = (unsigned int*)(pd[pd_idx] & ~0xFFF);
        if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
        if (!(pt[pt_idx] & PAGE_USER)) return 0;
    }
    return 1;
}

static int validate_user_string(const char* str) {
    unsigned int addr = (unsigned int)str;
    if (addr < 0x01000000) return 0;
    
    unsigned int current_pd;
    asm volatile("mov %%cr3, %0" : "=r"(current_pd));
    unsigned int* pd = (unsigned int*)current_pd;
    
    while (1) {
        unsigned int p = addr / 4096;
        unsigned int pd_idx = p / 1024;
        unsigned int pt_idx = p % 1024;
        
        if (!(pd[pd_idx] & PAGE_PRESENT) || !(pd[pd_idx] & PAGE_USER)) return 0;
        
        unsigned int* pt = (unsigned int*)(pd[pd_idx] & ~0xFFF);
        if (!(pt[pt_idx] & PAGE_PRESENT) || !(pt[pt_idx] & PAGE_USER)) return 0;
        
        unsigned int page_end = (addr & ~0xFFF) + 4096;
        while (addr < page_end) {
            if (*(char*)addr == '\0') return 1;
            addr++;
        }
    }
}

void syscall_handler(unsigned int esp) {
    struct registers* regs = (struct registers*)esp;
    
    unsigned int syscall_no = regs->eax;
    unsigned int arg1 = regs->ebx;
    unsigned int arg2 = regs->ecx;
    unsigned int arg3 = regs->edx;
    unsigned int arg4 = regs->esi;
    
    switch (syscall_no) {
        case 1:
            if (!validate_user_string((const char*)arg1)) {
                vga_print("\n[SIGSEGV] User process tried to access invalid memory!\n");
                destroy_current_process();
                break;
            }
            vga_print((const char*)arg1);
            break;
        case 2:
            vga_print("\n[User thread exited]\n");
            destroy_current_process();
            break;
        case 3:
            if (arg1 == 0) { // File descriptor 0 indicates standard input.
                if (!validate_user_ptr((void*)arg2, arg3)) {
                    vga_print("\n[SIGSEGV] User process tried to access invalid memory!\n");
                    destroy_current_process();
                    break;
                }
                char* buf = (char*)arg2;
                unsigned int count = arg3;
                extern char keyboard_getchar(void);
                for (unsigned int i = 0; i < count; i++) {
                    buf[i] = keyboard_getchar();
                    if (buf[i] == '\n') {
                        regs->eax = i + 1;
                        break;
                    }
                }
                if (count > 0 && buf[count-1] != '\n') {
                    regs->eax = count;
                }
            } else {
                if (!validate_user_ptr((void*)arg2, arg3)) {
                    vga_print("\n[SIGSEGV] User process tried to access invalid memory!\n");
                    destroy_current_process();
                    break;
                }
                regs->eax = fs_read(arg1 - 3, (char*)arg2, arg3);
            }
            break;
        case 4:
            if (!validate_user_string((const char*)arg1)) {
                vga_print("\n[SIGSEGV] User process tried to access invalid memory!\n");
                destroy_current_process();
                break;
            }
            extern int elf_load(const char* filename);
            regs->eax = elf_load((const char*)arg1);
            break;
        case 5:
            regs->eax = wait_for_task(arg1);
            break;
        case 6:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            int fd = fs_open((const char*)arg1);
            if (fd >= 0) regs->eax = fd + 3;
            else regs->eax = -1;
            break;
        case 7:
            if (arg1 == 1 || arg1 == 2) {
                // Standard output and error streams are pending full implementation.
                regs->eax = arg3;
            } else {
                if (!validate_user_ptr((void*)arg2, arg3)) {
                    destroy_current_process();
                    break;
                }
                regs->eax = fs_write(arg1 - 3, (const char*)arg2, arg3);
            }
            break;
        case 8:
            regs->eax = fs_close(arg1 - 3);
            break;
        case 9:
            regs->eax = fs_seek(arg1 - 3, arg2);
            break;
        case 10:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_create_dir((const char*)arg1);
            break;
        case 11:
            if (!validate_user_string((const char*)arg1) || !validate_user_ptr((void*)arg2, arg3)) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_list((const char*)arg1, (char*)arg2, arg3, arg4);
            break;
        case 12:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_delete((const char*)arg1);
            break;
        case 13:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_change_dir((const char*)arg1);
            break;
        case 14:
            if (!validate_user_ptr((void*)arg1, arg2)) {
                destroy_current_process();
                break;
            }
            const char* cwd = fs_get_cwd();
            int len = 0;
            while (cwd[len] != '\0') len++;
            if (len >= (int)arg2) {
                regs->eax = -1;
            } else {
                for(int i=0; i<=len; i++) ((char*)arg1)[i] = cwd[i];
                regs->eax = 0;
            }
            break;
        case 15:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_create_file((const char*)arg1);
            break;
        case 16:
            if (!validate_user_ptr((void*)arg1, 6 * sizeof(int))) {
                destroy_current_process();
                break;
            }
            {
                int* time_arr = (int*)arg1;
                rtc_getDate(&time_arr[2], &time_arr[1], &time_arr[0]); 
                rtc_getTime(&time_arr[3], &time_arr[4], &time_arr[5]); 
            }
            regs->eax = 0;
            break;
        case 17:
            if (!validate_user_ptr((void*)arg1, 2 * sizeof(int))) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_pipe((int*)arg1);
            break;
        case 18:
            extern int task_fork(void);
            regs->eax = task_fork();
            break;
        case 19:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            extern int elf_exec(const char* filename, struct registers* regs);
            regs->eax = elf_exec((const char*)arg1, regs);
            break;
        default:
            vga_print("Unknown syscall!\n");
            break;
    }
}
