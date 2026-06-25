/*
 * fs.c - In-memory filesystem implementation
 */

#include "fs.h"
#include "../memory/memory.h"
#include "../lib/lib.h"

/* Filesystem entries storage */
static fs_entry_t entries[FS_MAX_FILES];

/* Current working directory index (0 = root) */
static int cwd = 0;

/* Find empty entry */
static int find_empty_entry(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!entries[i].used) {
            return i;
        }
    }
    return -1;
}

/* Find entry by name in directory */
static int find_entry_in_dir(int dir_idx, const char* name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (entries[i].used && entries[i].parent == dir_idx) {
            if (strcmp(entries[i].name, name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

/* Parse path and return entry index */
static int parse_path(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    
    /* Handle "." as current directory */
    if (strcmp(path, ".") == 0) {
        return cwd;
    }
    
    /* Handle ".." as parent directory */
    if (strcmp(path, "..") == 0) {
        if (cwd == 0) {
            return 0;  /* Root has no parent */
        }
        return entries[cwd].parent;
    }
    
    /* Start from current directory for relative paths */
    int current = cwd;
    
    if (path[0] == '/') {
        current = 0;
        path++;
    }
    
    /* Handle empty path (means current directory) */
    if (path[0] == '\0') {
        return current;
    }
    
    /* Parse path components */
    char component[FS_MAX_NAME_LENGTH];
    int comp_len = 0;
    
    while (*path) {
        if (*path == '/') {
            if (comp_len > 0) {
                component[comp_len] = '\0';
                int next = find_entry_in_dir(current, component);
                if (next < 0 || entries[next].type != FS_TYPE_DIRECTORY) {
                    return -1;
                }
                current = next;
                comp_len = 0;
            }
            path++;
        } else {
            if (comp_len < FS_MAX_NAME_LENGTH - 1) {
                component[comp_len++] = *path;
            }
            path++;
        }
    }
    
    /* Handle last component */
    if (comp_len > 0) {
        component[comp_len] = '\0';
        int next = find_entry_in_dir(current, component);
        if (next < 0) {
            return -1;
        }
        current = next;
    }
    
    return current;
}

/* Get parent directory index */
static int get_parent_idx(const char* path) {
    /* Find last slash */
    const char* last_slash = NULL;
    const char* p = path;
    
    while (*p) {
        if (*p == '/') {
            last_slash = p;
        }
        p++;
    }
    
    if (last_slash == NULL) {
        /* No slash - it's in current directory */
        return cwd;
    }
    
    if (last_slash == path) {
        /* Path starts with / - parent is root */
        return 0;
    }
    
    /* Extract parent path */
    char parent_path[256];
    int len = last_slash - path;
    for (int i = 0; i < len && i < 255; i++) {
        parent_path[i] = path[i];
    }
    parent_path[len] = '\0';
    
    return parse_path(parent_path);
}

/* Get entry name from path */
static const char* get_name_from_path(const char* path) {
    const char* last_slash = NULL;
    const char* p = path;
    
    while (*p) {
        if (*p == '/') {
            last_slash = p;
        }
        p++;
    }
    
    if (last_slash == NULL) {
        return path;
    }
    
    return last_slash + 1;
}

/*
 * Initialize filesystem
 */
void fs_init(void) {
    /* Clear all entries */
    for (int i = 0; i < FS_MAX_FILES; i++) {
        entries[i].used = 0;
        entries[i].name[0] = '\0';
    }
    
    /* Create root directory */
    entries[0].used = 1;
    entries[0].type = FS_TYPE_DIRECTORY;
    entries[0].name[0] = '/';
    entries[0].name[1] = '\0';
    entries[0].parent = 0;
    entries[0].size = 0;
    
    cwd = 0;
}

/*
 * Create a file or directory
 */
int fs_create(const char* path, unsigned char type) {
    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    
    /* Check if already exists */
    int existing = parse_path(path);
    if (existing >= 0) {
        return -1;  /* Already exists */
    }
    
    /* Find empty entry */
    int idx = find_empty_entry();
    if (idx < 0) {
        return -1;  /* No space */
    }
    
    /* Get parent directory */
    int parent_idx = get_parent_idx(path);
    if (parent_idx < 0) {
        return -1;
    }
    
    /* Get entry name */
    const char* name = get_name_from_path(path);
    int name_len = 0;
    while (name[name_len] && name_len < FS_MAX_NAME_LENGTH - 1) {
        name_len++;
    }
    
    /* Create entry */
    entries[idx].used = 1;
    entries[idx].type = type;
    entries[idx].parent = parent_idx;
    entries[idx].size = 0;
    
    for (int i = 0; i < name_len && i < FS_MAX_NAME_LENGTH - 1; i++) {
        entries[idx].name[i] = name[i];
    }
    entries[idx].name[name_len] = '\0';
    
    return 0;
}

/*
 * Delete a file or directory
 */
int fs_delete(const char* path) {
    int idx = parse_path(path);
    if (idx < 0) {
        return -1;
    }
    
    /* Can't delete root */
    if (idx == 0) {
        return -1;
    }
    
    /* For directories, check if empty */
    if (entries[idx].type == FS_TYPE_DIRECTORY) {
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (entries[i].used && entries[i].parent == idx) {
                return -1;  /* Directory not empty */
            }
        }
    }
    
    entries[idx].used = 0;
    return 0;
}

/*
 * Read from a file
 */
int fs_read(const char* path, char* buf, size_t size) {
    int idx = parse_path(path);
    if (idx < 0) {
        return -1;
    }
    
    if (entries[idx].type != FS_TYPE_FILE) {
        return -1;
    }
    
    unsigned int file_size = entries[idx].size;
    unsigned int read_size = (size < file_size) ? size : file_size;
    
    for (unsigned int i = 0; i < read_size; i++) {
        buf[i] = entries[idx].data[i];
    }
    
    return read_size;
}

/*
 * Write to a file
 */
int fs_write(const char* path, const char* data, size_t size) {
    int idx = parse_path(path);
    if (idx < 0) {
        return -1;
    }
    
    if (entries[idx].type != FS_TYPE_FILE) {
        return -1;
    }
    
    unsigned int write_size = (size < FS_MAX_FILE_SIZE) ? size : FS_MAX_FILE_SIZE;
    
    for (unsigned int i = 0; i < write_size; i++) {
        entries[idx].data[i] = data[i];
    }
    
    entries[idx].size = write_size;
    return write_size;
}

/*
 * List directory contents
 */
int fs_list(const char* path, char* output, size_t output_size) {
    int idx = parse_path(path);
    if (idx < 0) {
        return -1;
    }
    
    if (entries[idx].type != FS_TYPE_DIRECTORY) {
        return -1;
    }
    
    output[0] = '\0';
    size_t pos = 0;
    
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (entries[i].used && entries[i].parent == idx) {
            /* Check if we have enough space */
            int name_len = 0;
            while (entries[i].name[name_len]) name_len++;
            
            if (pos + name_len + 2 < output_size) {
                /* Copy name */
                for (int j = 0; j < name_len; j++) {
                    output[pos++] = entries[i].name[j];
                }
                
                /* Add indicator for directories */
                if (entries[i].type == FS_TYPE_DIRECTORY) {
                    output[pos++] = '/';
                }
                
                output[pos++] = ' ';
            }
        }
    }
    
    if (pos > 0 && pos < output_size) {
        output[pos - 1] = '\0';
    }
    
    return 0;
}

/*
 * Change current directory
 */
int fs_change_dir(const char* path) {
    int idx = parse_path(path);
    if (idx < 0) {
        return -1;
    }
    
    if (entries[idx].type != FS_TYPE_DIRECTORY) {
        return -1;
    }
    
    cwd = idx;
    return 0;
}

/*
 * Get current working directory
 */
const char* fs_get_cwd(void) {
    /* Return path for current directory */
    static char path[256];
    
    if (cwd == 0) {
        return "/";
    }
    
    /* Build path by traversing up */
    int idx = cwd;
    int depth = 0;
    int stack[FS_MAX_DEPTH];
    
    while (idx != 0 && depth < FS_MAX_DEPTH) {
        stack[depth++] = idx;
        idx = entries[idx].parent;
    }
    
    path[0] = '\0';
    
    /* Build path from root to current */
    for (int i = depth - 1; i >= 0; i--) {
        int len = 0;
        while (entries[stack[i]].name[len]) {
            len++;
        }
        
        /* Add slash */
        int pos = 0;
        while (path[pos]) pos++;
        path[pos] = '/';
        path[pos + 1] = '\0';
        
        /* Add name */
        for (int j = 0; j < len && pos + 1 + j < 255; j++) {
            path[pos + 1 + j] = entries[stack[i]].name[j];
        }
        /* Null terminate */
        path[pos + 1 + len] = '\0';
    }
    
    if (path[0] == '\0') {
        path[0] = '/';
        path[1] = '\0';
    }
    
    return path;
}

/*
 * Check if path exists
 */
int fs_exists(const char* path) {
    return parse_path(path) >= 0;
}

/*
 * Get file/directory type
 */
int fs_get_type(const char* path) {
    int idx = parse_path(path);
    if (idx < 0) {
        return -1;
    }
    return entries[idx].type;
}
