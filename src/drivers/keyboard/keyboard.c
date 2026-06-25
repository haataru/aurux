/*
 * keyboard.c - Keyboard input driver
 */

#include "keyboard.h"
#include "../../kernel/kernel.h"

#define KEY_BUFFER_SIZE 256

/* Scancode to character mappings (lowercase) */
static const char keymap_lower[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

/* Scancode to character mappings (uppercase) */
static const char keymap_upper[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};

static char key_buffer[KEY_BUFFER_SIZE];
static int key_read_pos = 0;
static int key_write_pos = 0;
static int shift_pressed = 0;
static int caps_lock = 0;

/*
 * Initialize keyboard driver
 */
void keyboard_init(void) {
    key_read_pos = 0;
    key_write_pos = 0;
    shift_pressed = 0;
    caps_lock = 0;
}

/*
 * Keyboard interrupt handler
 * Called from assembly ISR
 */
void keyboard_handler_main(void) {
    unsigned char scancode = inb(0x60);
    
    /* Handle key release (bit 7 set) */
    if (scancode & 0x80) {
        unsigned char key = scancode & 0x7F;
        if (key == 0x2A || key == 0x36) {
            shift_pressed = 0;
        }
    } else {
        /* Handle key press */
        if (scancode == 0x2A || scancode == 0x36) {
            /* Left or right shift pressed */
            shift_pressed = 1;
        } else if (scancode == 0x3A) {
            /* Caps lock toggle */
            caps_lock = !caps_lock;
        } else {
            char character;
            if (shift_pressed ^ caps_lock) {
                character = keymap_upper[scancode];
            } else {
                character = keymap_lower[scancode];
            }
            
            if (character != 0) {
                key_buffer[key_write_pos] = character;
                key_write_pos = (key_write_pos + 1) % KEY_BUFFER_SIZE;
            }
        }
    }
    
    /* Send End of Interrupt to PIC */
    outb(0x20, 0x20);
}

/*
 * Get a character from keyboard (blocking)
 */
char keyboard_getchar(void) {
    while (key_read_pos == key_write_pos) {
        asm volatile("hlt");
    }
    
    char c = key_buffer[key_read_pos];
    key_read_pos = (key_read_pos + 1) % KEY_BUFFER_SIZE;
    return c;
}

/*
 * Check if there's a character available
 */
int keyboard_has_char(void) {
    return key_read_pos != key_write_pos;
}

/*
 * Get a character without blocking (returns 0 if no char)
 */
char keyboard_getchar_nb(void) {
    if (key_read_pos == key_write_pos) {
        return 0;
    }
    
    char c = key_buffer[key_read_pos];
    key_read_pos = (key_read_pos + 1) % KEY_BUFFER_SIZE;
    return c;
}
