#include "pit.h"

volatile unsigned int timer_ticks = 0;

void pit_init(unsigned int frequency) {
    // The value sent to the PIT divides its input clock (1193180 Hz) to achieve the target frequency.
    unsigned int divisor = 1193180 / frequency;

    // Send command byte: channel 0, lobyte/hibyte, square wave, binary
    outb(0x43, 0x36);

    // Split divisor into upper and lower bytes for byte-wise transmission
    unsigned char l = (unsigned char)(divisor & 0xFF);
    unsigned char h = (unsigned char)( (divisor>>8) & 0xFF );

    outb(0x40, l);
    outb(0x40, h);
}

unsigned int get_ticks(void) {
    return timer_ticks;
}
