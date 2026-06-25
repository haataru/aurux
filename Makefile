# Makefile for aurux kernel
# Uses i686 cross-compiler

CC = i686-linux-gnu-gcc
AS = i686-linux-gnu-as
LD = i686-linux-gnu-ld

CFLAGS = -ffreestanding -nostdlib -Wall -Wextra -Isrc -std=gnu99 -g -m32
ASFLAGS = --32
LDFLAGS = -T src/kernel/link.ld -Map=build/kernel.map -m elf_i386

BUILD_DIR = build

# Source files
KERNEL_C_SRCS = $(wildcard src/kernel/*.c src/drivers/*/*.c src/memory/*.c src/fs/*.c src/fs/*/*.c src/shell/*.c src/lib/*.c)
USER_SHELL_C_SRCS = src/user/shell.c src/lib/string.c

# Object files (in build/)
KERNEL_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/src/%.o, $(KERNEL_C_SRCS))
USER_SHELL_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/src/%.o, $(USER_SHELL_C_SRCS))

ASM_OBJ = $(BUILD_DIR)/src/boot/boot.o
KERNEL = $(BUILD_DIR)/kernel.bin
IMAGE = $(BUILD_DIR)/aurux.img
SHELL_ELF = $(BUILD_DIR)/SHELL.ELF

.PHONY: all clean run dirs

all: dirs $(IMAGE)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(sort $(dir $(KERNEL_OBJS) $(USER_SHELL_OBJS) $(ASM_OBJ)))

$(IMAGE): $(KERNEL) $(SHELL_ELF)
	if [ ! -f $(IMAGE) ]; then \
		dd if=/dev/zero of=$(IMAGE) bs=1M count=64; \
		mkfs.vfat -F 32 $(IMAGE); \
	fi
	echo "Hello from FAT32 real disk!" > $(BUILD_DIR)/message.txt
	mcopy -o -i $(IMAGE) $(BUILD_DIR)/message.txt ::MESSAGE.TXT
	mcopy -o -i $(IMAGE) $(SHELL_ELF) ::SHELL.ELF

$(KERNEL): $(KERNEL_OBJS) $(ASM_OBJ) src/kernel/link.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(ASM_OBJ)

$(SHELL_ELF): $(USER_SHELL_OBJS) src/user/user_link.ld
	$(LD) -m elf_i386 -T src/user/user_link.ld -o $@ $(USER_SHELL_OBJS)

$(BUILD_DIR)/src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/src/boot/boot.o: src/boot/boot.asm
	$(AS) $(ASFLAGS) -o $@ $<

run: $(IMAGE)
	qemu-system-i386 -kernel $(KERNEL) -hda $(IMAGE)

clean:
	rm -rf $(BUILD_DIR)
