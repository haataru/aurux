#include "ata.h"
#include "../vga/vga.h"

static void ata_wait_bsy(void) {
    while (inb(ATA_PRIMARY_COMM_STAT) & ATA_SR_BSY);
}

static void ata_wait_drq(void) {
    while (!(inb(ATA_PRIMARY_COMM_STAT) & ATA_SR_DRQ));
}

void ata_init(void) {
    // Simple initialization, assuming Primary Master is present.
    // Additional IDENTIFY command could be implemented if required.
    vga_print("ATA PIO Primary Master initialized.\n");
}

static volatile int ata_lock = 0;

static void acquire_ata_lock() {
    while (__sync_lock_test_and_set(&ata_lock, 1)) {
        extern void yield(void);
        yield();
    }
}

static void release_ata_lock() {
    __sync_lock_release(&ata_lock);
}

int ata_read_sector(unsigned int lba, unsigned char* buffer) {
    acquire_ata_lock();
    ata_wait_bsy();

    // Select drive and send highest 4 bits of LBA
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_ERR, 0x00);
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (unsigned char)lba);
    outb(ATA_PRIMARY_LBA_MID, (unsigned char)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (unsigned char)(lba >> 16));
    
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_READ_PIO);

    ata_wait_bsy();
    ata_wait_drq();

    // Read 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        unsigned short word = inw(ATA_PRIMARY_DATA);
        buffer[i * 2] = (unsigned char)word;
        buffer[i * 2 + 1] = (unsigned char)(word >> 8);
    }
    
    release_ata_lock();
    return 0;
}

int ata_read_sectors(unsigned int lba, unsigned char* buffer, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        ata_read_sector(lba + i, buffer + (i * 512));
    }
    return 0;
}

int ata_write_sector(unsigned int lba, const unsigned char* buffer) {
    acquire_ata_lock();
    ata_wait_bsy();

    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_ERR, 0x00);
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (unsigned char)lba);
    outb(ATA_PRIMARY_LBA_MID, (unsigned char)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (unsigned char)(lba >> 16));
    
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_WRITE_PIO);

    ata_wait_bsy();
    ata_wait_drq();

    for (int i = 0; i < 256; i++) {
        unsigned short word = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_PRIMARY_DATA, word);
    }
    
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
    
    release_ata_lock();
    return 0;
}

int ata_write_sectors(unsigned int lba, const unsigned char* buffer, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        ata_write_sector(lba + i, buffer + (i * 512));
    }
    return 0;
}
