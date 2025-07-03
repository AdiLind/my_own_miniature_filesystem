#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "fs.h"

#define TEST_DISK "test_create_disk.img"

int main() {
    printf("=== Testing fs_create (Simple Version) ===\n");
    
    // Setup
    printf("Setup - Format and mount: ");
    if (fs_format(TEST_DISK) != 0) {
        printf("FAILED - format\n");
        return 1;
    }
    if (fs_mount(TEST_DISK) != 0) {
        printf("FAILED - mount\n");
        return 1;
    }
    printf("OK\n");
    
    // Test 1: Create simple file
    printf("Test 1 - Create simple file: ");
    int result = fs_create("test.txt");
    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED (returned %d)\n", result);
    }
    
    // Test 2: Create duplicate (should fail)
    printf("Test 2 - Create duplicate: ");
    result = fs_create("test.txt");
    if (result == -1) {
        printf("PASSED\n");
    } else {
        printf("FAILED (expected -1, got %d)\n", result);
    }
    
    // Test 3: Create with empty name
    printf("Test 3 - Empty name: ");
    result = fs_create("");
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED (expected -3, got %d)\n", result);
    }
    
    // Test 4: Create with NULL
    printf("Test 4 - NULL name: ");
    result = fs_create(NULL);
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED (expected -3, got %d)\n", result);
    }
    
    // Test 5: Long name
    printf("Test 5 - Name too long: ");
    char long_name[50];
    memset(long_name, 'a', 49);
    long_name[49] = '\0';
    result = fs_create(long_name);
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED (expected -3, got %d)\n", result);
    }
    
    // Test 6: Persistence
    printf("Test 6 - Persistence: ");
    fs_unmount();
    fs_mount(TEST_DISK);
    result = fs_create("test.txt"); // Should exist
    if (result == -1) {
        printf("PASSED\n");
    } else {
        printf("FAILED (file didn't persist)\n");
    }
    
    fs_unmount();
    unlink(TEST_DISK);
    printf("\nDone!\n");
    return 0;
}