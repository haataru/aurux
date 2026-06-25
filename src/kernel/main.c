/* Kernel entry point */
#include "kernel.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/rtc/rtc.h"
#include "../memory/memory.h"
#include "../fs/fs.h"
#include "../shell/shell.h"

void OSmain(void) {
    vga_init();
    memory_init();
    fs_init();
    keyboard_init();
    rtc_init();

    vga_print("Booting turbanOS v0.4...\n");
    vga_print("Initializing subsystems: ");

    for (volatile int i = 0; i < 100000; i++);

    vga_print("[OK]\n");

    shell_run();

    while (1) {
        asm volatile("hlt");
    }
}
