#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define TEST_DISK "test_disk.img"
#define BACKUP_DISK "backup_disk.img"

// Color codes for output
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define YELLOW "\033[0;33m"
#define RESET "\033[0m"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

// Helper function to print test results
void print_test_result(const char* test_name, int passed) {
    if (passed) {
        printf("%s[PASS]%s %s\n", GREEN, RESET, test_name);
        tests_passed++;
    } else {
        printf("%s[FAIL]%s %s\n", RED, RESET, test_name);
        tests_failed++;
    }
}

// Helper function to clean up test disk
void cleanup_test_disk() {
    unlink(TEST_DISK);
    unlink(BACKUP_DISK);
}

// Test 1: Basic format and mount
void test_format_and_mount() {
    printf("\n%s=== Test 1: Basic Format and Mount ===%s\n", YELLOW, RESET);
    
    // Test format
    int result = fs_format(TEST_DISK);
    print_test_result("Format filesystem", result == 0);
    
    // Test mount
    result = fs_mount(TEST_DISK);
    print_test_result("Mount filesystem", result == 0);
    
    // Test double mount (should fail)
    result = fs_mount(TEST_DISK);
    print_test_result("Double mount fails", result == -1);
    
    // Unmount
    fs_unmount();
    
    // Test mount non-existent disk
    result = fs_mount("non_existent.img");
    print_test_result("Mount non-existent disk fails", result == -1);
}

// Test 2: File creation
void test_file_creation() {
    printf("\n%s=== Test 2: File Creation ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // Create a file
    int result = fs_create("test.txt");
    print_test_result("Create file", result == 0);
    
    // Create duplicate file (should fail)
    result = fs_create("test.txt");
    print_test_result("Create duplicate file fails", result == -1);
    
    // Create file with max length name (28 chars)
    char max_name[MAX_FILENAME];
    memset(max_name, 'a', MAX_FILENAME - 1);
    max_name[MAX_FILENAME - 1] = '\0';
    result = fs_create(max_name);
    print_test_result("Create file with max length name", result == 0);
    
    // Create file with too long name
    char long_name[MAX_FILENAME + 10];
    memset(long_name, 'b', MAX_FILENAME + 5);
    long_name[MAX_FILENAME + 5] = '\0';
    result = fs_create(long_name);
    print_test_result("Create file with too long name fails", result == -3);
    
    // Create file with empty name
    result = fs_create("");
    print_test_result("Create file with empty name fails", result == -3);
    
    // Create file with NULL name
    result = fs_create(NULL);
    print_test_result("Create file with NULL name fails", result == -3);
    
    fs_unmount();
}

// Test 3: File listing
void test_file_listing() {
    printf("\n%s=== Test 3: File Listing ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    char filenames[10][MAX_FILENAME];
    
    // List empty filesystem
    int count = fs_list(filenames, 10);
    print_test_result("List empty filesystem", count == 0);
    
    // Create some files
    fs_create("file1.txt");
    fs_create("file2.txt");
    fs_create("file3.txt");
    
    // List files
    count = fs_list(filenames, 10);
    print_test_result("List 3 files", count == 3);
    
    // Verify file names
    int found_file1 = 0, found_file2 = 0, found_file3 = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(filenames[i], "file1.txt") == 0) found_file1 = 1;
        if (strcmp(filenames[i], "file2.txt") == 0) found_file2 = 1;
        if (strcmp(filenames[i], "file3.txt") == 0) found_file3 = 1;
    }
    print_test_result("All files found in listing", found_file1 && found_file2 && found_file3);
    
    // List with limited buffer
    count = fs_list(filenames, 2);
    print_test_result("List with limited buffer", count == 2);
    
    fs_unmount();
}

// Test 4: Basic read/write
void test_basic_read_write() {
    printf("\n%s=== Test 4: Basic Read/Write ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // Create file
    fs_create("data.txt");
    
    // Write data
    const char* test_data = "Hello, World!";
    int result = fs_write("data.txt", test_data, strlen(test_data));
    print_test_result("Write data to file", result == 0);
    
    // Read data back
    char buffer[100] = {0};
    int bytes_read = fs_read("data.txt", buffer, sizeof(buffer));
    print_test_result("Read data from file", bytes_read == strlen(test_data));
    print_test_result("Data matches", strcmp(buffer, test_data) == 0);
    
    // Write to non-existent file
    result = fs_write("nonexistent.txt", test_data, strlen(test_data));
    print_test_result("Write to non-existent file fails", result == -1);
    
    // Read from non-existent file
    bytes_read = fs_read("nonexistent.txt", buffer, sizeof(buffer));
    print_test_result("Read from non-existent file fails", bytes_read == -1);
    
    fs_unmount();
}

// Test 5: Large file operations
void test_large_files() {
    printf("\n%s=== Test 5: Large File Operations ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    fs_create("large.bin");
    
    // Create data that spans multiple blocks
    const int data_size = BLOCK_SIZE * 3 + 1000; // 3 blocks + partial block
    char* large_data = malloc(data_size);
    for (int i = 0; i < data_size; i++) {
        large_data[i] = (char)(i % 256);
    }
    
    // Write large data
    int result = fs_write("large.bin", large_data, data_size);
    print_test_result("Write multi-block file", result == 0);
    
    // Read it back
    char* read_buffer = malloc(data_size);
    int bytes_read = fs_read("large.bin", read_buffer, data_size);
    print_test_result("Read multi-block file", bytes_read == data_size);
    
    // Verify data integrity
    int data_matches = 1;
    for (int i = 0; i < data_size; i++) {
        if (read_buffer[i] != large_data[i]) {
            data_matches = 0;
            break;
        }
    }
    print_test_result("Multi-block data integrity", data_matches);
    
    // Test maximum file size (48KB)
    const int max_size = MAX_DIRECT_BLOCKS * BLOCK_SIZE;
    char* max_data = malloc(max_size);
    memset(max_data, 'X', max_size);
    
    fs_create("maxfile.bin");
    result = fs_write("maxfile.bin", max_data, max_size);
    print_test_result("Write maximum size file (48KB)", result == 0);
    
    // Try to write more than maximum
    result = fs_write("maxfile.bin", max_data, max_size + 1);
    print_test_result("Write beyond maximum size fails", result == -2);
    
    free(large_data);
    free(read_buffer);
    free(max_data);
    
    fs_unmount();
}

// Test 6: File deletion
void test_file_deletion() {
    printf("\n%s=== Test 6: File Deletion ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // Create and delete file
    fs_create("temp.txt");
    int result = fs_delete("temp.txt");
    print_test_result("Delete existing file", result == 0);
    
    // Try to read deleted file
    char buffer[10];
    int bytes_read = fs_read("temp.txt", buffer, sizeof(buffer));
    print_test_result("Read deleted file fails", bytes_read == -1);
    
    // Delete non-existent file
    result = fs_delete("nonexistent.txt");
    print_test_result("Delete non-existent file fails", result == -1);
    
    // Create file with data, delete, and reuse space
    fs_create("file1.txt");
    const char* data = "This is test data";
    fs_write("file1.txt", data, strlen(data));
    fs_delete("file1.txt");
    
    // Create new file and verify space is reused
    fs_create("file2.txt");
    result = fs_write("file2.txt", data, strlen(data));
    print_test_result("Reuse deleted file space", result == 0);
    
    fs_unmount();
}

// Test 7: Filesystem capacity
void test_filesystem_capacity() {
    printf("\n%s=== Test 7: Filesystem Capacity ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // Create maximum number of files
    char filename[MAX_FILENAME];
    int files_created = 0;
    
    for (int i = 0; i < MAX_FILES + 10; i++) {
        sprintf(filename, "file%d.txt", i);
        if (fs_create(filename) == 0) {
            files_created++;
        } else {
            break;
        }
    }
    
    print_test_result("Create maximum files (256)", files_created == MAX_FILES);
    
    // Try to create one more
    int result = fs_create("overflow.txt");
    print_test_result("Create beyond max files fails", result == -2);
    
    // Delete one and create again
    fs_delete("file0.txt");
    result = fs_create("newfile.txt");
    print_test_result("Create after delete when full", result == 0);
    
    fs_unmount();
}

// Test 8: Persistence
void test_persistence() {
    printf("\n%s=== Test 8: Persistence ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // Create files and write data
    fs_create("persist1.txt");
    fs_create("persist2.txt");
    
    const char* data1 = "Persistent data 1";
    const char* data2 = "Persistent data 2";
    
    fs_write("persist1.txt", data1, strlen(data1));
    fs_write("persist2.txt", data2, strlen(data2));
    
    // Unmount
    fs_unmount();
    
    // Mount again
    int result = fs_mount(TEST_DISK);
    print_test_result("Remount filesystem", result == 0);
    
    // Check files exist
    char buffer[100];
    int bytes_read = fs_read("persist1.txt", buffer, sizeof(buffer));
    print_test_result("File 1 persists after remount", bytes_read == strlen(data1));
    
    buffer[bytes_read] = '\0';
    print_test_result("File 1 data persists correctly", strcmp(buffer, data1) == 0);
    
    memset(buffer, 0, sizeof(buffer));
    bytes_read = fs_read("persist2.txt", buffer, sizeof(buffer));
    print_test_result("File 2 persists after remount", bytes_read == strlen(data2));
    
    fs_unmount();
}

// Test 9: Edge cases and error handling
void test_edge_cases() {
    printf("\n%s=== Test 9: Edge Cases ===%s\n", YELLOW, RESET);
    
    // Operations on unmounted filesystem
    cleanup_test_disk();
    
    int result = fs_create("test.txt");
    print_test_result("Create on unmounted fs fails", result == -3);
    
    char buffer[10];
    result = fs_read("test.txt", buffer, sizeof(buffer));
    print_test_result("Read on unmounted fs fails", result == -1);
    
    result = fs_write("test.txt", "data", 4);
    print_test_result("Write on unmounted fs fails", result == -3);
    
    result = fs_delete("test.txt");
    print_test_result("Delete on unmounted fs fails", result == -2);
    
    char filenames[10][MAX_FILENAME];
    result = fs_list(filenames, 10);
    print_test_result("List on unmounted fs fails", result == -1);
    
    // Mount and test edge cases
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // NULL parameters
    result = fs_write(NULL, "data", 4);
    print_test_result("Write with NULL filename fails", result == -3);
    
    result = fs_write("test.txt", NULL, 4);
    print_test_result("Write with NULL data fails", result == -3);
    
    result = fs_read(NULL, buffer, sizeof(buffer));
    print_test_result("Read with NULL filename fails", result == -3);
    
    result = fs_read("test.txt", NULL, sizeof(buffer));
    print_test_result("Read with NULL buffer fails", result == -3);
    
    // Zero and negative sizes
    fs_create("test.txt");
    result = fs_write("test.txt", "data", 0);
    print_test_result("Write with size 0 fails", result == -3);
    
    result = fs_write("test.txt", "data", -1);
    print_test_result("Write with negative size fails", result == -3);
    
    result = fs_read("test.txt", buffer, 0);
    print_test_result("Read with size 0 fails", result == -3);
    
    result = fs_read("test.txt", buffer, -1);
    print_test_result("Read with negative size fails", result == -3);
    
    fs_unmount();
}

// Test 10: Stress test - fill disk with data
void test_disk_full() {
    printf("\n%s=== Test 10: Disk Full Stress Test ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    // Create large data buffer
    const int block_data_size = BLOCK_SIZE;
    char* block_data = malloc(block_data_size);
    memset(block_data, 'D', block_data_size);
    
    // Keep creating files until disk is full
    int files_created = 0;
    int total_blocks_used = 0;
    char filename[MAX_FILENAME];
    
    while (files_created < MAX_FILES) {
        sprintf(filename, "bigfile%d.bin", files_created);
        
        if (fs_create(filename) != 0) {
            break;
        }
        
        // Try to write maximum data to this file
        int blocks_written = 0;
        for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
            if (fs_write(filename, block_data, block_data_size) == 0) {
                blocks_written++;
                total_blocks_used++;
            } else {
                break;
            }
        }
        
        if (blocks_written == 0) {
            // No space left, delete the empty file
            fs_delete(filename);
            break;
        }
        
        files_created++;
    }
    
    printf("  Created %d files using %d data blocks\n", files_created, total_blocks_used);
    print_test_result("Fill disk with data", total_blocks_used > 0);
    
    // Try to write more data (should fail)
    fs_create("overflow.txt");
    int result = fs_write("overflow.txt", block_data, block_data_size);
    print_test_result("Write to full disk fails", result == -2);
    
    // Delete a file and try again
    fs_delete("bigfile0.bin");
    result = fs_write("overflow.txt", block_data, block_data_size);
    print_test_result("Write after freeing space succeeds", result == 0);
    
    free(block_data);
    fs_unmount();
}

// Test 11: Partial reads
void test_partial_reads() {
    printf("\n%s=== Test 11: Partial Reads ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    fs_create("partial.txt");
    
    const char* data = "This is a test string for partial reads";
    fs_write("partial.txt", data, strlen(data));
    
    // Read less than file size
    char small_buffer[10];
    int bytes_read = fs_read("partial.txt", small_buffer, sizeof(small_buffer));
    print_test_result("Partial read returns correct size", bytes_read == sizeof(small_buffer));
    
    // Read more than file size
    char large_buffer[100];
    bytes_read = fs_read("partial.txt", large_buffer, sizeof(large_buffer));
    print_test_result("Read beyond file size returns file size", bytes_read == strlen(data));
    
    // Verify data
    large_buffer[bytes_read] = '\0';
    print_test_result("Full read data matches", strcmp(large_buffer, data) == 0);
    
    fs_unmount();
}

// Test 12: File overwrite
void test_file_overwrite() {
    printf("\n%s=== Test 12: File Overwrite ===%s\n", YELLOW, RESET);
    
    fs_format(TEST_DISK);
    fs_mount(TEST_DISK);
    
    fs_create("overwrite.txt");
    
    // Write initial data
    const char* data1 = "Initial data";
    fs_write("overwrite.txt", data1, strlen(data1));
    
    // Overwrite with shorter data
    const char* data2 = "New";
    int result = fs_write("overwrite.txt", data2, strlen(data2));
    print_test_result("Overwrite with shorter data", result == 0);
    
    // Read and verify
    char buffer[100];
    int bytes_read = fs_read("overwrite.txt", buffer, sizeof(buffer));
    buffer[bytes_read] = '\0';
    print_test_result("Overwritten file size correct", bytes_read == strlen(data2));
    print_test_result("Overwritten data correct", strcmp(buffer, data2) == 0);
    
    // Overwrite with longer data
    const char* data3 = "This is much longer data than before";
    result = fs_write("overwrite.txt", data3, strlen(data3));
    print_test_result("Overwrite with longer data", result == 0);
    
    memset(buffer, 0, sizeof(buffer));
    bytes_read = fs_read("overwrite.txt", buffer, sizeof(buffer));
    buffer[bytes_read] = '\0';
    print_test_result("Re-overwritten data correct", strcmp(buffer, data3) == 0);
    
    fs_unmount();
}

// Main test runner
int main() {
    printf("\n%s========================================%s\n", YELLOW, RESET);
    printf("%s     FILE SYSTEM TEST SUITE%s\n", YELLOW, RESET);
    printf("%s========================================%s\n", YELLOW, RESET);
    
    // Run all tests
    test_format_and_mount();
    test_file_creation();
    test_file_listing();
    test_basic_read_write();
    test_large_files();
    test_file_deletion();
    test_filesystem_capacity();
    test_persistence();
    test_edge_cases();
    test_disk_full();
    test_partial_reads();
    test_file_overwrite();
    
    // Print summary
    printf("\n%s========================================%s\n", YELLOW, RESET);
    printf("Test Summary:\n");
    printf("  %sPassed: %d%s\n", GREEN, tests_passed, RESET);
    printf("  %sFailed: %d%s\n", RED, tests_failed, RESET);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n%sAll tests passed! 🎉%s\n", GREEN, RESET);
    } else {
        printf("\n%sSome tests failed. Please review the output above.%s\n", RED, RESET);
    }
    
    // Cleanup
    cleanup_test_disk();
    
    return tests_failed > 0 ? 1 : 0;
}