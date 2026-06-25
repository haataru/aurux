# Makefile for turbanOS kernel
# Uses i686 cross-compiler

CC = i686-linux-gnu-gcc
AS = /opt/i686-elf/bin/as
LD = /opt/i686-elf/bin/ld

CFLAGS = -ffreestanding -nostdlib -Wall -Wextra -Isrc -std=gnu99 -g -m32
ASFLAGS = --32
LDFLAGS = -T link.ld -Map=kernel.map -m elf_i386

OBJS = src/kernel/main.o \
       src/kernel/interrupts.o \
       src/drivers/vga/vga.o \
       src/drivers/keyboard/keyboard.o \
       src/drivers/rtc/rtc.o \
       src/memory/memory.o \
       src/fs/fs.o \
       src/shell/shell.o \
       src/shell/commands.o \
       src/lib/string.o \
       src/lib/stdio.o

KERNEL = kernel.bin
ASM_OBJ = boot.o

.PHONY: all clean run

all: $(KERNEL)

$(ASM_OBJ): boot.asm
	$(AS) $(ASFLAGS) -o $@ $<

src/kernel/main.o: src/kernel/main.c src/kernel/kernel.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/kernel/interrupts.o: src/kernel/interrupts.c src/kernel/kernel.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/drivers/vga/vga.o: src/drivers/vga/vga.c src/drivers/vga/vga.h src/kernel/kernel.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/drivers/keyboard/keyboard.o: src/drivers/keyboard/keyboard.c src/drivers/keyboard/keyboard.h src/kernel/kernel.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/drivers/rtc/rtc.o: src/drivers/rtc/rtc.c src/drivers/rtc/rtc.h src/kernel/kernel.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/memory/memory.o: src/memory/memory.c src/memory/memory.h src/kernel/kernel.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/fs/fs.o: src/fs/fs.c src/fs/fs.h src/memory/memory.h src/lib/lib.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/shell/shell.o: src/shell/shell.c src/shell/shell.h src/kernel/kernel.h src/drivers/vga/vga.h src/drivers/keyboard/keyboard.h src/lib/lib.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/shell/commands.o: src/shell/commands.c src/shell/shell.h src/kernel/kernel.h src/drivers/vga/vga.h src/drivers/rtc/rtc.h src/fs/fs.h src/memory/memory.h src/lib/lib.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/lib/string.o: src/lib/string.c src/lib/lib.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/lib/stdio.o: src/lib/stdio.c src/lib/lib.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERNEL): $(OBJS) $(ASM_OBJ) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(ASM_OBJ)

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

clean:
	rm -f $(OBJS) $(ASM_OBJ) $(KERNEL) kernel.map
