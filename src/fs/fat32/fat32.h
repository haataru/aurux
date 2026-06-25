#ifndef FAT32_H
#define FAT32_H

#include "../../kernel/kernel.h"

struct fat32_bpb {
    unsigned char  jmp[3];
    char           oem[8];
    unsigned short bytes_per_sector;
    unsigned char  sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char  fat_count;
    unsigned short root_dir_entries;
    unsigned short total_sectors_short;
    unsigned char  media_descriptor;
    unsigned short fat_size_16;
    unsigned short sectors_per_track;
    unsigned short heads;
    unsigned int   hidden_sectors;
    unsigned int   total_sectors_long;
    // FAT32 specific fields.
    unsigned int   fat_size_32;
    unsigned short ext_flags;
    unsigned short fs_version;
    unsigned int   root_cluster;
    unsigned short fs_info;
    unsigned short backup_boot_sector;
    unsigned char  reserved[12];
    unsigned char  drive_number;
    unsigned char  reserved1;
    unsigned char  boot_signature;
    unsigned int   volume_id;
    char           volume_label[11];
    char           fs_type[8];
} __attribute__((packed));

struct fat32_dir_entry {
    char           name[11];
    unsigned char  attributes;
    unsigned char  reserved;
    unsigned char  creation_time_tenths;
    unsigned short creation_time;
    unsigned short creation_date;
    unsigned short last_access_date;
    unsigned short cluster_high;
    unsigned short last_write_time;
    unsigned short last_write_date;
    unsigned short cluster_low;
    unsigned int   size;
} __attribute__((packed));

struct fat32_lfn_entry {
    unsigned char  order;
    unsigned short name1[5];
    unsigned char  attributes;
    unsigned char  type;
    unsigned char  checksum;
    unsigned short name2[6];
    unsigned short first_cluster; // Always zero.
    unsigned short name3[2];
} __attribute__((packed));

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

void fat32_init(void);
int fat32_read_file(const char* filename, unsigned char* buffer);
int fat32_read_file_ex(const char* filename, unsigned char* buffer, size_t size, unsigned int offset);
int fat32_create_file(const char* filename, unsigned char attr);
int fat32_write_file(const char* filename, const unsigned char* buffer, unsigned int size);
int fat32_delete_file(const char* filename);
int fat32_list_dir(const char* path, char* output, unsigned int output_size, int detailed);
int fat32_get_file_size(const char* filename);
int fat32_is_dir(const char* path);

#endif
