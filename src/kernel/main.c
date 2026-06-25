
#include "kernel.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/rtc/rtc.h"
#include "../memory/memory.h"
#include "../fs/fs.h"
#include "../shell/shell.h"
#include "../memory/paging.h"
#include "../drivers/pit/pit.h"
#include "task.h"

#include "gdt.h"
#include "syscall.h"
#include "../drivers/ata/ata.h"
#include "elf.h"


void OSmain(unsigned int magic, unsigned int addr) {
    vga_init();
    
    struct multiboot_info* mbi = (struct multiboot_info*)addr;
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_print("Invalid multiboot magic!\n");
        mbi = NULL;
    }
    
    gdt_init();
    memory_init(mbi);
    paging_init();
    
    ata_init();
    fs_init();
    
    keyboard_init();
    rtc_init();
    
    tasking_init();
    pit_init(100); // Frequency set to 100 Hz.
    
    vga_print("\033[96m  __ _ _   _ _ __ _   ___  __\033[0m\n");
    vga_print("\033[96m / _` | | | | '__| | | \\ \\/ /\033[0m\n");
    vga_print("\033[96m| (_| | |_| | |  | |_| |>  < \033[0m\n");
    vga_print("\033[96m \\__,_|\\__,_|_|   \\__,_/_/\\_\\\033[0m\n\n");
    
    vga_print("Booting aurux v0.7...\n");
    
    int fd = fs_open("message.txt");
    if (fd >= 0) {
        char buffer[512] = {0};
        int bytes = fs_read(fd, buffer, 511);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            vga_print("Read from disk: ");
            vga_print(buffer);
            vga_print("\n");
        } else {
            vga_print("Failed to read from disk!\n");
        }
        fs_close(fd);
    } else {
        vga_print("Failed to open file on disk!\n");
    }
    
    vga_print("Starting User Space Shell...\n");
    elf_load("shell.elf");
    
    // Shell execution is temporarily bypassed to verify thread functionality.
    // shell_run();
    
    while (1) {
        yield();
        asm volatile("hlt");
    }
}
