# Makefile for aurux kernel
# Uses i686 cross-compiler

CC ?= i686-linux-gnu-gcc
AS ?= i686-linux-gnu-as
LD ?= i686-linux-gnu-ld

MKFS_FAT ?= mkfs.vfat

CFLAGS = -ffreestanding -nostdlib -Wall -Wextra -Werror -Isrc -std=gnu99 -g -m32
ASFLAGS = --32 --fatal-warnings
LDFLAGS = -T src/kernel/link.ld -Map=$(BUILD_DIR)/kernel.map -m elf_i386

BUILD_DIR = build

# Source files
KERNEL_C_SRCS = $(filter-out src/lib/syscalls.c src/lib/crt0.c src/lib/crypto.c src/lib/pwd.c src/lib/grp.c, $(wildcard src/kernel/*.c src/drivers/*/*.c src/memory/*.c src/fs/*.c src/fs/*/*.c src/shell/*.c src/lib/*.c))
USER_LIB_SRCS = src/lib/string.c src/lib/malloc.c src/lib/syscalls.c src/lib/crt0.c src/lib/crypto.c src/lib/pwd.c src/lib/grp.c
USER_BIN_C_SRCS = $(wildcard src/user/bin/*.c) src/user/echo_args.c src/user/hello.c

# Object files (in build/)
KERNEL_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/src/%.o, $(KERNEL_C_SRCS))
USER_LIB_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/src/%.o, $(USER_LIB_SRCS))
USER_BIN_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/src/%.o, $(USER_BIN_C_SRCS))

ASM_OBJ = $(BUILD_DIR)/src/boot/boot.o
KERNEL = $(BUILD_DIR)/kernel.bin
IMAGE = $(BUILD_DIR)/aurux.img
SHELL_ELF = $(BUILD_DIR)/shell.elf

# Output ELFs
USER_BIN_ELFS = $(patsubst src/%.c, $(BUILD_DIR)/bin/%.elf, $(USER_BIN_C_SRCS))

.PHONY: all clean run dirs check debug

all: dirs $(IMAGE)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/bin
	@mkdir -p $(sort $(dir $(KERNEL_OBJS) $(USER_LIB_OBJS) $(USER_BIN_OBJS) $(ASM_OBJ)))

$(IMAGE): $(KERNEL) $(SHELL_ELF) $(USER_BIN_ELFS)
	@rm -f $(IMAGE)
	@mkdir -p build
	dd if=/dev/zero of=$(IMAGE) bs=1M count=64
	$(MKFS_FAT) -F 32 $(IMAGE)
	mmd -i $(IMAGE) ::/bin || true
	mmd -i $(IMAGE) ::/etc || true
	mmd -i $(IMAGE) ::/root || true
	mmd -i $(IMAGE) ::/home || true
	mcopy -o -i $(IMAGE) src/etc/passwd ::/etc/passwd
	mcopy -o -i $(IMAGE) src/etc/groups ::/etc/groups
	echo "Hello from FAT32 real disk!" > $(BUILD_DIR)/message.txt
	mcopy -o -i $(IMAGE) $(BUILD_DIR)/message.txt ::message.txt
	mcopy -o -i $(IMAGE) $(SHELL_ELF) ::shell.elf
	mcopy -o -i $(IMAGE) $(SHELL_ELF) ::/bin/shell.elf
	for elf in $(USER_BIN_ELFS); do \
		mcopy -o -i $(IMAGE) $$elf ::/bin/; \
	done

$(KERNEL): $(KERNEL_OBJS) $(ASM_OBJ) src/kernel/link.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(ASM_OBJ)

$(SHELL_ELF): $(BUILD_DIR)/src/user/shell.o $(USER_LIB_OBJS) src/user/user_link.ld
	$(LD) -m elf_i386 -T src/user/user_link.ld -o $@ $< $(USER_LIB_OBJS)

$(BUILD_DIR)/bin/%.elf: $(BUILD_DIR)/src/%.o $(USER_LIB_OBJS) src/user/user_link.ld
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -T src/user/user_link.ld -o $@ $< $(USER_LIB_OBJS)

$(BUILD_DIR)/src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/src/boot/boot.o: src/boot/boot.asm
	$(AS) $(ASFLAGS) -o $@ $<

QEMU_FLAGS ?=

run: $(IMAGE)
	qemu-system-i386 $(QEMU_FLAGS) -kernel $(KERNEL) -hda $(IMAGE)

debug: $(IMAGE)
	qemu-system-i386 -s -S $(QEMU_FLAGS) -kernel $(KERNEL) -hda $(IMAGE)

check:
	@echo "Checking compilation and linkage (-Werror)..."
	@$(MAKE) clean && $(MAKE) all
	@echo "Checking for undefined symbols in ELF files..."
	@for f in $(KERNEL) $(SHELL_ELF) $(USER_BIN_ELFS); do \
		if nm $$f | grep -q " U "; then echo "Undefined symbol in $$f"; nm $$f | grep " U "; exit 1; fi; \
	done
	@echo "Running Sparse on all C files..."
	@for file in $$(find src -name "*.c"); do \
		echo "Checking $$file"; \
		sparse -m32 -Wbitwise -Wcontext -Wdecl -Werror -Isrc -I$$($(CC) -print-file-name=include) $$file || exit 1; \
	done
	@echo "All static checks passed perfectly."


clean:
	rm -rf $(BUILD_DIR)
