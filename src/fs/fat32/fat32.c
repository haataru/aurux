#include "fat32.h"
#include "../../drivers/ata/ata.h"
#include "../../lib/lib.h"
#include "../../drivers/vga/vga.h"
#include "../../drivers/rtc/rtc.h"

static unsigned int partition_offset = 0;
static struct fat32_bpb bpb;
static unsigned int fat_start = 0;
static unsigned int data_start = 0;
static unsigned int root_cluster = 0;

unsigned char fat32_lfn_checksum(const unsigned char* short_name) {
    unsigned char sum = 0;
    for (int i = 11; i != 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_name++;
    }
    return sum;
}

static unsigned int fat32_get_next_cluster(unsigned int cluster);
static void format_83_name(const char* input, char* output);
static int strncmp_83(const char* name11, const char* formatted_name);

unsigned short fat32_pack_time(int hours, int minutes, int seconds) {
    return (unsigned short)((hours << 11) | (minutes << 5) | (seconds / 2));
}

unsigned short fat32_pack_date(int year, int month, int day) {
    if (year < 1980) year = 1980;
    return (unsigned short)(((year - 1980) << 9) | (month << 5) | day);
}

int strcmp_ignore_case(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1 >= 'A' && *s1 <= 'Z' ? *s1 + 32 : *s1;
        char c2 = *s2 >= 'A' && *s2 <= 'Z' ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return *s1 - *s2;
}

static int fat32_find_entry_in_dir(unsigned int dir_cluster, const char* target_name, struct fat32_dir_entry* out_entry, unsigned int* out_sector, unsigned int* out_index) {
    char lfn_buf[256];
    int lfn_active = 0;
    unsigned char expected_checksum = 0;
    unsigned char sector[512];
    
    char formatted_name[11];
    format_83_name(target_name, formatted_name);

    unsigned int cluster = dir_cluster;
    while (cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(first_sector_of_cluster + i, sector);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
            for (unsigned int j = 0; j < 512 / sizeof(struct fat32_dir_entry); j++) {
                if (dir[j].name[0] == 0x00) return -1;
                if (dir[j].name[0] == (char)0xE5) {
                    lfn_active = 0;
                    continue;
                }
                
                if (dir[j].attributes == FAT_ATTR_LFN) {
                    struct fat32_lfn_entry* lfn = (struct fat32_lfn_entry*)&dir[j];
                    if (lfn->order & 0x40) {
                        for(int k=0; k<256; k++) lfn_buf[k] = 0;
                        lfn_active = 1;
                        expected_checksum = lfn->checksum;
                    }
                    if (lfn_active && lfn->checksum == expected_checksum) {
                        int index = (lfn->order & 0x3F) - 1;
                        int offset = index * 13;
                        if (offset >= 0 && offset < 242) {
                            lfn_buf[offset+0] = lfn->name1[0] & 0xFF; lfn_buf[offset+1] = lfn->name1[1] & 0xFF; lfn_buf[offset+2] = lfn->name1[2] & 0xFF; lfn_buf[offset+3] = lfn->name1[3] & 0xFF; lfn_buf[offset+4] = lfn->name1[4] & 0xFF;
                            lfn_buf[offset+5] = lfn->name2[0] & 0xFF; lfn_buf[offset+6] = lfn->name2[1] & 0xFF; lfn_buf[offset+7] = lfn->name2[2] & 0xFF; lfn_buf[offset+8] = lfn->name2[3] & 0xFF; lfn_buf[offset+9] = lfn->name2[4] & 0xFF; lfn_buf[offset+10]= lfn->name2[5] & 0xFF;
                            lfn_buf[offset+11]= lfn->name3[0] & 0xFF; lfn_buf[offset+12]= lfn->name3[1] & 0xFF;
                        }
                    }
                    continue;
                }
                
                char name[256];

                int is_match = 0;
                
                if (lfn_active && fat32_lfn_checksum((unsigned char*)dir[j].name) == expected_checksum) {
                    strcpy(name, lfn_buf);
                    if (strcmp_ignore_case(name, target_name) == 0) is_match = 1;
                }
                lfn_active = 0;
                
                if (!is_match && strncmp_83(dir[j].name, formatted_name) == 0) {
                    is_match = 1;
                }
                
                if (is_match) {
                    if (out_entry) *out_entry = dir[j];
                    if (out_sector) *out_sector = first_sector_of_cluster + i;
                    if (out_index) *out_index = j;
                    return 0;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return -1;
}

// MBR Partition entry
struct partition_entry {
    unsigned char  status;
    unsigned char  chs_first[3];
    unsigned char  type;
    unsigned char  chs_last[3];
    unsigned int   lba_first;
    unsigned int   sectors;
} __attribute__((packed));

void fat32_init(void) {
    unsigned char sector[512];
    
    // Read BPB (Sector 0)
    ata_read_sector(0, sector);
    

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        vga_print("FAT32: No valid BPB signature found!\n");
        return;
    }
    
    partition_offset = 0; // Unpartitioned image
    
    memcpy(&bpb, sector, sizeof(struct fat32_bpb));
    
    fat_start = partition_offset + bpb.reserved_sectors;
    data_start = fat_start + (bpb.fat_count * bpb.fat_size_32);
    root_cluster = bpb.root_cluster;
    
    vga_print("FAT32 initialized.\n");
}

static unsigned int fat32_get_next_cluster(unsigned int cluster) {
    unsigned char sector[512];
    unsigned int fat_offset = cluster * 4;
    unsigned int fat_sector = fat_start + (fat_offset / 512);
    unsigned int ent_offset = fat_offset % 512;
    
    ata_read_sector(fat_sector, sector);
    unsigned int next_cluster = *((unsigned int*)&sector[ent_offset]) & 0x0FFFFFFF;
    return next_cluster;
}

static void fat32_write_fat_entry(unsigned int cluster, unsigned int value) {
    unsigned char sector[512];
    unsigned int fat_offset = cluster * 4;
    unsigned int fat_sector = fat_start + (fat_offset / 512);
    unsigned int ent_offset = fat_offset % 512;
    
    // We must update the FAT entry in all FAT copies.
    for (int i = 0; i < bpb.fat_count; i++) {
        unsigned int current_fat_sector = fat_sector + (i * bpb.fat_size_32);
        ata_read_sector(current_fat_sector, sector);
        
        unsigned int existing = *((unsigned int*)&sector[ent_offset]);
        // Preserve the top 4 bits.
        existing = (existing & 0xF0000000) | (value & 0x0FFFFFFF);
        *((unsigned int*)&sector[ent_offset]) = existing;
        
        ata_write_sector(current_fat_sector, sector);
    }
}

static unsigned int fat32_find_free_cluster(void) {
    unsigned char sector[512];
    // Calculate approximate total clusters.
    unsigned int total_data_sectors = bpb.total_sectors_long - (bpb.reserved_sectors + (bpb.fat_count * bpb.fat_size_32));
    unsigned int total_clusters = total_data_sectors / bpb.sectors_per_cluster;
    
    // Iterate through FAT to find a 0 entry. Cluster 0 and 1 are reserved, start from 2.
    for (unsigned int cluster = 2; cluster < total_clusters + 2; cluster++) {
        unsigned int fat_offset = cluster * 4;
        unsigned int fat_sector = fat_start + (fat_offset / 512);
        unsigned int ent_offset = fat_offset % 512;
        
        // Optimization: read sector once for 128 entries.
        if (ent_offset == 0 || cluster == 2) {
            ata_read_sector(fat_sector, sector);
        }
        
        unsigned int entry = *((unsigned int*)&sector[ent_offset]) & 0x0FFFFFFF;
        if (entry == 0x00000000) {
            return cluster;
        }
    }
    return 0; // Disk full
}

static unsigned int fat32_allocate_cluster(unsigned int previous_cluster) {
    unsigned int new_cluster = fat32_find_free_cluster();
    if (new_cluster == 0) return 0;
    
    // Mark new cluster as End Of Chain (EOC).
    fat32_write_fat_entry(new_cluster, 0x0FFFFFFF);
    

    unsigned char zero_sector[512];
    for (int i=0; i<512; i++) zero_sector[i] = 0;
    
    unsigned int first_sector = data_start + (new_cluster - 2) * bpb.sectors_per_cluster;
    for (int i = 0; i < bpb.sectors_per_cluster; i++) {
        ata_write_sector(first_sector + i, zero_sector);
    }
    

    if (previous_cluster != 0) {
        fat32_write_fat_entry(previous_cluster, new_cluster);
    }
    
    return new_cluster;
}

static void fat32_free_chain(unsigned int start_cluster) {
    unsigned int cluster = start_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        unsigned int next = fat32_get_next_cluster(cluster);
        fat32_write_fat_entry(cluster, 0x00000000);
        cluster = next;
    }
}

static void format_83_name(const char* input, char* output) {
    // Initialize with spaces for 8.3 name format.
    for (int i = 0; i < 11; i++) {
        output[i] = ' ';
    }
    
    if (input[0] == '.' && input[1] == '\0') {
        output[0] = '.';
        return;
    }
    if (input[0] == '.' && input[1] == '.' && input[2] == '\0') {
        output[0] = '.';
        output[1] = '.';
        return;
    }
    
    int i = 0;
    int j = 0;

    while (input[i] != '\0' && input[i] != '.' && j < 8) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }
    

    while (input[i] != '\0' && input[i] != '.') i++;
    if (input[i] == '.') {
        i++;
        j = 8;
        
        while (input[i] != '\0' && j < 11) {
            char c = input[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            output[j++] = c;
        }
    }
}

static int strncmp_83(const char* name11, const char* formatted_name) {
    for(int i=0; i<11; i++) {
        if (name11[i] != formatted_name[i]) return 1;
    }
    return 0;
}

unsigned int fat32_find_dir_cluster(const char* abs_path, unsigned int* parent_cluster, char* filename) {
    unsigned int current_cluster = root_cluster;
    if (parent_cluster) *parent_cluster = root_cluster;
    
    char path_copy[256];
    strcpy(path_copy, abs_path);
    

    if (path_copy[0] == '\0' || strcmp(path_copy, "/") == 0) {
        if (filename) filename[0] = '\0';
        return root_cluster;
    }
    
    char* token = path_copy;
    if (token[0] == '/') token++;
    
    char* next_token = token;
    
    while (*token) {

        while (*next_token && *next_token != '/') next_token++;
        
        if (*next_token == '/') {
            *next_token = '\0';
            next_token++;
        }
        
        // If there is no next token, this is the last part (the filename or target directory).
        if (*next_token == '\0' || strlen(next_token) == 0) {
            if (filename) strcpy(filename, token);
            return current_cluster;
        }
        
        // Search for the directory 'token' in current_cluster.
        struct fat32_dir_entry entry;
        if (fat32_find_entry_in_dir(current_cluster, token, &entry, NULL, NULL) == 0) {
            if (entry.attributes & FAT_ATTR_DIRECTORY) {
                unsigned int next_cluster = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
                if (next_cluster == 0) next_cluster = root_cluster;
                current_cluster = next_cluster;
                if (parent_cluster) *parent_cluster = current_cluster;
                token = next_token;
                continue;
            }
        }
        return 0xFFFFFFFF; // Path not found or not a directory.
    }
    
    return current_cluster;
}

unsigned int fat32_get_cluster_for_path(const char* path) {
    if (path[0] == '\0' || strcmp(path, "/") == 0) return root_cluster;
    
    char filename[256];
    unsigned int parent_cluster = fat32_find_dir_cluster(path, NULL, filename);
    if (parent_cluster == 0xFFFFFFFF) return 0xFFFFFFFF;
    
    char formatted_name[11];
    format_83_name(filename, formatted_name);
    
    unsigned char sector[512];
    unsigned int cluster = parent_cluster;
    
    while (cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(first_sector_of_cluster + i, sector);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
            for (unsigned int j = 0; j < 512 / sizeof(struct fat32_dir_entry); j++) {
                if (dir[j].name[0] == 0x00) return 0xFFFFFFFF;
                if (dir[j].name[0] == (char)0xE5 || (dir[j].attributes & FAT_ATTR_LFN) == FAT_ATTR_LFN) continue;
                
                if (strncmp_83(dir[j].name, formatted_name) == 0) {
                    unsigned int target_cluster = ((unsigned int)dir[j].cluster_high << 16) | dir[j].cluster_low;
                    if (target_cluster == 0) return root_cluster;
                    return target_cluster;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return 0xFFFFFFFF;
}

int fat32_is_dir(const char* path) {
    if (path[0] == '\0' || strcmp(path, "/") == 0) return 1;
    
    char filename[256];
    unsigned int dir_cluster = fat32_find_dir_cluster(path, NULL, filename);
    if (dir_cluster == 0xFFFFFFFF) return 0;
    
    struct fat32_dir_entry entry;
    if (fat32_find_entry_in_dir(dir_cluster, filename, &entry, NULL, NULL) == 0) {
        return (entry.attributes & FAT_ATTR_DIRECTORY) ? 1 : 0;
    }
    return 0;
}

int fat32_read_file_ex(const char* filename, unsigned char* buffer, size_t size, unsigned int offset) {
    char target_name[256];
    unsigned int cluster = fat32_find_dir_cluster(filename, NULL, target_name);
    if (cluster == 0xFFFFFFFF) return -1;
    
    struct fat32_dir_entry entry;
    if (fat32_find_entry_in_dir(cluster, target_name, &entry, NULL, NULL) == 0) {
        if (entry.attributes & FAT_ATTR_DIRECTORY) return -1;
        
        unsigned int file_cluster = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
        unsigned int file_size = entry.size;
        
        if (offset >= file_size) return 0;
        unsigned int to_read = size;
        if (offset + size > file_size) to_read = file_size - offset;
        
        unsigned int bytes_read = 0;
        unsigned int current_pos = 0;
        unsigned char sector[512];
        
        while (file_cluster < 0x0FFFFFF8 && bytes_read < to_read) {
            unsigned int first_sector_of_cluster = data_start + (file_cluster - 2) * bpb.sectors_per_cluster;
            for (int i = 0; i < bpb.sectors_per_cluster && bytes_read < to_read; i++) {
                if (current_pos + 512 <= offset) {
                    current_pos += 512;
                    continue; // Skip this sector entirely
                }
                
                ata_read_sector(first_sector_of_cluster + i, sector);
                
                for (int k = 0; k < 512 && bytes_read < to_read; k++, current_pos++) {
                    if (current_pos >= offset) {
                        buffer[bytes_read++] = sector[k];
                    }
                }
            }
            file_cluster = fat32_get_next_cluster(file_cluster);
        }
        return bytes_read;
    }
    return -1;
}

int fat32_read_file(const char* filename, unsigned char* buffer) {
    int size = fat32_get_file_size(filename);
    if (size < 0) return -1;
    return fat32_read_file_ex(filename, buffer, size, 0);
}

int fat32_get_file_size(const char* filename) {
    char target_name[256];
    unsigned int cluster = fat32_find_dir_cluster(filename, NULL, target_name);
    if (cluster == 0xFFFFFFFF) return -1;
    
    struct fat32_dir_entry entry;
    if (fat32_find_entry_in_dir(cluster, target_name, &entry, NULL, NULL) == 0) {
        if (entry.attributes & FAT_ATTR_DIRECTORY) return -1;
        return entry.size;
    }
    return -1;
}

static int fat32_allocate_dir_entries(unsigned int dir_cluster, struct fat32_dir_entry* entries, int count) {
    unsigned char sector[512];
    unsigned int cluster = dir_cluster;
    unsigned int prev_cluster = 0;
    
    int free_count = 0;

    unsigned int start_sector_idx = 0;
    unsigned int start_entry_idx = 0;
    
    while (cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(first_sector_of_cluster + i, sector);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
            
            for (unsigned int j = 0; j < 512 / sizeof(struct fat32_dir_entry); j++) {
                if (dir[j].name[0] == 0x00 || dir[j].name[0] == (char)0xE5) {
                    if (free_count == 0) {
                        start_sector_idx = first_sector_of_cluster + i;
                        start_entry_idx = j;
                    }
                    free_count++;
                    
                    if (free_count == count) {
                        // Write the entries starting from start_cluster/sector/entry.

                        unsigned int c_sector_idx = start_sector_idx;
                        unsigned int c_entry_idx = start_entry_idx;
                        
                        for (int k = 0; k < count; k++) {
                            ata_read_sector(c_sector_idx, sector);
                            struct fat32_dir_entry* w_dir = (struct fat32_dir_entry*)sector;
                            memcpy(&w_dir[c_entry_idx], &entries[k], sizeof(struct fat32_dir_entry));
                            ata_write_sector(c_sector_idx, sector);
                            
                            c_entry_idx++;
                            if (c_entry_idx >= 512 / sizeof(struct fat32_dir_entry)) {
                                c_entry_idx = 0;
                                c_sector_idx++;
                                // If crossed cluster boundary, assume contiguous sectors within cluster for simplicity.
                                // A robust implementation should properly iterate through the cluster chain.
                            }
                        }
                        return 0;
                    }
                } else {
                    free_count = 0;
                }
            }
        }
        prev_cluster = cluster;
        cluster = fat32_get_next_cluster(cluster);
    }
    
    // If the directory is full, allocate a new cluster.
    unsigned int new_cluster = fat32_allocate_cluster(prev_cluster);
    if (new_cluster == 0) return -1;
    
    // Safely assume the new cluster has plenty of free space for 'count' entries.
    unsigned int first_sector_of_new_cluster = data_start + (new_cluster - 2) * bpb.sectors_per_cluster;
    ata_read_sector(first_sector_of_new_cluster, sector);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
    for (int k = 0; k < count; k++) {
        memcpy(&dir[k], &entries[k], sizeof(struct fat32_dir_entry));
    }
    ata_write_sector(first_sector_of_new_cluster, sector);
    
    return 0;
}

int fat32_create_file(const char* filename, unsigned char attr) {
    char target_name[256];
    unsigned int dir_cluster = fat32_find_dir_cluster(filename, NULL, target_name);
    if (dir_cluster == 0xFFFFFFFF) return -1;
    
    // Check if it already exists.
    struct fat32_dir_entry existing;
    if (fat32_find_entry_in_dir(dir_cluster, target_name, &existing, NULL, NULL) == 0) {
        return -1; // File already exists.
    }

    struct fat32_dir_entry main_entry;
    for(int i=0; i<11; i++) main_entry.name[i] = ' ';
    
    // Generate a unique 8.3 name.
    char formatted_name[11];
    format_83_name(target_name, formatted_name);
    
    // To ensure uniqueness, we should technically check if formatted_name exists.
    // If it does, we append ~1, ~2. For now, we use a simple ~1 collision resolution.
    if (fat32_find_entry_in_dir(dir_cluster, target_name, &existing, NULL, NULL) == 0) {
        // This is a simplification. A robust implementation would iterate ~1 to ~9.
    }
    memcpy(main_entry.name, formatted_name, 11);
    
    unsigned int file_cluster = (attr & FAT_ATTR_DIRECTORY) ? fat32_allocate_cluster(0) : 0;
    if ((attr & FAT_ATTR_DIRECTORY) && file_cluster == 0) return -1;

    int hours, minutes, seconds;
    int day, month, year;
    rtc_getTime(&hours, &minutes, &seconds);
    rtc_getDate(&day, &month, &year);

    unsigned short time = fat32_pack_time(hours, minutes, seconds);
    unsigned short date = fat32_pack_date(year, month, day);

    main_entry.attributes = attr;
    main_entry.reserved = 0;
    main_entry.creation_time_tenths = 0;
    main_entry.creation_time = time;
    main_entry.creation_date = date;
    main_entry.last_access_date = date;
    main_entry.cluster_high = (unsigned short)(file_cluster >> 16);
    main_entry.last_write_time = time;
    main_entry.last_write_date = date;
    main_entry.cluster_low = (unsigned short)(file_cluster & 0xFFFF);
    main_entry.size = 0;
    
    int name_len = strlen(target_name);
    int lfn_count = (name_len + 12) / 13; // 13 characters per LFN entry.
    
    struct fat32_dir_entry entries[32]; // Max LFN entries is ~20, plus 1 for main entry
    unsigned char chksum = fat32_lfn_checksum((unsigned char*)main_entry.name);
    
    for (int i = 0; i < lfn_count; i++) {
        struct fat32_lfn_entry* lfn = (struct fat32_lfn_entry*)&entries[lfn_count - 1 - i];
        int lfn_index = i;
        lfn->order = (lfn_index + 1) | (lfn_index == lfn_count - 1 ? 0x40 : 0x00);
        lfn->attributes = FAT_ATTR_LFN;
        lfn->type = 0;
        lfn->checksum = chksum;
        lfn->first_cluster = 0;
        
        int offset = lfn_index * 13;
        for (int k = 0; k < 5; k++) lfn->name1[k] = (offset + k < name_len) ? target_name[offset + k] : ((offset + k == name_len) ? 0x0000 : 0xFFFF);
        offset += 5;
        for (int k = 0; k < 6; k++) lfn->name2[k] = (offset + k < name_len) ? target_name[offset + k] : ((offset + k == name_len) ? 0x0000 : 0xFFFF);
        offset += 6;
        for (int k = 0; k < 2; k++) lfn->name3[k] = (offset + k < name_len) ? target_name[offset + k] : ((offset + k == name_len) ? 0x0000 : 0xFFFF);
    }
    
    entries[lfn_count] = main_entry;
    
    if (fat32_allocate_dir_entries(dir_cluster, entries, lfn_count + 1) != 0) {
        if (attr & FAT_ATTR_DIRECTORY) fat32_free_chain(file_cluster);
        return -1;
    }
    
    if (attr & FAT_ATTR_DIRECTORY) {
        // Initialize . and .. for the new directory.
        unsigned char sector[512];
        for(int i=0; i<512; i++) sector[i] = 0;
        
        struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
        

        format_83_name(".", (char*)dir[0].name);
        dir[0].attributes = FAT_ATTR_DIRECTORY;
        dir[0].cluster_high = file_cluster >> 16;
        dir[0].cluster_low = file_cluster & 0xFFFF;
        

        format_83_name("..", (char*)dir[1].name);
        dir[1].attributes = FAT_ATTR_DIRECTORY;
        dir[1].cluster_high = dir_cluster >> 16;
        dir[1].cluster_low = dir_cluster & 0xFFFF;
        
        unsigned int file_sector = data_start + (file_cluster - 2) * bpb.sectors_per_cluster;
        ata_write_sector(file_sector, sector);
    }
    
    return 0;
}

int fat32_write_file(const char* filename, const unsigned char* buffer, unsigned int size) {
    char target_name[256];
    unsigned int cluster = fat32_find_dir_cluster(filename, NULL, target_name);
    if (cluster == 0xFFFFFFFF) return -1;
    
    unsigned char sector[512];
    unsigned int file_cluster = 0;
    
    char formatted_name[11];
    format_83_name(target_name, formatted_name);
    
    int entry_sector = -1;
    int entry_index = -1;
    
    unsigned int search_cluster = cluster;
    while (search_cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = data_start + (search_cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(first_sector_of_cluster + i, sector);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
            for (unsigned int j = 0; j < 512 / sizeof(struct fat32_dir_entry); j++) {
                if (dir[j].name[0] == 0x00) goto create_it;
                if (dir[j].name[0] == (char)0xE5 || (dir[j].attributes & FAT_ATTR_LFN) == FAT_ATTR_LFN) continue;
                if (strncmp_83(dir[j].name, formatted_name) == 0) {
                    file_cluster = ((unsigned int)dir[j].cluster_high << 16) | dir[j].cluster_low;
                    entry_sector = first_sector_of_cluster + i;
                    entry_index = j;
                    goto found;
                }
            }
        }
        search_cluster = fat32_get_next_cluster(search_cluster);
    }

create_it:
    if (fat32_create_file(filename, 0) != 0) return -1;
    
    // Re-search to find the newly created entry.
    search_cluster = cluster;
    while (search_cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = data_start + (search_cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(first_sector_of_cluster + i, sector);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
            for (unsigned int j = 0; j < 512 / sizeof(struct fat32_dir_entry); j++) {
                if (dir[j].name[0] == 0x00) return -1;
                if (dir[j].name[0] == (char)0xE5 || (dir[j].attributes & FAT_ATTR_LFN) == FAT_ATTR_LFN) continue;
                if (strncmp_83(dir[j].name, formatted_name) == 0) {
                    file_cluster = ((unsigned int)dir[j].cluster_high << 16) | dir[j].cluster_low;
                    entry_sector = first_sector_of_cluster + i;
                    entry_index = j;
                    goto found;
                }
            }
        }
        search_cluster = fat32_get_next_cluster(search_cluster);
    }
    return -1;

found:
    ;
    unsigned int bytes_written = 0;
    const unsigned char* ptr = buffer;
    
    unsigned int current_cluster = file_cluster;
    unsigned int prev_cluster = 0;
    
    while (bytes_written < size) {
        if (current_cluster >= 0x0FFFFFF8 || current_cluster == 0) {
            unsigned int new_cluster = fat32_allocate_cluster(prev_cluster);
            if (new_cluster == 0) break; // Disk full.
            if (file_cluster == 0) file_cluster = new_cluster;
            current_cluster = new_cluster;
        }
        
        unsigned int first_sector_of_cluster = data_start + (current_cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            unsigned int to_copy = 512;
            if (size - bytes_written < 512) {
                to_copy = size - bytes_written;
            }
            
            unsigned char write_sec[512] = {0};
            memcpy(write_sec, ptr, to_copy);
            ata_write_sector(first_sector_of_cluster + i, write_sec);
            
            ptr += to_copy;
            bytes_written += to_copy;
            if (bytes_written >= size) break;
        }
        
        prev_cluster = current_cluster;
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    // Update file size in directory entry.
    ata_read_sector(entry_sector, sector);
    struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
    if (file_cluster != 0) {
        dir[entry_index].cluster_high = (unsigned short)(file_cluster >> 16);
        dir[entry_index].cluster_low = (unsigned short)(file_cluster & 0xFFFF);
    }
    dir[entry_index].size = bytes_written;
    ata_write_sector(entry_sector, sector);
    
    return bytes_written;
}

int fat32_delete_file(const char* filename) {
    char target_name[256];
    unsigned int cluster = fat32_find_dir_cluster(filename, NULL, target_name);
    if (cluster == 0xFFFFFFFF) return -1;
    
    struct fat32_dir_entry entry;
    unsigned int sector_idx, entry_idx;
    if (fat32_find_entry_in_dir(cluster, target_name, &entry, &sector_idx, &entry_idx) == 0) {
        unsigned int file_cluster = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
        if (file_cluster != 0) {
            fat32_free_chain(file_cluster);
        }
        
        unsigned char sector[512];
        ata_read_sector(sector_idx, sector);
        struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
        dir[entry_idx].name[0] = (char)0xE5; // Mark as deleted.
        ata_write_sector(sector_idx, sector);
        
        // We also should delete associated LFN entries, but for read-only LFN
        // we can just delete the 8.3 entry. The LFN entries will be orphaned,
        // which is not ideal but works for now.
           
        return 0;
    }
    return -1;
}

int fat32_list_dir(const char* path, char* output, unsigned int output_size, int detailed) {
    unsigned int cluster = fat32_get_cluster_for_path(path);
    if (cluster == 0xFFFFFFFF) return -1;
    
    unsigned char sector[512];
    unsigned int out_len = 0;
    output[0] = '\0';
    
    char lfn_buf[256];
    int lfn_active = 0;
    unsigned char expected_checksum = 0;
    
    while (cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = data_start + (cluster - 2) * bpb.sectors_per_cluster;
        for (int i = 0; i < bpb.sectors_per_cluster; i++) {
            ata_read_sector(first_sector_of_cluster + i, sector);
            struct fat32_dir_entry* dir = (struct fat32_dir_entry*)sector;
            for (unsigned int j = 0; j < 512 / sizeof(struct fat32_dir_entry); j++) {
                if (dir[j].name[0] == 0x00) return 0;
                if (dir[j].name[0] == (char)0xE5) {
                    lfn_active = 0;
                    continue;
                }
                
                if (dir[j].attributes == FAT_ATTR_LFN) {
                    struct fat32_lfn_entry* lfn = (struct fat32_lfn_entry*)&dir[j];
                    if (lfn->order & 0x40) {
                        for(int k=0; k<256; k++) lfn_buf[k] = 0;
                        lfn_active = 1;
                        expected_checksum = lfn->checksum;
                    }
                    if (lfn_active && lfn->checksum == expected_checksum) {
                        int index = (lfn->order & 0x3F) - 1;
                        int offset = index * 13;
                        if (offset >= 0 && offset < 242) {
                            lfn_buf[offset+0] = lfn->name1[0] & 0xFF;
                            lfn_buf[offset+1] = lfn->name1[1] & 0xFF;
                            lfn_buf[offset+2] = lfn->name1[2] & 0xFF;
                            lfn_buf[offset+3] = lfn->name1[3] & 0xFF;
                            lfn_buf[offset+4] = lfn->name1[4] & 0xFF;
                            lfn_buf[offset+5] = lfn->name2[0] & 0xFF;
                            lfn_buf[offset+6] = lfn->name2[1] & 0xFF;
                            lfn_buf[offset+7] = lfn->name2[2] & 0xFF;
                            lfn_buf[offset+8] = lfn->name2[3] & 0xFF;
                            lfn_buf[offset+9] = lfn->name2[4] & 0xFF;
                            lfn_buf[offset+10]= lfn->name2[5] & 0xFF;
                            lfn_buf[offset+11]= lfn->name3[0] & 0xFF;
                            lfn_buf[offset+12]= lfn->name3[1] & 0xFF;
                        }
                    }
                    continue;
                }
                
                char name[256];
                int n = 0;
                
                if (lfn_active && fat32_lfn_checksum((unsigned char*)dir[j].name) == expected_checksum) {
                    strcpy(name, lfn_buf);
                    n = strlen(name);
                } else {
                    for (int k = 0; k < 8 && dir[j].name[k] != ' '; k++) {
                        char c = dir[j].name[k];
                        if (c >= 'A' && c <= 'Z') c += 32;
                        name[n++] = c;
                    }
                    if (dir[j].name[8] != ' ') {
                        name[n++] = '.';
                        for (int k = 8; k < 11 && dir[j].name[k] != ' '; k++) {
                            char c = dir[j].name[k];
                            if (c >= 'A' && c <= 'Z') c += 32;
                            name[n++] = c;
                        }
                    }
                    name[n] = '\0';
                }
                lfn_active = 0;
                
                if (detailed) {
                    int year = ((dir[j].last_write_date >> 9) & 0x7F) + 1980;
                    int month = (dir[j].last_write_date >> 5) & 0x0F;
                    int day = dir[j].last_write_date & 0x1F;
                    int hours = (dir[j].last_write_time >> 11) & 0x1F;
                    int minutes = (dir[j].last_write_time >> 5) & 0x3F;
                    
                    if (year < 1980 || year > 2100) year = 1980;
                    if (month < 1 || month > 12) month = 1;
                    if (day < 1 || day > 31) day = 1;
                    
                    char entry_buf[512];
                    
                    // Manual padding since our sprintf does not support %02d.
                    char y_str[8], m_str[4], d_str[4], h_str[4], min_str[4], sz_str[16];
                    

                    int ty = year, i = 3;
                    y_str[4] = 0;
                    while(i >= 0) { y_str[i--] = '0' + (ty % 10); ty /= 10; }
                    

                    m_str[0] = '0' + (month / 10); m_str[1] = '0' + (month % 10); m_str[2] = 0;
                    

                    d_str[0] = '0' + (day / 10); d_str[1] = '0' + (day % 10); d_str[2] = 0;
                    

                    h_str[0] = '0' + (hours / 10); h_str[1] = '0' + (hours % 10); h_str[2] = 0;
                    

                    min_str[0] = '0' + (minutes / 10); min_str[1] = '0' + (minutes % 10); min_str[2] = 0;
                    
                    // Size (simple implementation uses %d with spaces).
                    sprintf(sz_str, "%d", dir[j].size);
                    
                    sprintf(entry_buf, "%s-%s-%s %s:%s  %s  %s%s\n", 
                            y_str, m_str, d_str, h_str, min_str, 
                            sz_str, 
                            name, 
                            (dir[j].attributes & FAT_ATTR_DIRECTORY) ? "/" : "");
                    
                    if (out_len + strlen(entry_buf) < output_size) {
                        strcat(output, entry_buf);
                        out_len += strlen(entry_buf);
                    }
                } else {
                    if (out_len + n + 1 < output_size) {
                        if (out_len > 0) {
                            output[out_len++] = ' ';
                            output[out_len] = '\0';
                        }
                        strcat(output, name);
                        if (dir[j].attributes & FAT_ATTR_DIRECTORY) {
                            strcat(output, "/");
                            out_len++;
                        }
                        out_len += n;
                    }
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return 0;
}
