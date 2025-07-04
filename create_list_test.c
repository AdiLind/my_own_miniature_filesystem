#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "fs.h"

#define TEST_DISK "test_create_disk.img"

// Test helper: Check if a file exists in the filesystem
int file_exists(const char* filename) {
    char files[MAX_FILES][MAX_FILENAME];
    int count = fs_list(files, MAX_FILES);
    
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i], filename) == 0) {
            return 1;
        }
    }
    return 0;
}

// Test helper: Get current free inodes count
int get_free_inodes_count() {
    // This would need access to current_superblock
    // For now, we'll use fs_list to count used inodes
    char files[MAX_FILES][MAX_FILENAME];
    int used = fs_list(files, MAX_FILES);
    return MAX_FILES - used;
}

int main() {
    printf("=== Testing fs_create ===\n");
    
    // Setup: Create and mount a fresh filesystem
    printf("Setup - Creating and mounting filesystem: ");
    if (fs_format(TEST_DISK) != 0) {
        printf("FAILED - Could not format disk\n");
        return 1;
    }
    if (fs_mount(TEST_DISK) != 0) {
        printf("FAILED - Could not mount disk\n");
        return 1;
    }
    printf("OK\n");
    
    // Test 1: Create a simple file
    printf("Test 1 - Create simple file: ");
    int result = fs_create("test.txt");
    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - fs_create returned %d\n", result);
        return 1;
    }
    
    // Test 2: Create file with maximum length name (28 chars)
    printf("Test 2 - Create file with max length name: ");
    char max_name[MAX_FILENAME + 1];
    memset(max_name, 'a', MAX_FILENAME);
    max_name[MAX_FILENAME] = '\0';
    result = fs_create(max_name);
    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - fs_create returned %d\n", result);
        return 1;
    }
    
    // Test 3: Try to create existing file (should fail with -1)
    printf("Test 3 - Create existing file (should fail): ");
    result = fs_create("test.txt");
    if (result == -1) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Expected -1, got %d\n", result);
        return 1;
    }
    
    // Test 4: Create file with empty name (should fail with -3)
    printf("Test 4 - Create file with empty name: ");
    result = fs_create("");
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Expected -3, got %d\n", result);
        return 1;
    }
    
    // Test 5: Create file with NULL name (should fail with -3)
    printf("Test 5 - Create file with NULL name: ");
    result = fs_create(NULL);
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Expected -3, got %d\n", result);
        return 1;
    }
    
    // Test 6: Create file with name too long (should fail with -3)
    printf("Test 6 - Create file with name too long: ");
    char long_name[50];
    memset(long_name, 'b', 49);
    long_name[49] = '\0';
    result = fs_create(long_name);
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Expected -3, got %d\n", result);
        return 1;
    }
    
    // Test 7: Create multiple files
    printf("Test 7 - Create multiple files: ");
    int created_count = 2; // Already created 2 files
    char filename[MAX_FILENAME];
    
    for (int i = 0; i < 10; i++) {
        sprintf(filename, "file%d.dat", i);
        if (fs_create(filename) == 0) {
            created_count++;
        } else {
            printf("FAILED - Could not create %s\n", filename);
            return 1;
        }
    }
    printf("PASSED (created %d files total)\n", created_count);
    
    // Test 8: Create files with various valid characters
    printf("Test 8 - Create files with special characters: ");
    const char* special_names[] = {
        "file_with_underscore",
        "file-with-dash",
        "file.with.dots",
        "FILE123",
        "123file",
        "MiXeD_CaSe.TxT"
    };
    
    for (int i = 0; i < 6; i++) {
        if (fs_create(special_names[i]) != 0) {
            printf("FAILED - Could not create '%s'\n", special_names[i]);
            return 1;
        }
    }
    printf("PASSED\n");
    
    // Test 9: Unmount and remount to verify persistence
    printf("Test 9 - Persistence after unmount/mount: ");
    fs_unmount();
    
    if (fs_mount(TEST_DISK) != 0) {
        printf("FAILED - Could not remount\n");
        return 1;
    }
    
    // Verify files still exist (need fs_list to be implemented)
    // For now, try to create "test.txt" again - should fail if it persisted
    result = fs_create("test.txt");
    if (result == -1) {
        printf("PASSED (file persisted)\n");
    } else {
        printf("FAILED - File did not persist\n");
        return 1;
    }
    
    // Test 10: Fill up all inodes
    printf("Test 10 - Fill all inodes (test -2 error): ");
    int files_created = 0;
    
    // Create files until we run out of inodes
    for (int i = 0; i < MAX_FILES; i++) {
        sprintf(filename, "bulk%d", i);
        result = fs_create(filename);
        if (result == 0) {
            files_created++;
        } else if (result == -1) {
            // File exists, continue
            continue;
        } else if (result == -2) {
            // No free inodes - this is what we expect eventually
            break;
        } else {
            printf("FAILED - Unexpected error %d\n", result);
            return 1;
        }
    }
    
    // Now try one more - should definitely fail with -2
    result = fs_create("should_fail");
    if (result == -2) {
        printf("PASSED (created %d files before full)\n", files_created);
    } else {
        printf("FAILED - Expected -2, got %d\n", result);
        return 1;
    }
    
    // Test 11: Create file when not mounted
    printf("Test 11 - Create when not mounted: ");
    fs_unmount();
    
    result = fs_create("unmounted.txt");
    if (result == -3) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Expected -3, got %d\n", result);
        return 1;
    }
    
    printf("\nAll tests passed!\n");
    
    // Cleanup
    unlink(TEST_DISK);
    
    return 0;
}