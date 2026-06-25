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
            destroy_current_process();
            break;
        case 3:
            if (!validate_user_ptr((void*)arg2, arg3)) {
                vga_print("\n[SIGSEGV] User process tried to access invalid memory!\n");
                destroy_current_process();
                break;
            }
            regs->eax = fs_read(arg1, (char*)arg2, arg3);
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
            regs->eax = fd;
            break;
        case 7:
            if (!validate_user_ptr((void*)arg2, arg3)) {
                destroy_current_process();
                break;
            }
            regs->eax = fs_write(arg1, (const char*)arg2, arg3);
            break;
        case 8:
            regs->eax = fs_close(arg1);
            break;
        case 9:
            regs->eax = fs_seek(arg1, arg2);
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
            extern int task_fork(unsigned int esp);
            regs->eax = task_fork(esp);
            break;
        case 19:
            if (!validate_user_string((const char*)arg1)) {
                destroy_current_process();
                break;
            }
            if (arg2 != 0 && !validate_user_string((const char*)arg2)) {
                destroy_current_process();
                break;
            }
            extern int elf_exec(const char* filename, const char* args, struct registers* regs);
            regs->eax = elf_exec((const char*)arg1, (const char*)arg2, regs);
            break;
        case 20: // sys_brk
            {
                extern struct task* current_task;
                if (arg1 == 0) {
                    regs->eax = current_task->heap_end;
                } else if (arg1 >= current_task->heap_start) {
                    unsigned int new_end = (arg1 + 0xFFF) & ~0xFFF;
                    unsigned int old_end = current_task->heap_end;
                    
                    if (new_end > old_end) {
                        for (unsigned int addr = old_end; addr < new_end; addr += PAGE_SIZE) {
                            unsigned int phys = (unsigned int)pmm_alloc_page();
                            if (!phys) {
                                regs->eax = -1;
                                return;
                            }
                            vmm_map_page_ex(current_task->page_dir, addr, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                        }
                    } else if (new_end < old_end) {
                        for (unsigned int addr = new_end; addr < old_end; addr += PAGE_SIZE) {
                            unsigned int pd_index = addr >> 22;
                            unsigned int pt_index = (addr >> 12) & 0x03FF;
                            if (current_task->page_dir[pd_index] & PAGE_PRESENT) {
                                unsigned int* pt = (unsigned int*)(current_task->page_dir[pd_index] & ~0xFFF);
                                if (pt[pt_index] & PAGE_PRESENT) {
                                    pmm_free_page((void*)(pt[pt_index] & ~0xFFF));
                                    pt[pt_index] = 0;
                                }
                            }
                        }
                    }
                    current_task->heap_end = arg1;
                    regs->eax = current_task->heap_end;
                } else {
                    regs->eax = -1;
                }
            }
            break;
        case 21: // sys_dup2
            regs->eax = fs_dup2(arg1, arg2);
            break;
        default:
            vga_print("Unknown syscall!\n");
            break;
    }
}
