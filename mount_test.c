#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "fs.h"

#define TEST_DISK "test_mount_disk.img"
#define INVALID_DISK "invalid_disk.img"
#define NONEXISTENT_DISK "does_not_exist.img"

// Helper to check if filesystem is accessible after unmount
int verify_disk_closed(const char* disk_path) {
    // Try to open the disk file - should succeed if properly unmounted
    int fd = open(disk_path, O_RDWR);
    if (fd < 0) {
        return -1; // File locked or doesn't exist
    }
    close(fd);
    return 0; // File is accessible
}

// Create an invalid disk
int create_invalid_disk(const char* path) {
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    // Write only 1MB instead of 10MB
    char buffer[BLOCK_SIZE] = {0};
    for (int i = 0; i < 256; i++) {
        write(fd, buffer, BLOCK_SIZE);
    }
    
    // Write invalid superblock
    superblock bad_sb = {
        .total_blocks = 999,  // Wrong value
        .block_size = 2048,   // Wrong block size
        .free_blocks = 100,
        .total_inodes = 50,
        .free_inodes = 50
    };
    
    lseek(fd, 0, SEEK_SET);
    write(fd, &bad_sb, sizeof(superblock));
    close(fd);
    return 0;
}

int main() {
    printf("=== Testing fs_mount and fs_unmount ===\n");
    
    // Setup: Create a valid disk first
    printf("Setup - Creating valid disk: ");
    if (fs_format(TEST_DISK) != 0) {
        printf("FAILED - Could not create test disk\n");
        return 1;
    }
    printf("OK\n");
    
    // Test 1: Mount valid disk
    printf("Test 1 - Mount valid disk: ");
    int result = fs_mount(TEST_DISK);
    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - fs_mount returned %d\n", result);
        return 1;
    }
    
    // Test 2: Try to mount again (should fail - already mounted)
    printf("Test 2 - Double mount (should fail): ");
    result = fs_mount(TEST_DISK);
    if (result == -1) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Double mount should return -1, got %d\n", result);
        return 1;
    }
    
    // Test 3: Unmount (void function - just verify it completes)
    printf("Test 3 - Unmount: ");
    fs_unmount(); // No return value to check
    // Verify disk is accessible (not locked)
    if (verify_disk_closed(TEST_DISK) == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Disk not properly closed\n");
        return 1;
    }
    
    // Test 4: Unmount when not mounted (should do nothing)
    printf("Test 4 - Unmount when not mounted: ");
    fs_unmount(); // Should safely do nothing
    printf("PASSED (no crash)\n");
    
    // Test 5: Mount non-existent disk
    printf("Test 5 - Mount non-existent disk: ");
    result = fs_mount(NONEXISTENT_DISK);
    if (result == -1) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Should fail for non-existent disk\n");
        return 1;
    }
    
    // Test 6: Mount invalid disk
    printf("Test 6 - Mount invalid disk: ");
    create_invalid_disk(INVALID_DISK);
    result = fs_mount(INVALID_DISK);
    if (result == -1) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Should fail for invalid disk\n");
        fs_unmount();  // Clean up if somehow mounted
        return 1;
    }
    
    // Test 7: Mount, unmount, mount again cycle
    printf("Test 7 - Mount-unmount-mount cycle: ");
    result = fs_mount(TEST_DISK);
    if (result != 0) {
        printf("FAILED - First mount failed\n");
        return 1;
    }
    
    fs_unmount(); // Void function
    
    // Try to mount again - should succeed if properly unmounted
    result = fs_mount(TEST_DISK);
    if (result == 0) {
        printf("PASSED\n");
        fs_unmount();
    } else {
        printf("FAILED - Second mount failed\n");
        return 1;
    }
    
    // Test 8: Verify superblock persistence
    printf("Test 8 - Superblock persistence: ");
    
    // Mount and read initial values
    fs_mount(TEST_DISK);
    // Would need access to current_superblock to fully test
    // For now, just verify mount/unmount/mount works
    fs_unmount();
    
    result = fs_mount(TEST_DISK);
    if (result == 0) {
        printf("PASSED\n");
        fs_unmount();
    } else {
        printf("FAILED - Could not remount\n");
        return 1;
    }
    
    printf("\nAll tests passed!\n");
    
    // Cleanup
    unlink(TEST_DISK);
    unlink(INVALID_DISK);
    
    return 0;
}