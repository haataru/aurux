/* Interrupt handling: IDT, IRQ, PIC */
#include "kernel.h"

/* IDT entry */
struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short offset_high;
} __attribute__((packed));

/* IDT ptr */
struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

/* IDT */
static struct idt_entry idt[256];
static struct idt_ptr idt_ptr;

void idt_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (unsigned int)&idt;
    
    /* Clear */
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].flags = 0;
        idt[i].offset_high = 0;
    }
    
}

void idt_set_gate(unsigned char num, unsigned int handler, unsigned short selector, unsigned char flags) {
void idt_set_gate(unsigned char num, unsigned int handler, unsigned short selector, unsigned char flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

void pic_remap(void) {
    /* ICW1: Start initialization */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    
    /* ICW2: Vector offsets */
    outb(0x21, 0x20);  /* IRQ0-7 -> vectors 0x20-0x27 */
    outb(0xA1, 0x28);  /* IRQ8-15 -> vectors 0x28-0x2F */
    
    /* ICW3: Master/Slave wiring */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    
    /* ICW4: 8086 mode */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    /* Disable all IRQs except keyboard (IRQ1) */
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

/*
 * Enable IRQ
 */
void irq_enable(int irq) {
    if (irq < 8) {
        unsigned char mask = inb(0x21);
        mask &= ~(1 << irq);
        outb(0x21, mask);
    } else {
        unsigned char mask = inb(0xA1);
        mask &= ~(1 << (irq - 8));
        outb(0xA1, mask);
    }
}

/*
 * Disable IRQ
 */
void irq_disable(int irq) {
    if (irq < 8) {
        unsigned char mask = inb(0x21);
        mask |= (1 << irq);
        outb(0x21, mask);
    } else {
        unsigned char mask = inb(0xA1);
        mask |= (1 << (irq - 8));
        outb(0xA1, mask);
    }
}

/*
 * Send End of Interrupt to PIC
 */
void eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}
