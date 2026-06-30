#include "lib.h"


int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}


int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}


size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}


size_t strnlen(const char* str, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && str[len]) len++;
    return len;
}


char* strcpy(char* dest, const char* src) {
    char* ret = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return ret;
}


char* strncpy(char* dest, const char* src, size_t n) {
    char* ret = dest;
    while (n > 0 && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n > 0) {
        *dest++ = '\0';
        n--;
    }
    return ret;
}


char* strcat(char* dest, const char* src) {
    char* ret = dest;
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
    return ret;
}


char* strncat(char* dest, const char* src, size_t n) {
    char* ret = dest;
    while (*dest) dest++;
    while (n > 0 && *src) {
        *dest++ = *src++;
        n--;
    }
    *dest = '\0';
    return ret;
}


void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}


void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}


void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}


int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}


char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}


char* strrchr(const char* s, int c) {
    const char* found = NULL;
    while (*s) {
        if (*s == (char)c) {
            found = s;
        }
        s++;
    }
    return (char*)found;
}


void hex_to_str(unsigned int val, char* buf) {
    const char hex[] = "0123456789ABCDEF";
    int i = 0;
    
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    char temp[16];
    int pos = 0;
    
    while (val > 0) {
        temp[pos++] = hex[val & 0xF];
        val >>= 4;
    }
    
    // Reverse
    for (int j = pos - 1; j >= 0; j--) {
        buf[i++] = temp[j];
    }
    buf[i] = '\0';
}
