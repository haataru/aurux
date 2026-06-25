#ifndef ATA_H
#define ATA_H

#include "../../kernel/kernel.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRV_HEAD     0x1F6
#define ATA_PRIMARY_COMM_STAT    0x1F7

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_CACHE_FLUSH      0xE7

#define ATA_SR_BSY               0x80
#define ATA_SR_DRDY              0x40
#define ATA_SR_DF                0x20
#define ATA_SR_DSC               0x10
#define ATA_SR_DRQ               0x08
#define ATA_SR_CORR              0x04
#define ATA_SR_IDX               0x02
#define ATA_SR_ERR               0x01

void ata_init(void);
int ata_read_sector(unsigned int lba, unsigned char* buffer);
int ata_read_sectors(unsigned int lba, unsigned char* buffer, unsigned int count);
int ata_write_sector(unsigned int lba, const unsigned char* buffer);
int ata_write_sectors(unsigned int lba, const unsigned char* buffer, unsigned int count);

#endif
