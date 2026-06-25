#include "devfs.h"
#include "../../lib/lib.h"
#include "../../drivers/vga/vga.h"

static int devfs_open(const char* path) {
    if (strcmp(path, "tty0") == 0) return 0;
    if (strcmp(path, "null") == 0) return 0;
    return -1;
}

static int devfs_read(const char* path, char* buf, size_t size, unsigned int offset) {
    (void)offset;
    if (strcmp(path, "tty0") == 0) {
        extern char keyboard_getchar(void);
        for (size_t i = 0; i < size; i++) {
            char c = keyboard_getchar();
            buf[i] = c;
            if (c == '\n') return i + 1;
        }
        return size;
    }
    if (strcmp(path, "null") == 0) {
        return 0; // EOF
    }
    return -1;
}

static int devfs_write(const char* path, const char* data, size_t size, unsigned int offset) {
    (void)offset;
    if (strcmp(path, "tty0") == 0) {
        extern void* kmalloc(unsigned int size);
        extern void kfree(void* ptr);
        char* temp = (char*)kmalloc(size + 1);
        if (temp) {
            for (size_t i = 0; i < size; i++) temp[i] = data[i];
            temp[size] = '\0';
            vga_print(temp);
            kfree(temp);
        }
        return size;
    }
    if (strcmp(path, "null") == 0) {
        return size;
    }
    return -1;
}

static int devfs_create_file(const char* path) {
    (void)path;
    return -1;
}

static int devfs_create_dir(const char* path) {
    (void)path;
    return -1;
}

static int devfs_delete_file(const char* path) {
    (void)path;
    return -1;
}

static int devfs_list_dir(const char* path, char* output, unsigned int output_size, int detailed) {
    (void)output_size;
    if (path[0] == '\0' || strcmp(path, "/") == 0) {
        if (detailed) {
            strcat(output, "2026-01-01 00:00  0  tty0\n");
            strcat(output, "2026-01-01 00:00  0  null\n");
        } else {
            strcat(output, "tty0 null ");
        }
        return 0;
    }
    return -1;
}

static int devfs_get_size(const char* path) {
    if (strcmp(path, "tty0") == 0 || strcmp(path, "null") == 0) return 0;
    return -1;
}

static int devfs_is_dir(const char* path) {
    if (path[0] == '\0' || strcmp(path, "/") == 0) return 1;
    return 0;
}

struct fs_driver devfs_driver = {
    .open = devfs_open,
    .read = devfs_read,
    .write = devfs_write,
    .create_file = devfs_create_file,
    .create_dir = devfs_create_dir,
    .delete_file = devfs_delete_file,
    .list_dir = devfs_list_dir,
    .get_size = devfs_get_size,
    .is_dir = devfs_is_dir
};

void devfs_init(void) {
    fs_mount("/dev", &devfs_driver);
}
