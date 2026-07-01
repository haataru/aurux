#include "../../lib/lib.h"

void pass(const char* test_name) {
    print("[OK]   ");
    print(test_name);
    print("\n");
}

void fail(const char* test_name, const char* reason) {
    print("[FAIL] ");
    print(test_name);
    print(" - ");
    print(reason);
    print("\n");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    print("=== Aurux Permission Test Suite ===\n");
    
    // Test 1: Verify we are not root
    int uid = getuid();
    if (uid == 0) {
        fail("User check", "Please run this test as a normal user (e.g. lykhe), not root");
        return 1;
    } else {
        pass("User check");
    }
    
    // Test 2: Umask and file creation
    int old_mask = umask(0077); // R/W only for user
    if (create_file("test_file.txt") != 0) {
        fail("File creation", "Could not create test_file.txt in current dir");
    } else {
        int fd = open("test_file.txt");
        if (fd >= 0) close(fd);
        struct fs_stat st;
        if (stat("test_file.txt", &st) == 0) {
            if ((st.mode & 0777) == 0600) {
                pass("Umask and File mode check");
            } else {
                fail("Umask and File mode check", "File mode is incorrect");
            }
        } else {
            fail("File stat", "Could not stat test_file.txt");
        }
        unlink("test_file.txt");
    }
    umask(old_mask);
    
    // Test 3: Read restricted file (root owned)
    int fd2 = open("/etc/passwd");
    if (fd2 >= 0) close(fd2);
    int fd3 = open("/root");
    if (fd3 == -2) { // Permission denied
        pass("Read restricted folder /root");
    } else {
        fail("Read restricted folder /root", "Should be Permission Denied (-2)");
        if (fd3 >= 0) close(fd3);
    }
    
    // Test 4: Open SUID binary
    struct fs_stat su_st;
    if (stat("/bin/su.elf", &su_st) == 0) {
        if (su_st.mode & 0x0800) { // SUID
            pass("SUID bit on /bin/su.elf");
        } else {
            fail("SUID bit on /bin/su.elf", "SUID bit not set");
        }
    } else {
        fail("Stat /bin/su.elf", "Could not stat binary");
    }

    print("=== All tests completed ===\n");
    return 0;
}
