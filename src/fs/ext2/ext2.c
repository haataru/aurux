#include "ext2.h"
#include "../../lib/lib.h"
#include "../../drivers/ata/ata.h"
#include "../../memory/memory.h"
#include "../../drivers/vga/vga.h"
#include "../../kernel/task.h"

extern struct task* current_task;

static ext2_superblock_t sb;
static ext2_bg_descriptor_t* bgd_table = NULL;
static uint32_t block_size;
static uint32_t inodes_per_group;
static uint32_t blocks_per_group;
static uint32_t num_groups;

static int ext2_read_block(uint32_t block, uint8_t* buffer) {
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = block * sectors_per_block;
    return ata_read_sectors(lba, buffer, sectors_per_block);
}

static int ext2_write_block(uint32_t block, const uint8_t* buffer) {
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = block * sectors_per_block;
    return ata_write_sectors(lba, buffer, sectors_per_block);
}

static int ext2_read_inode(uint32_t inode_num, ext2_inode_t* inode) {
    if (inode_num == 0 || inode_num > sb.s_inodes_count) return -1;
    
    uint32_t bg_index = (inode_num - 1) / inodes_per_group;
    uint32_t index_in_bg = (inode_num - 1) % inodes_per_group;
    
    uint32_t inode_table_block = bgd_table[bg_index].bg_inode_table;
    uint32_t inode_size = (sb.s_rev_level >= 1) ? sb.s_inode_size : 128;
    
    uint32_t block_index = (index_in_bg * inode_size) / block_size;
    uint32_t offset_in_block = (index_in_bg * inode_size) % block_size;
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    ext2_read_block(inode_table_block + block_index, block_buf);
    
    memcpy(inode, block_buf + offset_in_block, sizeof(ext2_inode_t));
    kfree(block_buf);
    return 0;
}

static void ext2_write_superblock(void) {
    uint8_t* buf = (uint8_t*)kmalloc(1024);
    memset(buf, 0, 1024);
    memcpy(buf, &sb, sizeof(ext2_superblock_t));
    ata_write_sectors(2, (const unsigned char*)buf, 2);
    kfree(buf);
}

static void ext2_write_bgd(void) {
    uint32_t bgdt_block = (block_size == 1024) ? 2 : 1;
    uint32_t bgdt_sectors = (num_groups * sizeof(ext2_bg_descriptor_t)) / 512 + 1;
    uint8_t* bgdt_buf = (uint8_t*)kmalloc(bgdt_sectors * 512);
    memset(bgdt_buf, 0, bgdt_sectors * 512);
    memcpy(bgdt_buf, bgd_table, num_groups * sizeof(ext2_bg_descriptor_t));
    ata_write_sectors(bgdt_block * (block_size / 512), (const unsigned char*)bgdt_buf, bgdt_sectors);
    kfree(bgdt_buf);
}

static int ext2_write_inode_sync(uint32_t inode_num, ext2_inode_t* inode) {
    if (inode_num == 0 || inode_num > sb.s_inodes_count) return -1;
    
    uint32_t bg_index = (inode_num - 1) / inodes_per_group;
    uint32_t index_in_bg = (inode_num - 1) % inodes_per_group;
    
    uint32_t inode_table_block = bgd_table[bg_index].bg_inode_table;
    uint32_t inode_size = (sb.s_rev_level >= 1) ? sb.s_inode_size : 128;
    
    uint32_t block_index = (index_in_bg * inode_size) / block_size;
    uint32_t offset_in_block = (index_in_bg * inode_size) % block_size;
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    ext2_read_block(inode_table_block + block_index, block_buf);
    
    memcpy(block_buf + offset_in_block, inode, sizeof(ext2_inode_t));
    ext2_write_block(inode_table_block + block_index, block_buf);
    
    kfree(block_buf);
    return 0;
}

static uint32_t ext2_allocate_block(void) {
    if (sb.s_free_blocks_count == 0) return 0;
    for (uint32_t g = 0; g < num_groups; g++) {
        if (bgd_table[g].bg_free_blocks_count > 0) {
            uint8_t* bitmap = (uint8_t*)kmalloc(block_size);
            ext2_read_block(bgd_table[g].bg_block_bitmap, bitmap);
            for (uint32_t byte = 0; byte < block_size; byte++) {
                if (bitmap[byte] != 0xFF) {
                    for (int bit = 0; bit < 8; bit++) {
                        if (!(bitmap[byte] & (1 << bit))) {
                            bitmap[byte] |= (1 << bit);
                            ext2_write_block(bgd_table[g].bg_block_bitmap, bitmap);
                            kfree(bitmap);
                            bgd_table[g].bg_free_blocks_count--;
                            sb.s_free_blocks_count--;
                            ext2_write_bgd();
                            ext2_write_superblock();
                            return g * blocks_per_group + (byte * 8 + bit) + sb.s_first_data_block;
                        }
                    }
                }
            }
            kfree(bitmap);
        }
    }
    return 0;
}

static void ext2_free_block(uint32_t block) {
    if (block < sb.s_first_data_block || block >= sb.s_blocks_count) return;
    uint32_t bg_index = (block - sb.s_first_data_block) / blocks_per_group;
    uint32_t bit_index = (block - sb.s_first_data_block) % blocks_per_group;
    uint8_t* bitmap = (uint8_t*)kmalloc(block_size);
    ext2_read_block(bgd_table[bg_index].bg_block_bitmap, bitmap);
    bitmap[bit_index / 8] &= ~(1 << (bit_index % 8));
    ext2_write_block(bgd_table[bg_index].bg_block_bitmap, bitmap);
    kfree(bitmap);
    bgd_table[bg_index].bg_free_blocks_count++;
    sb.s_free_blocks_count++;
    ext2_write_bgd();
    ext2_write_superblock();
}

static uint32_t ext2_allocate_inode(void) {
    if (sb.s_free_inodes_count == 0) return 0;
    for (uint32_t g = 0; g < num_groups; g++) {
        if (bgd_table[g].bg_free_inodes_count > 0) {
            uint8_t* bitmap = (uint8_t*)kmalloc(block_size);
            ext2_read_block(bgd_table[g].bg_inode_bitmap, bitmap);
            for (uint32_t byte = 0; byte < block_size; byte++) {
                if (bitmap[byte] != 0xFF) {
                    for (int bit = 0; bit < 8; bit++) {
                        if (!(bitmap[byte] & (1 << bit))) {
                            bitmap[byte] |= (1 << bit);
                            ext2_write_block(bgd_table[g].bg_inode_bitmap, bitmap);
                            kfree(bitmap);
                            bgd_table[g].bg_free_inodes_count--;
                            sb.s_free_inodes_count--;
                            ext2_write_bgd();
                            ext2_write_superblock();
                            return g * inodes_per_group + (byte * 8 + bit) + 1;
                        }
                    }
                }
            }
            kfree(bitmap);
        }
    }
    return 0;
}

static void ext2_free_inode(uint32_t inode_num) {
    if (inode_num == 0 || inode_num > sb.s_inodes_count) return;
    uint32_t bg_index = (inode_num - 1) / inodes_per_group;
    uint32_t bit_index = (inode_num - 1) % inodes_per_group;
    uint8_t* bitmap = (uint8_t*)kmalloc(block_size);
    ext2_read_block(bgd_table[bg_index].bg_inode_bitmap, bitmap);
    bitmap[bit_index / 8] &= ~(1 << (bit_index % 8));
    ext2_write_block(bgd_table[bg_index].bg_inode_bitmap, bitmap);
    kfree(bitmap);
    bgd_table[bg_index].bg_free_inodes_count++;
    sb.s_free_inodes_count++;
    ext2_write_bgd();
    ext2_write_superblock();
}

static uint32_t ext2_get_file_block(ext2_inode_t* inode, uint32_t file_block) {
    if (file_block < 12) {
        return inode->i_block[file_block];
    }
    
    if (file_block < 12 + (block_size / 4)) {
        uint32_t indirect_block = inode->i_block[12];
        if (!indirect_block) return 0;
        uint32_t* buf = (uint32_t*)kmalloc(block_size);
        ext2_read_block(indirect_block, (uint8_t*)buf);
        uint32_t res = buf[file_block - 12];
        kfree(buf);
        return res;
    }
    // Doubly and triply indirect are too complex for a basic implementation right now
    return 0;
}

static int ext2_find_in_dir(ext2_inode_t* dir_inode, const char* name, ext2_inode_t* out_inode, uint32_t* out_inode_num) {
    if (!(dir_inode->i_mode & EXT2_S_IFDIR)) return -1;
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    uint32_t blocks = dir_inode->i_size / block_size + (dir_inode->i_size % block_size ? 1 : 0);
    
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t pblock = ext2_get_file_block(dir_inode, b);
        if (!pblock) continue;
        
        ext2_read_block(pblock, block_buf);
        uint32_t offset = 0;
        
        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buf + offset);
            if (entry->inode != 0 && entry->name_len > 0) {
                if (strlen(name) == entry->name_len && strncmp(name, entry->name, entry->name_len) == 0) {
                    uint32_t inum = entry->inode;
                    kfree(block_buf);
                    if (out_inode_num) *out_inode_num = inum;
                    return ext2_read_inode(inum, out_inode);
                }
            }
            if (entry->rec_len == 0) break;
            offset += entry->rec_len;
        }
    }
    
    kfree(block_buf);
    return -1;
}

static int ext2_append_block(ext2_inode_t* inode, uint32_t inode_num) {
    uint32_t new_block = ext2_allocate_block();
    if (!new_block) return -1;
    
    uint8_t* zero_buf = (uint8_t*)kmalloc(block_size);
    memset(zero_buf, 0, block_size);
    ext2_write_block(new_block, zero_buf);
    kfree(zero_buf);
    
    uint32_t block_index = inode->i_size / block_size;
    if (block_index < 12) {
        inode->i_block[block_index] = new_block;
    } else {
        ext2_free_block(new_block);
        return -1; // Indirect blocks not supported for simplicity yet
    }
    
    inode->i_size += block_size;
    inode->i_blocks += block_size / 512;
    ext2_write_inode_sync(inode_num, inode);
    return new_block;
}

static int ext2_add_dir_entry(ext2_inode_t* dir, uint32_t dir_inum, uint32_t inode_num, const char* name, uint8_t type) {
    uint32_t name_len = strlen(name);
    uint32_t needed_len = (8 + name_len + 3) & ~3;
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    uint32_t blocks = dir->i_size / block_size + (dir->i_size % block_size ? 1 : 0);
    
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t pblock = ext2_get_file_block(dir, b);
        if (!pblock) continue;
        
        ext2_read_block(pblock, block_buf);
        uint32_t offset = 0;
        
        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buf + offset);
            uint32_t real_len = 0;
            if (entry->inode != 0) {
                real_len = (8 + entry->name_len + 3) & ~3;
            }
            
            if (entry->rec_len - real_len >= needed_len) {
                uint32_t new_entry_offset = offset + real_len;
                uint16_t old_rec_len = entry->rec_len;
                entry->rec_len = real_len;
                
                ext2_dir_entry_t* new_entry = (ext2_dir_entry_t*)(block_buf + new_entry_offset);
                new_entry->inode = inode_num;
                new_entry->rec_len = old_rec_len - real_len;
                new_entry->name_len = name_len;
                new_entry->file_type = type;
                memcpy(new_entry->name, name, name_len);
                
                ext2_write_block(pblock, block_buf);
                kfree(block_buf);
                return 0;
            }
            
            if (entry->rec_len == 0) break;
            offset += entry->rec_len;
        }
    }
    
    uint32_t new_pblock = ext2_append_block(dir, dir_inum);
    if (new_pblock != 0 && new_pblock != (uint32_t)-1) {
        memset(block_buf, 0, block_size);
        ext2_dir_entry_t* new_entry = (ext2_dir_entry_t*)block_buf;
        new_entry->inode = inode_num;
        new_entry->rec_len = block_size;
        new_entry->name_len = name_len;
        new_entry->file_type = type;
        memcpy(new_entry->name, name, name_len);
        ext2_write_block(new_pblock, block_buf);
        kfree(block_buf);
        return 0;
    }
    
    kfree(block_buf);
    return -1;
}

static int ext2_resolve_path(const char* path, ext2_inode_t* out_inode, uint32_t* out_inode_num) {
    if (path[0] == '/') path++; // Skip leading slash
    
    ext2_inode_t current_inode;
    if (ext2_read_inode(2, &current_inode) < 0) return -1; // Inode 2 is Root
    uint32_t current_inum = 2;
    
    if (path[0] == '\0') {
        if (out_inode) *out_inode = current_inode;
        if (out_inode_num) *out_inode_num = current_inum;
        return 0;
    }
    
    char token[256];
    int p_idx = 0;
    
    while (path[p_idx] != '\0') {
        int t_idx = 0;
        while (path[p_idx] != '\0' && path[p_idx] != '/') {
            token[t_idx++] = path[p_idx++];
        }
        token[t_idx] = '\0';
        
        if (path[p_idx] == '/') p_idx++;
        
        if (strlen(token) == 0) continue;
        
        if (current_task && current_task->euid != 0) {
            if (current_inode.i_uid == current_task->euid) {
                if (!(current_inode.i_mode & EXT2_S_IXUSR)) return -2;
            } else if (current_inode.i_gid == current_task->egid) {
                if (!(current_inode.i_mode & EXT2_S_IXGRP)) return -2;
            } else {
                if (!(current_inode.i_mode & EXT2_S_IXOTH)) return -2;
            }
        }
        
        if (ext2_find_in_dir(&current_inode, token, &current_inode, &current_inum) < 0) {
            return -1; // Not found
        }
    }
    
    if (out_inode) *out_inode = current_inode;
    if (out_inode_num) *out_inode_num = current_inum;
    return 0;
}


int ext2_open_file(const char* path) {
    ext2_inode_t inode;
    if (ext2_resolve_path(path, &inode, NULL) == 0) {
        // Permissions check
        if (current_task && current_task->euid != 0) {
            if (inode.i_uid == current_task->euid) {
                if (!(inode.i_mode & EXT2_S_IRUSR)) return -2;
            } else if (inode.i_gid == current_task->egid) {
                if (!(inode.i_mode & EXT2_S_IRGRP)) return -2;
            } else {
                if (!(inode.i_mode & EXT2_S_IROTH)) return -2;
            }
        }
        return 0;
    }
    return -1;
}

int ext2_read_file(const char* path, char* buf, size_t size, unsigned int offset) {
    ext2_inode_t inode;
    if (ext2_resolve_path(path, &inode, NULL) < 0) return -1;
    
    if (offset >= inode.i_size) return 0;
    uint32_t to_read = size;
    if (offset + size > inode.i_size) to_read = inode.i_size - offset;
    if (to_read == 0) return 0;
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    uint32_t bytes_read = 0;
    
    while (bytes_read < to_read) {
        uint32_t current_offset = offset + bytes_read;
        uint32_t block_index = current_offset / block_size;
        uint32_t offset_in_block = current_offset % block_size;
        
        uint32_t pblock = ext2_get_file_block(&inode, block_index);
        if (pblock == 0) {
            // Sparse file block
            memset(block_buf, 0, block_size);
        } else {
            ext2_read_block(pblock, block_buf);
        }
        
        uint32_t chunk = block_size - offset_in_block;
        if (chunk > to_read - bytes_read) chunk = to_read - bytes_read;
        
        memcpy(buf + bytes_read, block_buf + offset_in_block, chunk);
        bytes_read += chunk;
    }
    
    kfree(block_buf);
    return bytes_read;
}

int ext2_write_file(const char* path, const char* data, size_t size, unsigned int offset) {
    ext2_inode_t inode;
    uint32_t inode_num;
    if (ext2_resolve_path(path, &inode, &inode_num) < 0) return -1;
    
    // Check permissions
    if (current_task && current_task->euid != 0) {
        if (inode.i_uid == current_task->euid) {
            if (!(inode.i_mode & EXT2_S_IWUSR)) return -1;
        } else if (inode.i_gid == current_task->egid) {
            if (!(inode.i_mode & EXT2_S_IWGRP)) return -1;
        } else {
            if (!(inode.i_mode & EXT2_S_IWOTH)) return -1;
        }
    }
    
    // Removed the check that prevents file expansion
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    uint32_t bytes_written = 0;
    
    while (bytes_written < size) {
        uint32_t current_offset = offset + bytes_written;
        uint32_t block_index = current_offset / block_size;
        uint32_t offset_in_block = current_offset % block_size;
        
        uint32_t pblock = ext2_get_file_block(&inode, block_index);
        if (pblock == 0) {
            pblock = ext2_allocate_block();
            if (!pblock) break; // Out of space
            
            if (block_index < 12) {
                inode.i_block[block_index] = pblock;
                inode.i_blocks += block_size / 512;
            } else {
                ext2_free_block(pblock);
                break; // Indirect blocks not supported for appending
            }
            memset(block_buf, 0, block_size);
        } else {
            ext2_read_block(pblock, block_buf);
        }
        
        uint32_t chunk = block_size - offset_in_block;
        if (chunk > size - bytes_written) chunk = size - bytes_written;
        
        memcpy(block_buf + offset_in_block, data + bytes_written, chunk);
        ext2_write_block(pblock, block_buf);
        
        bytes_written += chunk;
    }
    
    if (offset + bytes_written > inode.i_size) {
        inode.i_size = offset + bytes_written;
    }
    ext2_write_inode_sync(inode_num, &inode);
    
    kfree(block_buf);
    return bytes_written;
}

int ext2_list_dir(const char* path, char* output, unsigned int output_size, int detailed) {
    (void)output_size;
    ext2_inode_t dir_inode;
    if (ext2_resolve_path(path, &dir_inode, NULL) < 0) return -1;
    
    if (!(dir_inode.i_mode & EXT2_S_IFDIR)) return -1;
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    uint32_t blocks = dir_inode.i_size / block_size + (dir_inode.i_size % block_size ? 1 : 0);
    
    output[0] = '\0';
    
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t pblock = ext2_get_file_block(&dir_inode, b);
        if (!pblock) continue;
        
        ext2_read_block(pblock, block_buf);
        uint32_t offset = 0;
        
        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buf + offset);
            if (entry->inode != 0 && entry->name_len > 0) {
                char name[256];
                memcpy(name, entry->name, entry->name_len);
                name[entry->name_len] = '\0';
                
                if (detailed) {
                    ext2_inode_t child_inode;
                    ext2_read_inode(entry->inode, &child_inode);
                    char line[512];
                    char type = (child_inode.i_mode & EXT2_S_IFDIR) ? 'd' : '-';
                    
                    sprintf(line, "%d", child_inode.i_size);
                    strcat(output, "2026-01-01 00:00  ");
                    strcat(output, line);
                    strcat(output, "  ");
                    if (type == 'd') strcat(output, "/");
                    strcat(output, name);
                    strcat(output, "\n");
                } else {
                    if (entry->file_type == EXT2_FT_DIR) strcat(output, "/");
                    strcat(output, name);
                    strcat(output, " ");
                }
            }
            if (entry->rec_len == 0) break;
            offset += entry->rec_len;
        }
    }
    
    kfree(block_buf);
    return 0;
}

int ext2_get_size(const char* path) {
    ext2_inode_t inode;
    if (ext2_resolve_path(path, &inode, NULL) < 0) return -1;
    return inode.i_size;
}

int ext2_is_dir(const char* path) {
    ext2_inode_t inode;
    if (ext2_resolve_path(path, &inode, NULL) < 0) return -1;
    return (inode.i_mode & EXT2_S_IFDIR) ? 1 : 0;
}

void ext2_init(void) {
    uint8_t* buf = (uint8_t*)kmalloc(1024);
    
    // Superblock is at offset 1024 (LBA 2 for 512-byte sectors)
    ata_read_sectors(2, buf, 2); 
    memcpy(&sb, buf, sizeof(ext2_superblock_t));
    kfree(buf);
    
    if (sb.s_magic != EXT2_MAGIC) {
        vga_print("EXT2: Invalid magic number!\n");
        return;
    }
    
    block_size = 1024 << sb.s_log_block_size;
    inodes_per_group = sb.s_inodes_per_group;
    blocks_per_group = sb.s_blocks_per_group;
    num_groups = sb.s_blocks_count / blocks_per_group;
    if (sb.s_blocks_count % blocks_per_group != 0) num_groups++;
    
    // Read Block Group Descriptor Table
    // BGDT starts at the block immediately following the Superblock.
    uint32_t bgdt_block = (block_size == 1024) ? 2 : 1;
    uint32_t bgdt_sectors = (num_groups * sizeof(ext2_bg_descriptor_t)) / 512 + 1;
    
    uint8_t* bgdt_buf = (uint8_t*)kmalloc(bgdt_sectors * 512);
    ata_read_sectors(bgdt_block * (block_size / 512), bgdt_buf, bgdt_sectors);
    
    bgd_table = (ext2_bg_descriptor_t*)kmalloc(num_groups * sizeof(ext2_bg_descriptor_t));
    memcpy(bgd_table, bgdt_buf, num_groups * sizeof(ext2_bg_descriptor_t));
    kfree(bgdt_buf);
    
    vga_print("EXT2: Initialized successfully.\n");
}

static void ext2_split_path(const char* path, char* parent_path, char* filename) {
    int last_slash = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') last_slash = i;
    }
    
    if (last_slash == -1) {
        strcpy(parent_path, "");
        strcpy(filename, path);
    } else if (last_slash == 0) {
        strcpy(parent_path, "/");
        strcpy(filename, path + 1);
    } else {
        strncpy(parent_path, path, last_slash);
        parent_path[last_slash] = '\0';
        strcpy(filename, path + last_slash + 1);
    }
}

static int ext2_remove_dir_entry(ext2_inode_t* dir, uint32_t dir_inum, const char* name) {
    (void)dir_inum;
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    uint32_t blocks = dir->i_size / block_size + (dir->i_size % block_size ? 1 : 0);
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t pblock = ext2_get_file_block(dir, b);
        if (!pblock) continue;
        
        ext2_read_block(pblock, block_buf);
        uint32_t offset = 0;
        
        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buf + offset);
            if (entry->inode != 0 && entry->name_len > 0) {
                if (strlen(name) == entry->name_len && strncmp(name, entry->name, entry->name_len) == 0) {
                    entry->inode = 0;
                    ext2_write_block(pblock, block_buf);
                    kfree(block_buf);
                    return 0;
                }
            }
            if (entry->rec_len == 0) break;
            offset += entry->rec_len;
        }
    }
    kfree(block_buf);
    return -1;
}

int ext2_create_file(const char* path) {
    char parent_path[256];
    char filename[256];
    ext2_split_path(path, parent_path, filename);
    
    ext2_inode_t parent_inode;
    uint32_t parent_inum;
    if (ext2_resolve_path(parent_path, &parent_inode, &parent_inum) < 0) return -1;
    
    if (current_task && current_task->euid != 0) {
        if (parent_inode.i_uid == current_task->euid) {
            if (!(parent_inode.i_mode & EXT2_S_IWUSR)) return -1;
        } else if (parent_inode.i_gid == current_task->egid) {
            if (!(parent_inode.i_mode & EXT2_S_IWGRP)) return -1;
        } else {
            if (!(parent_inode.i_mode & EXT2_S_IWOTH)) return -1;
        }
    }
    
    uint32_t new_inum = ext2_allocate_inode();
    if (!new_inum) return -1;
    
    unsigned int mask = current_task ? current_task->umask : 022;
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.i_mode = EXT2_S_IFREG | ((0666 & ~mask) & 07777);
    new_inode.i_uid = current_task ? current_task->euid : 0;
    new_inode.i_gid = current_task ? current_task->egid : 0;
    new_inode.i_size = 0;
    new_inode.i_links_count = 1;
    new_inode.i_blocks = 0;
    
    ext2_write_inode_sync(new_inum, &new_inode);
    
    if (ext2_add_dir_entry(&parent_inode, parent_inum, new_inum, filename, EXT2_FT_REG_FILE) < 0) {
        ext2_free_inode(new_inum);
        return -1;
    }
    
    return 0;
}

int ext2_create_dir(const char* path) {
    char parent_path[256];
    char filename[256];
    ext2_split_path(path, parent_path, filename);
    
    ext2_inode_t parent_inode;
    uint32_t parent_inum;
    if (ext2_resolve_path(parent_path, &parent_inode, &parent_inum) < 0) return -1;
    
    if (current_task && current_task->euid != 0) {
        if (parent_inode.i_uid == current_task->euid) {
            if (!(parent_inode.i_mode & EXT2_S_IWUSR)) return -1;
        } else if (parent_inode.i_gid == current_task->egid) {
            if (!(parent_inode.i_mode & EXT2_S_IWGRP)) return -1;
        } else {
            if (!(parent_inode.i_mode & EXT2_S_IWOTH)) return -1;
        }
    }
    
    uint32_t new_inum = ext2_allocate_inode();
    if (!new_inum) return -1;
    
    unsigned int mask = current_task ? current_task->umask : 022;
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.i_mode = EXT2_S_IFDIR | ((0777 & ~mask) & 07777);
    new_inode.i_uid = current_task ? current_task->euid : 0;
    new_inode.i_gid = current_task ? current_task->egid : 0;
    new_inode.i_size = 0;
    new_inode.i_links_count = 2;
    new_inode.i_blocks = 0;
    
    uint32_t block = ext2_allocate_block();
    if (!block) {
        ext2_free_inode(new_inum);
        return -1;
    }
    new_inode.i_block[0] = block;
    new_inode.i_size = block_size;
    new_inode.i_blocks = block_size / 512;
    
    ext2_write_inode_sync(new_inum, &new_inode);
    
    uint8_t* block_buf = (uint8_t*)kmalloc(block_size);
    memset(block_buf, 0, block_size);
    
    ext2_dir_entry_t* dot = (ext2_dir_entry_t*)block_buf;
    dot->inode = new_inum;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';
    
    ext2_dir_entry_t* dotdot = (ext2_dir_entry_t*)(block_buf + 12);
    dotdot->inode = parent_inum;
    dotdot->rec_len = block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    
    ext2_write_block(block, block_buf);
    kfree(block_buf);
    
    if (ext2_add_dir_entry(&parent_inode, parent_inum, new_inum, filename, EXT2_FT_DIR) < 0) {
        return -1;
    }
    
    parent_inode.i_links_count++;
    ext2_write_inode_sync(parent_inum, &parent_inode);
    
    return 0;
}

int ext2_delete_file(const char* path) {
    char parent_path[256];
    char filename[256];
    ext2_split_path(path, parent_path, filename);
    
    ext2_inode_t parent_inode;
    uint32_t parent_inum;
    if (ext2_resolve_path(parent_path, &parent_inode, &parent_inum) < 0) return -1;
    
    if (current_task && current_task->euid != 0) {
        if (parent_inode.i_uid == current_task->euid) {
            if (!(parent_inode.i_mode & EXT2_S_IWUSR)) return -1;
        } else if (parent_inode.i_gid == current_task->egid) {
            if (!(parent_inode.i_mode & EXT2_S_IWGRP)) return -1;
        } else {
            if (!(parent_inode.i_mode & EXT2_S_IWOTH)) return -1;
        }
    }
    
    ext2_inode_t target_inode;
    uint32_t target_inum;
    if (ext2_resolve_path(path, &target_inode, &target_inum) < 0) return -1;
    
    for (int i = 0; i < 12; i++) {
        if (target_inode.i_block[i] != 0) {
            ext2_free_block(target_inode.i_block[i]);
            target_inode.i_block[i] = 0;
        }
    }
    
    target_inode.i_size = 0;
    target_inode.i_blocks = 0;
    target_inode.i_links_count = 0;
    target_inode.i_dtime = 1;
    ext2_write_inode_sync(target_inum, &target_inode);
    ext2_free_inode(target_inum);
    
    ext2_remove_dir_entry(&parent_inode, parent_inum, filename);
    return 0;
}
int ext2_close(unsigned int offset, void* internal_data, int mode) { 
    (void)offset; (void)internal_data; (void)mode; return 0; 
}

int ext2_chown(const char* path, int uid, int gid) {
    ext2_inode_t inode;
    uint32_t inum;
    if (ext2_resolve_path(path, &inode, &inum) < 0) return -1;
    
    if (current_task && current_task->euid != 0 && inode.i_uid != current_task->euid) {
        return -1;
    }
    
    if (uid != -1) inode.i_uid = uid;
    if (gid != -1) inode.i_gid = gid;
    
    ext2_write_inode_sync(inum, &inode);
    return 0;
}

int ext2_chmod(const char* path, int mode) {
    ext2_inode_t inode;
    uint32_t inum;
    if (ext2_resolve_path(path, &inode, &inum) < 0) return -1;
    
    if (current_task && current_task->euid != 0 && inode.i_uid != current_task->euid) {
        return -1; // Only owner or root can chmod
    }
    
    inode.i_mode = (inode.i_mode & EXT2_S_IFMT) | (mode & 07777);
    
    ext2_write_inode_sync(inum, &inode);
    return 0;
}

int ext2_stat(const char* path, struct fs_stat* st) {
    ext2_inode_t inode;
    if (ext2_resolve_path(path, &inode, NULL) < 0) return -1;
    st->mode = inode.i_mode;
    st->uid = inode.i_uid;
    st->gid = inode.i_gid;
    st->size = inode.i_size;
    return 0;
}

struct fs_driver ext2_fs_driver = {
    .open = ext2_open_file,
    .read = ext2_read_file,
    .write = ext2_write_file,
    .create_file = ext2_create_file,
    .create_dir = ext2_create_dir,
    .delete_file = ext2_delete_file,
    .list_dir = ext2_list_dir,
    .get_size = ext2_get_size,
    .is_dir = ext2_is_dir,
    .chown = ext2_chown,
    .chmod = ext2_chmod,
    .stat = ext2_stat,
    .close = ext2_close
};
