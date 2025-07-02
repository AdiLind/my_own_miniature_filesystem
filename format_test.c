// compile with: gcc -o format_test format_test.c fs.c -Wall -Wextra

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "fs.h"

#define TEST_DISK "test_disk.img"

// Helper function to verify file size
int verify_disk_size(const char* path) {
    struct stat file_stats;
    if (stat(path, &file_stats) != 0) {
        return -1;
    }
    // Should be 10MB (2560 blocks * 4096 bytes)
    return (file_stats.st_size == MAX_BLOCKS * BLOCK_SIZE) ? 0 : -1;
}

// Helper function to read and verify superblock
int verify_superblock(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    superblock sb;
    if (read(fd, &sb, sizeof(superblock)) != sizeof(superblock)) {
        close(fd);
        return -1;
    }
    close(fd);
    
    // Verify superblock values
    if (sb.total_blocks != MAX_BLOCKS ||
        sb.block_size != BLOCK_SIZE ||
        sb.free_blocks != (MAX_BLOCKS - 10) ||
        sb.total_inodes != MAX_FILES ||
        sb.free_inodes != MAX_FILES) {
        return -1;
    }
    return 0;
}

// Helper function to verify bitmap
int verify_bitmap(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    // Skip superblock
    lseek(fd, BLOCK_SIZE, SEEK_SET);
    
    unsigned char bitmap[BLOCK_SIZE];
    if (read(fd, bitmap, BLOCK_SIZE) != BLOCK_SIZE) {
        close(fd);
        return -1;
    }
    close(fd);
    
    // Check that blocks 0-9 are marked as used
    for (int i = 0; i < 10; i++) {
        if (!(bitmap[i/8] & (1 << (i%8)))) {
            printf("Block %d should be marked as used but isn't\n", i);
            return -1;
        }
    }
    
    // Check that blocks 10+ are marked as free
    for (int i = 10; i < 20; i++) {  // Just check first 20 blocks
        if (bitmap[i/8] & (1 << (i%8))) {
            printf("Block %d should be marked as free but isn't\n", i);
            return -1;
        }
    }
    
    return 0;
}

// Helper function to verify inode table
int verify_inode_table(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    // Skip to inode table (block 2)
    lseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
    
    inode test_inode;
    // Check first few inodes
    for (int i = 0; i < 5; i++) {
        if (read(fd, &test_inode, sizeof(inode)) != sizeof(inode)) {
            close(fd);
            return -1;
        }
        
        // All inodes should be unused initially
        if (test_inode.used != 0) {
            printf("Inode %d should be unused but has used=%d\n", i, test_inode.used);
            close(fd);
            return -1;
        }
    }
    
    close(fd);
    return 0;
}

int main() {
    printf("=== Testing fs_format ===\n");
    
    // Test 1: Basic format
    printf("Test 1 - Basic format: ");
    int result = fs_format(TEST_DISK);
    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - fs_format returned %d\n", result);
        return 1;
    }
    
    // Test 2: Verify file size
    printf("Test 2 - Disk size verification: ");
    if (verify_disk_size(TEST_DISK) == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Incorrect disk size\n");
        return 1;
    }
    
    // Test 3: Verify superblock
    printf("Test 3 - Superblock verification: ");
    if (verify_superblock(TEST_DISK) == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Incorrect superblock values\n");
        return 1;
    }
    
    // Test 4: Verify bitmap
    printf("Test 4 - Bitmap verification: ");
    if (verify_bitmap(TEST_DISK) == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Incorrect bitmap initialization\n");
        return 1;
    }
    
    // Test 5: Verify inode table
    printf("Test 5 - Inode table verification: ");
    if (verify_inode_table(TEST_DISK) == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Incorrect inode table initialization\n");
        return 1;
    }
    
    // Test 6: Format existing disk (should overwrite)
    printf("Test 6 - Format existing disk: ");
    result = fs_format(TEST_DISK);
    if (result == 0 && verify_disk_size(TEST_DISK) == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED - Could not reformat existing disk\n");
        return 1;
    }
    
    printf("\nAll tests passed!\n");
    
    // Cleanup
    unlink(TEST_DISK);
    
    return 0;
}