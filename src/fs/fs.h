#ifndef FS_H
#define FS_H

#include "../kernel/kernel.h"


#define FS_TYPE_FILE 1
#define FS_TYPE_DIRECTORY 2

#define MAX_OPEN_FILES 16

struct fs_driver {
    int (*open)(const char* path);
    int (*read)(const char* path, char* buf, size_t size, unsigned int offset);
    int (*write)(const char* path, const char* data, size_t size, unsigned int offset);
    int (*create_file)(const char* path);
    int (*create_dir)(const char* path);
    int (*delete_file)(const char* path);
    int (*list_dir)(const char* path, char* output, unsigned int output_size, int detailed);
    int (*get_size)(const char* path);
    int (*is_dir)(const char* path);
    int (*close)(unsigned int offset, void* internal_data, int mode);
};

#define MAX_MOUNT_POINTS 8

struct mount_point {
    int used;
    char prefix[64];
    struct fs_driver* driver;
};

typedef struct {
    int used;
    char path[256];
    unsigned int offset;
    struct fs_driver* driver;
    void* internal_data;
    int mode; // 0 = read/write, 1 = read-only, 2 = write-only
} file_descriptor_t;

int fs_mount(const char* prefix, struct fs_driver* driver);
struct fs_driver* fs_get_driver(const char* path, const char** out_relative_path);

void fs_init(void);


int fs_open(const char* path);
int fs_read(int fd, char* buf, size_t size);
int fs_write(int fd, const char* data, size_t size);
int fs_seek(int fd, unsigned int offset);
int fs_close(int fd);
int fs_pipe(int fd[2]);


int fs_create_file(const char* path);
int fs_create_dir(const char* path);
int fs_delete(const char* path);
int fs_list(const char* path, char* output, unsigned int output_size, int detailed);
int fs_change_dir(const char* path);
const char* fs_get_cwd(void);
int fs_exists(const char* path);
int fs_get_file_size(const char* path);

#endif
