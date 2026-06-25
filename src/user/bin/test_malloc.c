#include "../../lib/lib.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    print("Testing malloc...\n");
    
    char* str1 = (char*)malloc(100);
    if (!str1) {
        print("malloc(100) failed\n");
        return 1;
    }
    
    strcpy(str1, "Hello from dynamically allocated memory!\n");
    print(str1);
    
    char* str2 = (char*)malloc(200);
    if (!str2) {
        print("malloc(200) failed\n");
        return 1;
    }
    
    strcpy(str2, "Second allocation successful.\n");
    print(str2);
    
    free(str1);
    print("Freed str1\n");
    
    char* str3 = (char*)malloc(50);
    if (!str3) {
        print("malloc(50) failed\n");
        return 1;
    }
    
    strcpy(str3, "Third allocation (should reuse block 1).\n");
    print(str3);
    
    free(str2);
    free(str3);
    print("All memory freed.\n");
    
    return 0;
}
