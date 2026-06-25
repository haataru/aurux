#include "fs.h"
#include "../lib/lib.h"
#include "fat32/fat32.h"
#include "../memory/memory.h"
#include "pipe.h"
#include "../kernel/task.h"

static char current_path[256];

static global_file_descriptor_t global_fd_table[MAX_OPEN_FILES];
static struct mount_point mount_points[MAX_MOUNT_POINTS];

int fs_mount(const char* prefix, struct fs_driver* driver) {
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (!mount_points[i].used) {
            mount_points[i].used = 1;
            strcpy(mount_points[i].prefix, prefix);
            mount_points[i].driver = driver;
            return 0;
        }
    }
    return -1;
}

struct fs_driver* fs_get_driver(const char* path, const char** out_relative_path) {
    int best_match = -1;
    int best_match_len = -1;
    
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        if (mount_points[i].used) {
            int len = strlen(mount_points[i].prefix);
            if (strncmp(path, mount_points[i].prefix, len) == 0) {
                if (path[len] == '/' || path[len] == '\0' || len == 1) {
                    if (len > best_match_len) {
                        best_match = i;
                        best_match_len = len;
                    }
                }
            }
        }
    }
    
    if (best_match >= 0) {
        if (out_relative_path) {
            *out_relative_path = path + best_match_len;
            if (**out_relative_path == '/') (*out_relative_path)++;
        }
        return mount_points[best_match].driver;
    }
    
    return NULL;
}


static int fat_open(const char* path) {
    if (fat32_get_file_size(path) >= 0) return 0;
    return -1;
}
static int fat_read(const char* path, char* buf, size_t size, unsigned int offset) {
    int file_size = fat32_get_file_size(path);
    if (file_size < 0) return -1;
    
    int to_read = size;
    if (offset >= (unsigned int)file_size) to_read = 0;
    else if (offset + size > (unsigned int)file_size) to_read = file_size - offset;
    
    if (to_read <= 0) return 0;
    
    int read = fat32_read_file_ex(path, (unsigned char*)buf, to_read, offset);
    if (read < 0) return -1;
    return read;
}
static int fat_write(const char* path, const char* data, size_t size, unsigned int offset) {
    int file_size = fat32_get_file_size(path);
    if (file_size < 0) file_size = 0;
    
    unsigned int new_size = file_size;
    if (offset + size > (unsigned int)file_size) new_size = offset + size;
    
    unsigned char* temp_buf = (unsigned char*)kmalloc(new_size + 512);
    if (!temp_buf) return -1;
    
    if (file_size > 0) {
        fat32_read_file(path, temp_buf);
    }
    
    for(size_t i = 0; i < size; i++) temp_buf[offset + i] = data[i];
    
    int written = fat32_write_file(path, temp_buf, new_size);
    kfree(temp_buf);
    
    if (written >= 0) return size;
    return -1;
}
static int fat_create_file(const char* path) { return fat32_create_file(path, 0); }
static int fat_create_dir(const char* path) { return fat32_create_file(path, 0x10); }
static int fat_delete_file(const char* path) { return fat32_delete_file(path); }
static int fat_list(const char* path, char* output, unsigned int output_size, int detailed) {
    return fat32_list_dir(path, output, output_size, detailed);
}
static int fat_get_size(const char* path) { return fat32_get_file_size(path); }
static int fat_is_dir(const char* path) { return fat32_is_dir(path); }

static struct fs_driver fat32_fs_driver = {
    .open = fat_open,
    .read = fat_read,
    .write = fat_write,
    .create_file = fat_create_file,
    .create_dir = fat_create_dir,
    .delete_file = fat_delete_file,
    .list_dir = fat_list,
    .get_size = fat_get_size,
    .is_dir = fat_is_dir
};

extern void devfs_init(void);

void fs_init(void) {
    fat32_init();
    
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) mount_points[i].used = 0;
    
    strcpy(current_path, "/");
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        global_fd_table[i].used = 0;
        global_fd_table[i].refcount = 0;
        global_fd_table[i].internal_data = 0;
        global_fd_table[i].mode = 0;
    }
    
    fs_mount("/", &fat32_fs_driver);
    devfs_init();
}

int fs_pipe(int fd_array[2]) {
    extern struct fs_driver pipe_fs_driver;
    extern struct task* current_task;
    
    int g_r = -1, g_w = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!global_fd_table[i].used) {
            if (g_r == -1) g_r = i;
            else if (g_w == -1) { g_w = i; break; }
        }
    }
    if (g_r == -1 || g_w == -1) return -1;
    
    int l_r = -1, l_w = -1;
    for (int i = 0; i < 16; i++) {
        if (!current_task->fd_table[i]) {
            if (l_r == -1) l_r = i;
            else if (l_w == -1) { l_w = i; break; }
        }
    }
    if (l_r == -1 || l_w == -1) return -1;
    
    pipe_t* p = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!p) return -1;
    p->read_pos = 0;
    p->write_pos = 0;
    p->bytes_available = 0;
    p->readers = 1;
    p->writers = 1;
    
    global_fd_table[g_r].used = 1;
    global_fd_table[g_r].refcount = 1;
    global_fd_table[g_r].offset = (unsigned int)p;
    global_fd_table[g_r].driver = &pipe_fs_driver;
    global_fd_table[g_r].internal_data = p;
    global_fd_table[g_r].mode = 1; // read-only
    strcpy(global_fd_table[g_r].path, "pipe");
    
    global_fd_table[g_w].used = 1;
    global_fd_table[g_w].refcount = 1;
    global_fd_table[g_w].offset = (unsigned int)p;
    global_fd_table[g_w].driver = &pipe_fs_driver;
    global_fd_table[g_w].internal_data = p;
    global_fd_table[g_w].mode = 2; // write-only
    strcpy(global_fd_table[g_w].path, "pipe");
    
    current_task->fd_table[l_r] = &global_fd_table[g_r];
    current_task->fd_table[l_w] = &global_fd_table[g_w];
    
    fd_array[0] = l_r;
    fd_array[1] = l_w;
    
    return 0;
}

// Resolves a path against the current working directory.
void fs_resolve_path(const char* input_path, char* absolute_path) {
    if (input_path[0] == '/') {
        strcpy(absolute_path, input_path);
    } else {
        strcpy(absolute_path, current_path);
        if (absolute_path[strlen(absolute_path) - 1] != '/') {
            strcat(absolute_path, "/");
        }
        strcat(absolute_path, input_path);
    }
    
    // Normalize the absolute path to handle . and .. directories.
    char temp[256];
    int t_idx = 0;
    temp[0] = '/';
    temp[1] = '\0';
    t_idx = 1;
    
    int i = 0;
    while (absolute_path[i] != '\0') {
        if (absolute_path[i] == '/') {
            i++;
            continue;
        }
        
        char token[64];
        int tok_idx = 0;
        while (absolute_path[i] != '\0' && absolute_path[i] != '/') {
            token[tok_idx++] = absolute_path[i++];
        }
        token[tok_idx] = '\0';
        
        if (strcmp(token, ".") == 0) {
            continue;
        } else if (strcmp(token, "..") == 0) {
            if (t_idx > 1) {
                t_idx--;
                while (t_idx > 0 && temp[t_idx-1] != '/') t_idx--;
            }
        } else {
            for(int k=0; k<tok_idx; k++) {
                temp[t_idx++] = token[k];
            }
            temp[t_idx++] = '/';
        }
    }
    
    if (t_idx > 1) {
        temp[t_idx-1] = '\0'; // Remove trailing slash unless it is just the root directory.
    } else {
        temp[1] = '\0';
    }
    
    strcpy(absolute_path, temp);
}

int fs_open(const char* path) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    if (!driver) return -1;
    
    if (driver->open(rel_path) < 0) return -1;
    
    extern struct task* current_task;
    int l_fd = -1;
    for (int i = 0; i < 16; i++) {
        if (!current_task->fd_table[i]) {
            l_fd = i;
            break;
        }
    }
    if (l_fd == -1) return -1;
    
    int g_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!global_fd_table[i].used) {
            g_fd = i;
            break;
        }
    }
    if (g_fd == -1) return -1;
    
    global_fd_table[g_fd].used = 1;
    global_fd_table[g_fd].refcount = 1;
    strcpy(global_fd_table[g_fd].path, rel_path);
    global_fd_table[g_fd].offset = 0;
    global_fd_table[g_fd].driver = driver;
    global_fd_table[g_fd].internal_data = 0;
    global_fd_table[g_fd].mode = 0;
    
    current_task->fd_table[l_fd] = &global_fd_table[g_fd];
    return l_fd;
}

int fs_read(int fd, char* buf, size_t size) {
    extern struct task* current_task;
    if (fd < 0 || fd >= 16 || !current_task->fd_table[fd]) return -1;
    global_file_descriptor_t* gfd = (global_file_descriptor_t*)current_task->fd_table[fd];
    
    struct fs_driver* driver = gfd->driver;
    if (!driver) return -1;
    
    int bytes = driver->read(gfd->path, buf, size, gfd->offset);
    extern struct fs_driver pipe_fs_driver;
    if (bytes > 0 && driver != &pipe_fs_driver) gfd->offset += bytes;
    return bytes;
}

int fs_write(int fd, const char* data, size_t size) {
    extern struct task* current_task;
    if (fd < 0 || fd >= 16 || !current_task->fd_table[fd]) return -1;
    global_file_descriptor_t* gfd = (global_file_descriptor_t*)current_task->fd_table[fd];
    struct fs_driver* driver = gfd->driver;
    
    int bytes = driver->write(gfd->path, data, size, gfd->offset);
    extern struct fs_driver pipe_fs_driver;
    if (bytes > 0 && driver != &pipe_fs_driver) gfd->offset += bytes;
    return bytes;
}

int fs_seek(int fd, unsigned int offset) {
    extern struct task* current_task;
    if (fd < 0 || fd >= 16 || !current_task->fd_table[fd]) return -1;
    global_file_descriptor_t* gfd = (global_file_descriptor_t*)current_task->fd_table[fd];
    gfd->offset = offset;
    return 0;
}

int fs_close(int fd) {
    extern struct task* current_task;
    if (fd < 0 || fd >= 16 || !current_task->fd_table[fd]) return -1;
    global_file_descriptor_t* gfd = (global_file_descriptor_t*)current_task->fd_table[fd];
    
    current_task->fd_table[fd] = 0;
    
    gfd->refcount--;
    if (gfd->refcount <= 0) {
        if (gfd->driver && gfd->driver->close) {
            gfd->driver->close(gfd->offset, gfd->internal_data, gfd->mode);
        }
        gfd->used = 0;
    }
    return 0;
}

int fs_dup2(int oldfd, int newfd) {
    extern struct task* current_task;
    if (oldfd < 0 || oldfd >= 16 || !current_task->fd_table[oldfd]) return -1;
    if (newfd < 0 || newfd >= 16) return -1;
    
    if (oldfd == newfd) return newfd;
    
    if (current_task->fd_table[newfd]) {
        fs_close(newfd);
    }
    
    global_file_descriptor_t* gfd = (global_file_descriptor_t*)current_task->fd_table[oldfd];
    gfd->refcount++;
    current_task->fd_table[newfd] = gfd;
    
    return newfd;
}

int fs_create_file(const char* path) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    if (!driver) return -1;
    return driver->create_file(rel_path);
}

int fs_create_dir(const char* path) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    if (!driver) return -1;
    return driver->create_dir(rel_path);
}

int fs_delete(const char* path) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    if (!driver) return -1;
    return driver->delete_file(rel_path);
}

int fs_list(const char* path, char* output, unsigned int output_size, int detailed) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    
    output[0] = '\0';
    
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    
    if (strcmp(abs_path, "/") == 0) {
        if (detailed) {
            strcat(output, "2026-01-01 00:00  0  dev/\n");
        } else {
            strcat(output, "dev/ ");
        }
    }
    
    if (driver && driver->list_dir) {
        char drv_out[2048];
        drv_out[0] = '\0';
        int res = driver->list_dir(rel_path, drv_out, 2048, detailed);
        if (res >= 0) {
            if (strlen(output) + strlen(drv_out) < output_size) {
                strcat(output, drv_out);
            }
            return 0;
        }
    }
    
    if (output[0] != '\0') return 0;
    return -1;
}

int fs_change_dir(const char* path) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    if (!driver) return -1;
    
    if (driver->is_dir(rel_path)) {
        strcpy(current_path, abs_path);
        return 0;
    }
    return -1;
}

const char* fs_get_cwd(void) {
    return current_path;
}

int fs_exists(const char* path) {
    char abs_path[256];
    fs_resolve_path(path, abs_path);
    const char* rel_path;
    struct fs_driver* driver = fs_get_driver(abs_path, &rel_path);
    if (!driver) return -1;
    return (driver->get_size(rel_path) >= 0);
}
