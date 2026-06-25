#ifndef RTC_H
#define RTC_H

void rtc_init(void);
void rtc_getTime(int* hours, int* minutes, int* seconds);
void rtc_getDate(int* day, int* month, int* year);
unsigned char rtc_bcdToBin(unsigned char bcd);

#endif
