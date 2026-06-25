

#include "rtc.h"
#include "../../kernel/kernel.h"


void rtc_init(void) {
    // RTC is typically initialized by BIOS. Verify readability.
}


unsigned char rtc_bcdToBin(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}


void rtc_getTime(int* hours, int* minutes, int* seconds) {
    unsigned char h, m, s;
    
    asm volatile("cli");
    asm volatile("cli");
    
    // Wait for RTC update to complete
    outb(CMOS_ADDRESS, CMOS_STATUS_A);
    while (inb(CMOS_DATA) & 0x80);
    
    outb(CMOS_ADDRESS, CMOS_HOURS);
    h = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_MINUTES);
    m = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_SECONDS);
    s = inb(CMOS_DATA);
    
    asm volatile("sti");
    asm volatile("sti");
    
    *hours = rtc_bcdToBin(h);
    *minutes = rtc_bcdToBin(m);
    *seconds = rtc_bcdToBin(s);
    
    // Adjust for Moscow timezone (UTC+3)
    *hours = *hours + 3;
    if (*hours >= 24) {
        *hours = *hours - 24;
    }
}


void rtc_getDate(int* day, int* month, int* year) {
    unsigned char d, m, y;
    
    asm volatile("cli");
    asm volatile("cli");
    
    // Wait for RTC update to complete
    outb(CMOS_ADDRESS, CMOS_STATUS_A);
    while (inb(CMOS_DATA) & 0x80);
    
    outb(CMOS_ADDRESS, CMOS_DAY);
    d = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_MONTH);
    m = inb(CMOS_DATA);
    
    outb(CMOS_ADDRESS, CMOS_YEAR);
    y = inb(CMOS_DATA);
    
    asm volatile("sti");
    asm volatile("sti");
    
    *day = rtc_bcdToBin(d);
    *month = rtc_bcdToBin(m);
    *year = rtc_bcdToBin(y);
    
    // Year is 2-digit in RTC (e.g. 26 for 2026)
    if (*year < 80) {
        *year += 2000;
    } else {
        *year += 1900;
    }
}
