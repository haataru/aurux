#ifndef FS_H
#define FS_H

#include "../kernel/kernel.h"

/* Types */
#define FS_TYPE_FILE 1
#define FS_TYPE_DIRECTORY 2

/* Entry */
typedef struct fs_entry {
    char name[FS_MAX_NAME_LENGTH];
    unsigned char type;
    unsigned char used;
    unsigned int size;
    int parent;
    unsigned char data[FS_MAX_FILE_SIZE];
} fs_entry_t;

void fs_init(void);
int fs_create(const char* path, unsigned char type);
int fs_delete(const char* path);
int fs_read(const char* path, char* buf, size_t size);
int fs_write(const char* path, const char* data, size_t size);
int fs_list(const char* path, char* output, size_t output_size);
int fs_change_dir(const char* path);
const char* fs_get_cwd(void);
int fs_exists(const char* path);
int fs_get_type(const char* path);

#endif
