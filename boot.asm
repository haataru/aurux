.section .multiboot
.align 4
.long 0x1BADB002
.long 0x00
.long -(0x1BADB002 + 0x00)

.globl start
.globl load_idt
.extern OSmain
.extern keyboard_handler_main

# IDT entry structure
.set IDT_ENTRY_OFFSET_LOW, 0
.set IDT_ENTRY_SELECTOR, 2
.set IDT_ENTRY_ZERO, 4
.set IDT_ENTRY_FLAGS, 5
.set IDT_ENTRY_OFFSET_HIGH, 6
.set IDT_ENTRY_SIZE, 8

.section .text

load_idt:
    movl 4(%esp), %eax
    lidt (%eax)
    ret

keyboard_interrupt:
    pusha
    call keyboard_handler_main
    popa
    iret

default_interrupt:
    iret

.section .data
.globl idt
idt:
    .fill 256 * 8, 1, 0

idt_descriptor:
    .word 256 * 8 - 1
    .long idt

start:
    cli
    movl $stack_top, %esp
    
    movl $keyboard_interrupt, %eax
    
    movl $idt, %edi
    addl $33 * 8, %edi
    
    movw %ax, IDT_ENTRY_OFFSET_LOW(%edi)
    movw $0x08, IDT_ENTRY_SELECTOR(%edi)
    movb $0, IDT_ENTRY_ZERO(%edi)
    movb $0x8E, IDT_ENTRY_FLAGS(%edi)
    
    shrl $16, %eax
    movw %ax, IDT_ENTRY_OFFSET_HIGH(%edi)
    
    pushl $idt_descriptor
    call load_idt
    addl $4, %esp
    
    call remap_pic
    
    sti
    
    call OSmain
    
    jmp .

remap_pic:
    movb $0x11, %al
    outb %al, $0x20
    outb %al, $0xA0
    
    movb $0x20, %al
    outb %al, $0x21
    movb $0x28, %al
    outb %al, $0xA1
    
    movb $0x04, %al
    outb %al, $0x21
    movb $0x02, %al
    outb %al, $0xA1
    
    movb $0x01, %al
    outb %al, $0x21
    outb %al, $0xA1
    
    movb $0xFD, %al
    outb %al, $0x21
    movb $0xFF, %al
    outb %al, $0xA1
    
    ret

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
