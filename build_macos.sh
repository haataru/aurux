#!/usr/bin/env bash
# Helper script to build and run the Aurux OS on macOS (M1/M2/M3/M4 Apple Silicon)

export PATH="/opt/homebrew/sbin:$PATH"

# Configuration overrides for the macOS cross-compiler
export CC=i686-elf-gcc
export AS=i686-elf-as
export LD=i686-elf-ld
export MKFS_FAT=/opt/homebrew/sbin/mkfs.fat
export QEMU_FLAGS="-display cocoa,zoom-to-fit=on"

case "$1" in
    run)
        make run
        ;;
    check)
        make check
        ;;
    clean)
        make clean
        ;;
    *)
        make all
        ;;
esac
