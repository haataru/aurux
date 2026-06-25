/*
 * rtc.c - Real-Time Clock driver
 */

#include "rtc.h"
#include "../../kernel/kernel.h"

/*
 * Initialize RTC driver
 */
void rtc_init(void) {
    /* RTC is typically already initialized by BIOS */
    /* We just verify we can read from it */
}

/*
 * Convert BCD to binary
 */
unsigned char rtc_bcdToBin(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/*
 * Get current time
 */
void rtc_getTime(int* hours, int* minutes, int* seconds) {
    unsigned char h, m, s;
    
    /* Disable interrupts while reading RTC */
    asm volatile("cli");
    
    /* Wait for RTC update to complete */
    outb(CMOS_ADDRESS, CMOS_STATUS_A);
    while (inb(CMOS_DATA) & 0x80);
    
    /* Read hours */
    outb(CMOS_ADDRESS, CMOS_HOURS);
    h = inb(CMOS_DATA);
    
    /* Read minutes */
    outb(CMOS_ADDRESS, CMOS_MINUTES);
    m = inb(CMOS_DATA);
    
    /* Read seconds */
    outb(CMOS_ADDRESS, CMOS_SECONDS);
    s = inb(CMOS_DATA);
    
    /* Re-enable interrupts */
    asm volatile("sti");
    
    /* Convert from BCD to binary */
    *hours = rtc_bcdToBin(h);
    *minutes = rtc_bcdToBin(m);
    *seconds = rtc_bcdToBin(s);
    
    /* Adjust for Moscow timezone (UTC+3) */
    *hours = *hours + 3;
    if (*hours >= 24) {
        *hours = *hours - 24;
    }
}

/*
 * Get current date
 */
void rtc_getDate(int* day, int* month, int* year) {
    unsigned char d, m, y;
    
    /* Disable interrupts while reading RTC */
    asm volatile("cli");
    
    /* Wait for RTC update to complete */
    outb(CMOS_ADDRESS, CMOS_STATUS_A);
    while (inb(CMOS_DATA) & 0x80);
    
    /* Read day */
    outb(CMOS_ADDRESS, CMOS_DAY);
    d = inb(CMOS_DATA);
    
    /* Read month */
    outb(CMOS_ADDRESS, CMOS_MONTH);
    m = inb(CMOS_DATA);
    
    /* Read year */
    outb(CMOS_ADDRESS, CMOS_YEAR);
    y = inb(CMOS_DATA);
    
    /* Re-enable interrupts */
    asm volatile("sti");
    
    /* Convert from BCD to binary */
    *day = rtc_bcdToBin(d);
    *month = rtc_bcdToBin(m);
    *year = rtc_bcdToBin(y);
}
