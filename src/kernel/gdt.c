#include "gdt.h"
#include "../memory/memory.h"

extern void gdt_flush(unsigned int);
extern void tss_flush(void);

static struct gdt_entry gdt[6];
static struct gdt_ptr gdt_p;
static struct tss_entry tss;

static void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

static void write_tss(int num, unsigned short ss0, unsigned int esp0) {
    unsigned int base = (unsigned int)&tss;
    unsigned int limit = base + sizeof(tss);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);


    unsigned char* ptr = (unsigned char*)&tss;
    for(unsigned int i = 0; i < sizeof(tss); i++) {
        ptr[i] = 0;
    }

    tss.ss0  = ss0;
    tss.esp0 = esp0;
    tss.cs   = 0x0B; // User code segment with RPL 3.
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13; // User data segment with RPL 3.
    tss.iomap_base = sizeof(tss);
}

void gdt_init(void) {
    gdt_p.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gdt_p.base  = (unsigned int)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    write_tss(5, 0x10, 0x0);

    gdt_flush((unsigned int)&gdt_p);
    tss_flush();
}

void set_kernel_stack(unsigned int stack) {
    tss.esp0 = stack;
}
