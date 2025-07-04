/**
 * @file simple_test.c
 * @brief Simple test suite that only tests public API functions
 * 
 * This avoids all the implicit declaration warnings by only calling
 * functions that are declared in fs.h
 * 
 * Compile with: gcc -o simple_test simple_test.c fs.c -std=c17
 * Run with: ./simple_test
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
    printf("\n" "═══ %s ═══\n", section_name)

// Helper function to create a clean test environment
void setup_test_environment() {
    unlink(TEST_DISK);
    int result = fs_format(TEST_DISK);
    assert(result == 0);
    result = fs_mount(TEST_DISK);
    assert(result == 0);
}

void cleanup_test_environment() {
    fs_unmount();
    unlink(TEST_DISK);
}

// =========== PUBLIC API TESTS ===========

void test_fs_format_and_mount() {
    TEST_SECTION("Testing fs_format and fs_mount");
    
    unlink(TEST_DISK);
    
    // Test formatting
    int format_result = fs_format(TEST_DISK);
    TEST_ASSERT(format_result == 0, "fs_format should succeed");
    
    // Test mounting
    int mount_result = fs_mount(TEST_DISK);
    TEST_ASSERT(mount_result == 0, "fs_mount should succeed");
    
    // Test double mount
    int double_mount = fs_mount(TEST_DISK);
    TEST_ASSERT(double_mount == -1, "Double mount should fail");
    
    fs_unmount();
    
    // Test mount non-existent
    unlink(TEST_DISK);
    int mount_nonexistent = fs_mount(TEST_DISK);
    TEST_ASSERT(mount_nonexistent == -1, "Mount non-existent should fail");
}

void test_fs_create() {
    TEST_SECTION("Testing fs_create");
    
    setup_test_environment();
    
    // Valid create
    TEST_ASSERT(fs_create("test1.txt") == 0, "Valid create should succeed");
    
    // Duplicate create
    TEST_ASSERT(fs_create("test1.txt") == -1, "Duplicate create should return -1");
    
    // NULL filename
    TEST_ASSERT(fs_create(NULL) == -3, "NULL filename should return -3");
    
    // Empty filename
    TEST_ASSERT(fs_create("") == -3, "Empty filename should return -3");
    
    // Too long filename
    char long_name[MAX_FILENAME + 5];
    memset(long_name, 'a', MAX_FILENAME + 4);
    long_name[MAX_FILENAME + 4] = '\0';
    TEST_ASSERT(fs_create(long_name) == -3, "Too long filename should return -3");
    
    cleanup_test_environment();
}

void test_fs_list() {
    TEST_SECTION("Testing fs_list");
    
    setup_test_environment();
    
    // Empty filesystem
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    TEST_ASSERT(count == 0, "Empty filesystem should list 0 files");
    
    // Create files and list
    fs_create("file1.txt");
    fs_create("file2.txt");
    fs_create("file3.txt");
    
    count = fs_list(filenames, 10);
    TEST_ASSERT(count == 3, "Should list 3 files");
    
    // Limited listing
    count = fs_list(filenames, 2);
    TEST_ASSERT(count == 2, "Limited list should return 2");
    
    cleanup_test_environment();
}

void test_fs_write_basic() {
    TEST_SECTION("Testing fs_write basic functionality");
    
    setup_test_environment();
    
    // Create file and write
    fs_create("write_test.txt");
    char data[] = "Hello, World!";
    int result = fs_write("write_test.txt", data, strlen(data));
    TEST_ASSERT(result == 0, "Basic write should succeed");
    
    cleanup_test_environment();
}

void test_fs_write_errors() {
    TEST_SECTION("Testing fs_write error conditions");
    
    setup_test_environment();
    
    char data[] = "Test data";
    
    // File not found
    TEST_ASSERT(fs_write("nonexistent.txt", data, strlen(data)) == -1, 
                "Write to non-existent file should return -1");
    
    fs_create("test.txt");
    
    // NULL filename
    TEST_ASSERT(fs_write(NULL, data, strlen(data)) == -3, 
                "NULL filename should return -3");
    
    // NULL data
    TEST_ASSERT(fs_write("test.txt", NULL, strlen(data)) == -3, 
                "NULL data should return -3");
    
    // Zero size
    TEST_ASSERT(fs_write("test.txt", data, 0) == -3, 
                "Zero size should return -3");
    
    // Negative size
    TEST_ASSERT(fs_write("test.txt", data, -1) == -3, 
                "Negative size should return -3");
    
    // File too large
    TEST_ASSERT(fs_write("test.txt", data, MAX_DIRECT_BLOCKS * BLOCK_SIZE + 1) == -2, 
                "File too large should return -2");
    
    cleanup_test_environment();
}

void test_fs_write_sizes() {
    TEST_SECTION("Testing fs_write with different sizes");
    
    setup_test_environment();
    
    // Small file
    fs_create("small.txt");
    char small_data[100];
    memset(small_data, 'A', 99);
    small_data[99] = '\0';
    TEST_ASSERT(fs_write("small.txt", small_data, 99) == 0, 
                "Small file write should succeed");
    
    // One block file
    fs_create("oneblock.txt");
    char *block_data = malloc(BLOCK_SIZE);
    memset(block_data, 'B', BLOCK_SIZE);
    TEST_ASSERT(fs_write("oneblock.txt", block_data, BLOCK_SIZE) == 0, 
                "One block write should succeed");
    free(block_data);
    
    // Multi-block file
    fs_create("multi.txt");
    char *multi_data = malloc(BLOCK_SIZE * 3);
    memset(multi_data, 'C', BLOCK_SIZE * 3);
    TEST_ASSERT(fs_write("multi.txt", multi_data, BLOCK_SIZE * 3) == 0, 
                "Multi-block write should succeed");
    free(multi_data);
    
    // Maximum size file
    fs_create("max.txt");
    char *max_data = malloc(MAX_DIRECT_BLOCKS * BLOCK_SIZE);
    memset(max_data, 'D', MAX_DIRECT_BLOCKS * BLOCK_SIZE);
    TEST_ASSERT(fs_write("max.txt", max_data, MAX_DIRECT_BLOCKS * BLOCK_SIZE) == 0, 
                "Max size write should succeed");
    free(max_data);
    
    cleanup_test_environment();
}

void test_fs_write_overwrite() {
    TEST_SECTION("Testing fs_write overwrite functionality");
    
    setup_test_environment();
    
    fs_create("overwrite.txt");
    
    // Initial write
    char initial[] = "This is initial data that is quite long";
    TEST_ASSERT(fs_write("overwrite.txt", initial, strlen(initial)) == 0, 
                "Initial write should succeed");
    
    // Overwrite with smaller
    char smaller[] = "Small";
    TEST_ASSERT(fs_write("overwrite.txt", smaller, strlen(smaller)) == 0, 
                "Overwrite with smaller should succeed");
    
    // Overwrite with larger
    char *larger = malloc(BLOCK_SIZE * 2);
    memset(larger, 'X', BLOCK_SIZE * 2 - 1);
    larger[BLOCK_SIZE * 2 - 1] = '\0';
    TEST_ASSERT(fs_write("overwrite.txt", larger, BLOCK_SIZE * 2 - 1) == 0, 
                "Overwrite with larger should succeed");
    free(larger);
    
    cleanup_test_environment();
}

void test_full_integration() {
    TEST_SECTION("Testing full integration workflow");
    
    setup_test_environment();
    
    // Create multiple files
    TEST_ASSERT(fs_create("file1.txt") == 0, "Create file1");
    TEST_ASSERT(fs_create("file2.txt") == 0, "Create file2");
    TEST_ASSERT(fs_create("file3.txt") == 0, "Create file3");
    
    // List files
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    TEST_ASSERT(count == 3, "Should list 3 files");
    
    // Write to files
    char data1[] = "File 1 content";
    char data2[] = "File 2 has longer content than file 1";
    char *data3 = malloc(BLOCK_SIZE + 100);
    memset(data3, 'Z', BLOCK_SIZE + 99);
    data3[BLOCK_SIZE + 99] = '\0';
    
    TEST_ASSERT(fs_write("file1.txt", data1, strlen(data1)) == 0, "Write file1");
    TEST_ASSERT(fs_write("file2.txt", data2, strlen(data2)) == 0, "Write file2");
    TEST_ASSERT(fs_write("file3.txt", data3, strlen(data3)) == 0, "Write file3");
    
    // Overwrite files
    char new_data1[] = "New content";
    char *new_data2 = malloc(BLOCK_SIZE * 2);
    memset(new_data2, 'Y', BLOCK_SIZE * 2 - 1);
    new_data2[BLOCK_SIZE * 2 - 1] = '\0';
    
    TEST_ASSERT(fs_write("file1.txt", new_data1, strlen(new_data1)) == 0, "Overwrite file1");
    TEST_ASSERT(fs_write("file2.txt", new_data2, strlen(new_data2)) == 0, "Overwrite file2");
    
    free(data3);
    free(new_data2);
    cleanup_test_environment();
}

void test_persistence() {
    TEST_SECTION("Testing data persistence");
    
    setup_test_environment();
    
    // Create and write
    fs_create("persistent.txt");
    char data[] = "This should persist";
    fs_write("persistent.txt", data, strlen(data));
    
    // Unmount and remount
    fs_unmount();
    TEST_ASSERT(fs_mount(TEST_DISK) == 0, "Remount should succeed");
    
    // Check file still exists
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    TEST_ASSERT(count == 1, "Should have 1 file after remount");
    
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(filenames[i], "persistent.txt") == 0) {
            found = 1;
            break;
        }
    }
    TEST_ASSERT(found == 1, "persistent.txt should exist after remount");
    
    cleanup_test_environment();
}

void test_edge_cases() {
    TEST_SECTION("Testing edge cases");
    
    // Operations on unmounted filesystem
    TEST_ASSERT(fs_create("test.txt") == -3, "Create on unmounted should return -3");
    TEST_ASSERT(fs_list(NULL, 0) == -1, "List on unmounted should return -1");
    TEST_ASSERT(fs_write("test.txt", "data", 4) == -3, "Write on unmounted should return -3");
    
    setup_test_environment();
    
    // Edge case: Create many files
    char filename[MAX_FILENAME];
    int created = 0;
    for (int i = 0; i < 50; i++) {  // Try to create 50 files
        snprintf(filename, sizeof(filename), "file_%d.txt", i);
        if (fs_create(filename) == 0) {
            created++;
        } else {
            break;
        }
    }
    TEST_ASSERT(created > 0, "Should be able to create at least some files");
    printf("📊 Created %d files successfully\n", created);
    
    cleanup_test_environment();
}

// =========== MAIN TEST RUNNER ===========

int main() {
    printf("🧪 OnlyFiles Simple Test Suite\n");
    printf("===============================\n");
    printf("Testing only public API functions to avoid compilation warnings\n");
    
    test_fs_format_and_mount();
    test_fs_create();
    test_fs_list();
    test_fs_write_basic();
    test_fs_write_errors();
    test_fs_write_sizes();
    test_fs_write_overwrite();
    test_full_integration();
    test_persistence();
    test_edge_cases();
    
    // Print final results
    printf("\n" "═══ TEST RESULTS ═══\n");
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