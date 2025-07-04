/**
 * @file test_fs.c
 * @brief Comprehensive test suite for OnlyFiles filesystem implementation
 * 
 * This test suite verifies:
 * 1. Individual helper functions
 * 2. fs_write functionality according to homework requirements
 * 3. Integration tests using all implemented functions
 * 
 * Compile with: gcc -o test_fs test_fs.c fs.c -std=c17
 * Run with: ./test_fs
 */

#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

// Test counter and results
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test disk path
#define TEST_DISK "test_disk.img"

// Test macros
#define TEST_ASSERT(condition, test_name) do { \
    tests_run++; \
    if (condition) { \
        printf("✅ PASS: %s\n", test_name); \
        tests_passed++; \
    } else { \
        printf("❌ FAIL: %s\n", test_name); \
        tests_failed++; \
    } \
} while(0)

#define TEST_SECTION(section_name) \
    printf("\n" "=" "=" "=" " %s " "=" "=" "=" "\n", section_name)

// Helper function to create a clean test environment
void setup_test_environment() {
    // Remove old test disk if exists
    unlink(TEST_DISK);
    
    // Format and mount filesystem
    int result = fs_format(TEST_DISK);
    assert(result == 0);
    
    result = fs_mount(TEST_DISK);
    assert(result == 0);
}

void cleanup_test_environment() {
    fs_unmount();
    unlink(TEST_DISK);
}

// =========== HELPER FUNCTION TESTS ===========

void test_compare_strings() {
    TEST_SECTION("Testing compare_strings function");
    
    // Test identical strings
    TEST_ASSERT(compare_strings("hello", "hello") == 0, "Identical strings should return 0");
    
    // Test different strings
    TEST_ASSERT(compare_strings("hello", "world") != 0, "Different strings should not return 0");
    
    // Test string comparison ordering
    TEST_ASSERT(compare_strings("abc", "abd") < 0, "First string lexicographically smaller");
    TEST_ASSERT(compare_strings("abd", "abc") > 0, "First string lexicographically larger");
    
    // Test empty strings
    TEST_ASSERT(compare_strings("", "") == 0, "Empty strings should be equal");
    TEST_ASSERT(compare_strings("hello", "") > 0, "Non-empty vs empty string");
    
    // Test NULL handling
    TEST_ASSERT(compare_strings(NULL, NULL) == 0, "NULL strings should return 0");
}

void test_validate_write_operation_parameters() {
    TEST_SECTION("Testing validate_write_operation_parameters function");
    
    setup_test_environment();
    
    char test_data[] = "Hello World";
    
    // Valid parameters
    TEST_ASSERT(validate_write_operation_parameters("test.txt", test_data, 11) == 0, 
                "Valid parameters should pass");
    
    // NULL filename
    TEST_ASSERT(validate_write_operation_parameters(NULL, test_data, 11) == -3, 
                "NULL filename should return -3");
    
    // NULL data
    TEST_ASSERT(validate_write_operation_parameters("test.txt", NULL, 11) == -3, 
                "NULL data should return -3");
    
    // Zero size
    TEST_ASSERT(validate_write_operation_parameters("test.txt", test_data, 0) == -3, 
                "Zero size should return -3");
    
    // Negative size
    TEST_ASSERT(validate_write_operation_parameters("test.txt", test_data, -1) == -3, 
                "Negative size should return -3");
    
    // Empty filename
    TEST_ASSERT(validate_write_operation_parameters("", test_data, 11) == -3, 
                "Empty filename should return -3");
    
    // Filename too long
    char long_name[MAX_FILENAME + 5];
    memset(long_name, 'a', MAX_FILENAME + 4);
    long_name[MAX_FILENAME + 4] = '\0';
    TEST_ASSERT(validate_write_operation_parameters(long_name, test_data, 11) == -3, 
                "Too long filename should return -3");
    
    // File too large (> 48KB)
    TEST_ASSERT(validate_write_operation_parameters("test.txt", test_data, MAX_DIRECT_BLOCKS * BLOCK_SIZE + 1) == -2, 
                "File too large should return -2");
    
    cleanup_test_environment();
}

void test_bitmap_operations() {
    TEST_SECTION("Testing bitmap read/write operations");
    
    setup_test_environment();
    
    unsigned char test_bitmap[BLOCK_SIZE];
    memset(test_bitmap, 0, BLOCK_SIZE);
    
    // Test writing and reading bitmap
    test_bitmap[0] = 0xFF; // Set first 8 bits
    test_bitmap[1] = 0x0F; // Set first 4 bits of second byte
    
    int write_result = write_bitmap_to_disk(test_bitmap);
    TEST_ASSERT(write_result == 0, "Writing bitmap should succeed");
    
    unsigned char read_bitmap[BLOCK_SIZE];
    memset(read_bitmap, 0, BLOCK_SIZE);
    
    int read_result = read_bitmap_from_disk(read_bitmap);
    TEST_ASSERT(read_result == 0, "Reading bitmap should succeed");
    
    TEST_ASSERT(read_bitmap[0] == 0xFF, "First byte should match");
    TEST_ASSERT(read_bitmap[1] == 0x0F, "Second byte should match");
    
    cleanup_test_environment();
}

void test_block_allocation_functions() {
    TEST_SECTION("Testing block allocation functions");
    
    setup_test_environment();
    
    // Test finding free blocks
    int free_block = find_free_block();
    TEST_ASSERT(free_block >= 10, "First free block should be >= 10 (after metadata)");
    TEST_ASSERT(free_block < MAX_BLOCKS, "Free block should be within valid range");
    
    // Test marking block as used
    mark_block_as_used(free_block);
    
    // Try to find the same block again - should get a different one
    int next_free_block = find_free_block();
    TEST_ASSERT(next_free_block != free_block, "Should find different block after marking one as used");
    TEST_ASSERT(next_free_block == free_block + 1, "Should find next consecutive block");
    
    // Test marking block as free
    mark_block_as_free(free_block);
    
    // Now should be able to find the original block again
    int freed_block = find_free_block();
    TEST_ASSERT(freed_block == free_block, "Should find previously freed block");
    
    cleanup_test_environment();
}

void test_inode_operations() {
    TEST_SECTION("Testing inode operations");
    
    setup_test_environment();
    
    // Create a test file first
    int create_result = fs_create("test_inode.txt");
    TEST_ASSERT(create_result == 0, "Creating test file should succeed");
    
    // Test finding inode by name
    int inode_index = find_inode_by_name("test_inode.txt");
    TEST_ASSERT(inode_index >= 0, "Should find existing file inode");
    TEST_ASSERT(inode_index < MAX_FILES, "Inode index should be within valid range");
    
    // Test reading inode from disk
    inode test_inode;
    read_inode_from_disk(inode_index, &test_inode);
    TEST_ASSERT(test_inode.used == 1, "Inode should be marked as used");
    TEST_ASSERT(strcmp(test_inode.name, "test_inode.txt") == 0, "Inode name should match");
    TEST_ASSERT(test_inode.size == 0, "New file should have size 0");
    
    // Test finding non-existent file
    int non_existent = find_inode_by_name("non_existent.txt");
    TEST_ASSERT(non_existent < 0, "Non-existent file should return negative");
    
    cleanup_test_environment();
}

void test_space_checking() {
    TEST_SECTION("Testing space availability checking");
    
    setup_test_environment();
    
    // Create a small test file
    int create_result = fs_create("space_test.txt");
    TEST_ASSERT(create_result == 0, "Creating test file should succeed");
    
    // Test with reasonable space requirements
    int space_result = check_available_space_for_write_operation(5, 0);
    TEST_ASSERT(space_result == 0, "Should have space for 5 blocks");
    
    // Test with excessive space requirements
    int excessive_result = check_available_space_for_write_operation(3000, 0);
    TEST_ASSERT(excessive_result == -2, "Should not have space for 3000 blocks");
    
    cleanup_test_environment();
}

// =========== FS_WRITE SPECIFIC TESTS ===========

void test_fs_write_basic_functionality() {
    TEST_SECTION("Testing fs_write basic functionality");
    
    setup_test_environment();
    
    // Create a test file
    int create_result = fs_create("write_test.txt");
    TEST_ASSERT(create_result == 0, "Creating test file should succeed");
    
    // Test writing small data
    char small_data[] = "Hello, World!";
    int write_result = fs_write("write_test.txt", small_data, strlen(small_data));
    TEST_ASSERT(write_result == 0, "Writing small data should succeed");
    
    // Verify file size was updated
    int inode_index = find_inode_by_name("write_test.txt");
    inode file_inode;
    read_inode_from_disk(inode_index, &file_inode);
    TEST_ASSERT(file_inode.size == (int)strlen(small_data), "File size should be updated");
    TEST_ASSERT(file_inode.blocks[0] != 0, "File should have at least one block allocated");
    
    cleanup_test_environment();
}

void test_fs_write_error_conditions() {
    TEST_SECTION("Testing fs_write error conditions");
    
    setup_test_environment();
    
    char test_data[] = "Test data";
    
    // Test writing to non-existent file
    int result = fs_write("non_existent.txt", test_data, strlen(test_data));
    TEST_ASSERT(result == -1, "Writing to non-existent file should return -1");
    
    // Create a file for other tests
    fs_create("error_test.txt");
    
    // Test NULL filename
    result = fs_write(NULL, test_data, strlen(test_data));
    TEST_ASSERT(result == -3, "NULL filename should return -3");
    
    // Test NULL data
    result = fs_write("error_test.txt", NULL, strlen(test_data));
    TEST_ASSERT(result == -3, "NULL data should return -3");
    
    // Test negative size
    result = fs_write("error_test.txt", test_data, -1);
    TEST_ASSERT(result == -3, "Negative size should return -3");
    
    // Test file too large
    result = fs_write("error_test.txt", test_data, MAX_DIRECT_BLOCKS * BLOCK_SIZE + 1);
    TEST_ASSERT(result == -2, "Too large file should return -2");
    
    cleanup_test_environment();
}

void test_fs_write_various_sizes() {
    TEST_SECTION("Testing fs_write with various file sizes");
    
    setup_test_environment();
    
    // Test 1: Small file (less than one block)
    fs_create("small.txt");
    char small_data[100];
    memset(small_data, 'A', 99);
    small_data[99] = '\0';
    
    int result = fs_write("small.txt", small_data, 99);
    TEST_ASSERT(result == 0, "Writing small file should succeed");
    
    // Test 2: Exactly one block
    fs_create("oneblock.txt");
    char block_data[BLOCK_SIZE];
    memset(block_data, 'B', BLOCK_SIZE);
    
    result = fs_write("oneblock.txt", block_data, BLOCK_SIZE);
    TEST_ASSERT(result == 0, "Writing one block should succeed");
    
    // Test 3: Multiple blocks
    fs_create("multiblock.txt");
    char multi_data[BLOCK_SIZE * 3];
    memset(multi_data, 'C', BLOCK_SIZE * 3);
    
    result = fs_write("multiblock.txt", multi_data, BLOCK_SIZE * 3);
    TEST_ASSERT(result == 0, "Writing multiple blocks should succeed");
    
    // Test 4: Maximum file size
    fs_create("maxsize.txt");
    char max_data[MAX_DIRECT_BLOCKS * BLOCK_SIZE];
    memset(max_data, 'D', MAX_DIRECT_BLOCKS * BLOCK_SIZE);
    
    result = fs_write("maxsize.txt", max_data, MAX_DIRECT_BLOCKS * BLOCK_SIZE);
    TEST_ASSERT(result == 0, "Writing maximum size file should succeed");
    
    cleanup_test_environment();
}

void test_fs_write_overwrite_functionality() {
    TEST_SECTION("Testing fs_write overwrite functionality");
    
    setup_test_environment();
    
    // Create and write initial data
    fs_create("overwrite_test.txt");
    char initial_data[] = "This is the initial data that takes up some space in the file.";
    int result = fs_write("overwrite_test.txt", initial_data, strlen(initial_data));
    TEST_ASSERT(result == 0, "Initial write should succeed");
    
    // Get initial inode state
    int inode_index = find_inode_by_name("overwrite_test.txt");
    inode initial_inode;
    read_inode_from_disk(inode_index, &initial_inode);
    int initial_blocks = (initial_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // Overwrite with smaller data
    char smaller_data[] = "Small";
    result = fs_write("overwrite_test.txt", smaller_data, strlen(smaller_data));
    TEST_ASSERT(result == 0, "Overwrite with smaller data should succeed");
    
    // Verify new size
    inode updated_inode;
    read_inode_from_disk(inode_index, &updated_inode);
    TEST_ASSERT(updated_inode.size == (int)strlen(smaller_data), "Size should be updated to smaller size");
    
    // Overwrite with larger data
    char larger_data[BLOCK_SIZE * 2];
    memset(larger_data, 'X', BLOCK_SIZE * 2 - 1);
    larger_data[BLOCK_SIZE * 2 - 1] = '\0';
    
    result = fs_write("overwrite_test.txt", larger_data, BLOCK_SIZE * 2 - 1);
    TEST_ASSERT(result == 0, "Overwrite with larger data should succeed");
    
    // Verify new size
    read_inode_from_disk(inode_index, &updated_inode);
    TEST_ASSERT(updated_inode.size == BLOCK_SIZE * 2 - 1, "Size should be updated to larger size");
    
    cleanup_test_environment();
}

// =========== INTEGRATION TESTS ===========

void test_full_workflow_integration() {
    TEST_SECTION("Testing full filesystem workflow integration");
    
    setup_test_environment();
    
    // Test 1: Create multiple files
    TEST_ASSERT(fs_create("file1.txt") == 0, "Creating file1 should succeed");
    TEST_ASSERT(fs_create("file2.txt") == 0, "Creating file2 should succeed");
    TEST_ASSERT(fs_create("file3.txt") == 0, "Creating file3 should succeed");
    
    // Test 2: List files
    char filenames[10][MAX_FILENAME];
    int file_count = fs_list(filenames, 10);
    TEST_ASSERT(file_count == 3, "Should list 3 files");
    
    // Test 3: Write different data to each file
    char data1[] = "This is file 1 content";
    char data2[] = "File 2 has different content that is longer than file 1";
    char data3[BLOCK_SIZE + 100];
    memset(data3, 'Z', BLOCK_SIZE + 99);
    data3[BLOCK_SIZE + 99] = '\0';
    
    TEST_ASSERT(fs_write("file1.txt", data1, strlen(data1)) == 0, "Writing to file1 should succeed");
    TEST_ASSERT(fs_write("file2.txt", data2, strlen(data2)) == 0, "Writing to file2 should succeed");
    TEST_ASSERT(fs_write("file3.txt", data3, strlen(data3)) == 0, "Writing to file3 should succeed");
    
    // Test 4: Verify files have correct sizes
    int inode1 = find_inode_by_name("file1.txt");
    int inode2 = find_inode_by_name("file2.txt");
    int inode3 = find_inode_by_name("file3.txt");
    
    inode file1_inode, file2_inode, file3_inode;
    read_inode_from_disk(inode1, &file1_inode);
    read_inode_from_disk(inode2, &file2_inode);
    read_inode_from_disk(inode3, &file3_inode);
    
    TEST_ASSERT(file1_inode.size == (int)strlen(data1), "File1 should have correct size");
    TEST_ASSERT(file2_inode.size == (int)strlen(data2), "File2 should have correct size");
    TEST_ASSERT(file3_inode.size == (int)strlen(data3), "File3 should have correct size");
    
    // Test 5: Overwrite files with different sizes
    char new_data1[] = "New content for file 1";
    char new_data2[BLOCK_SIZE * 2];
    memset(new_data2, 'Y', BLOCK_SIZE * 2 - 1);
    new_data2[BLOCK_SIZE * 2 - 1] = '\0';
    
    TEST_ASSERT(fs_write("file1.txt", new_data1, strlen(new_data1)) == 0, "Overwriting file1 should succeed");
    TEST_ASSERT(fs_write("file2.txt", new_data2, strlen(new_data2)) == 0, "Overwriting file2 should succeed");
    
    // Test 6: Verify overwritten files
    read_inode_from_disk(inode1, &file1_inode);
    read_inode_from_disk(inode2, &file2_inode);
    
    TEST_ASSERT(file1_inode.size == (int)strlen(new_data1), "File1 should have new size");
    TEST_ASSERT(file2_inode.size == (int)strlen(new_data2), "File2 should have new size");
    
    cleanup_test_environment();
}

void test_filesystem_limits() {
    TEST_SECTION("Testing filesystem limits and edge cases");
    
    setup_test_environment();
    
    // Test 1: Create many files to test inode exhaustion
    char filename[MAX_FILENAME];
    int created_files = 0;
    
    for (int i = 0; i < MAX_FILES + 10; i++) {
        snprintf(filename, sizeof(filename), "file_%d.txt", i);
        int result = fs_create(filename);
        if (result == 0) {
            created_files++;
        } else if (result == -2) {
            // No more free inodes
            break;
        }
    }
    
    TEST_ASSERT(created_files <= MAX_FILES, "Should not create more files than MAX_FILES");
    printf("📊 Created %d files (max: %d)\n", created_files, MAX_FILES);
    
    // Test 2: Write to multiple files to test block exhaustion
    char large_data[BLOCK_SIZE * 10];
    memset(large_data, 'L', BLOCK_SIZE * 10 - 1);
    large_data[BLOCK_SIZE * 10 - 1] = '\0';
    
    int files_written = 0;
    for (int i = 0; i < created_files; i++) {
        snprintf(filename, sizeof(filename), "file_%d.txt", i);
        int result = fs_write(filename, large_data, BLOCK_SIZE * 10 - 1);
        if (result == 0) {
            files_written++;
        } else if (result == -2) {
            // Out of space
            break;
        }
    }
    
    printf("📊 Successfully wrote large data to %d files\n", files_written);
    
    cleanup_test_environment();
}

void test_unmount_remount_persistence() {
    TEST_SECTION("Testing unmount/remount data persistence");
    
    setup_test_environment();
    
    // Create and write data
    fs_create("persistent.txt");
    char test_data[] = "This data should persist across unmount/mount cycles";
    fs_write("persistent.txt", test_data, strlen(test_data));
    
    // Unmount and remount
    fs_unmount();
    int mount_result = fs_mount(TEST_DISK);
    TEST_ASSERT(mount_result == 0, "Remounting should succeed");
    
    // Verify file still exists
    int inode_index = find_inode_by_name("persistent.txt");
    TEST_ASSERT(inode_index >= 0, "File should still exist after remount");
    
    // Verify file size
    inode persistent_inode;
    read_inode_from_disk(inode_index, &persistent_inode);
    TEST_ASSERT(persistent_inode.size == (int)strlen(test_data), "File size should persist");
    
    cleanup_test_environment();
}

// =========== MAIN TEST RUNNER ===========

int main() {
    printf("🧪 OnlyFiles Filesystem Test Suite\n");
    printf("==================================\n");
    
    // Helper function tests
    test_compare_strings();
    test_validate_write_operation_parameters();
    test_bitmap_operations();
    test_block_allocation_functions();
    test_inode_operations();
    test_space_checking();
    
    // fs_write specific tests
    test_fs_write_basic_functionality();
    test_fs_write_error_conditions();
    test_fs_write_various_sizes();
    test_fs_write_overwrite_functionality();
    
    // Integration tests
    test_full_workflow_integration();
    test_filesystem_limits();
    test_unmount_remount_persistence();
    
    // Print final results
    printf("\n" "=" "=" "=" " TEST RESULTS " "=" "=" "=" "\n");
    printf("Total tests run: %d\n", tests_run);
    printf("✅ Passed: %d\n", tests_passed);
    printf("❌ Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL TESTS PASSED! Your filesystem implementation is working correctly!\n");
        return 0;
    } else {
        printf("\n⚠️  Some tests failed. Please review the implementation.\n");
        return 1;
    }
}