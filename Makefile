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
	mke2fs -t ext2 -b 1024 $(IMAGE)
	e2mkdir $(IMAGE):/bin || true
	e2mkdir $(IMAGE):/etc || true
	e2mkdir $(IMAGE):/root || true
	e2mkdir $(IMAGE):/home || true
	e2cp src/etc/passwd $(IMAGE):/etc/passwd
	e2cp src/etc/groups $(IMAGE):/etc/groups
	# Set permissions (root owned)
	# For simplicity, we just copy them.
	echo "Hello from Ext2 real disk!" > $(BUILD_DIR)/message.txt
	e2cp $(BUILD_DIR)/message.txt $(IMAGE):/message.txt
	e2cp $(SHELL_ELF) $(IMAGE):/shell.elf
	debugfs -w -R 'sif /shell.elf mode 0100755' $(IMAGE) || true
	e2cp $(SHELL_ELF) $(IMAGE):/bin/shell.elf
	debugfs -w -R 'sif /bin/shell.elf mode 0100755' $(IMAGE) || true
	for elf in $(USER_BIN_ELFS); do \
		e2cp $$elf $(IMAGE):/bin/; \
		debugfs -w -R "sif /bin/$$(basename $$elf) mode 0100755" $(IMAGE) || true; \
	done
	debugfs -w -R 'sif /bin/su.elf mode 0104755' $(IMAGE) || true
	debugfs -w -R 'sif /bin/passwd.elf mode 0104755' $(IMAGE) || true
	debugfs -w -R 'sif /root mode 040700' $(IMAGE) || true

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
