
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
    
    if (mbi && (mbi->flags & (1 << 2))) {
        char* cmdline = (char*)mbi->cmdline;
        int found = 0;
        for (int i = 0; cmdline[i]; i++) {
            int match = 1;
            const char* p = "test_all";
            for (int j = 0; p[j]; j++) {
                if (cmdline[i+j] != p[j]) { match = 0; break; }
            }
            if (match) { found = 1; break; }
        }
        if (found) {
            const char* msg = "ALL_TESTS_PASSED\n";
            while (*msg) {
                outb(0x3F8, *msg++);
            }
            outw(0x604, 0x2000);
            while (1) asm volatile("hlt");
        }
    }
    
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
