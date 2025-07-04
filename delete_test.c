#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

void test_basic_delete() {
    printf("Test 1: Basic Delete Operation\n");
    
    // Format and mount - ensures clean start
    assert(fs_format("test_disk.img") == 0);
    assert(fs_mount("test_disk.img") == 0);
    
    // Create and write to file
    assert(fs_create("test.txt") == 0);
    char data[] = "Hello, World!";
    assert(fs_write("test.txt", data, sizeof(data)) == 0);
    
    // Verify file exists
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    assert(count == 1);
    assert(strcmp(filenames[0], "test.txt") == 0);
    
    // Delete the file
    int result = fs_delete("test.txt");
    printf("  Delete result: %d\n", result);
    assert(result == 0);
    
    // Verify file is gone
    count = fs_list(filenames, 10);
    printf("  Files after delete: %d\n", count);
    assert(count == 0);
    
    // Try to read deleted file
    char buffer[100];
    result = fs_read("test.txt", buffer, sizeof(buffer));
    assert(result == -1); // file not found
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_delete_nonexistent() {
    printf("Test 2: Delete Non-existent File\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    int result = fs_delete("nonexistent.txt");
    printf("  Result: %d (expected: -1)\n", result);
    assert(result == -1);
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_delete_empty_file() {
    printf("Test 3: Delete Empty File\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    // Create empty file (no write)
    assert(fs_create("empty.txt") == 0);
    
    // Delete it
    int result = fs_delete("empty.txt");
    assert(result == 0);
    
    // Verify it's gone
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    assert(count == 0);
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_delete_large_file() {
    printf("Test 4: Delete Large File (Multiple Blocks)\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    // Create large file (3 blocks)
    assert(fs_create("large.bin") == 0);
    const int size = BLOCK_SIZE * 3 + 100;
    char* data = (char*)malloc(size);
    for(int i = 0; i < size; i++) {
        data[i] = (char)(i % 256);
    }
    assert(fs_write("large.bin", data, size) == 0);
    
    // Delete it
    int result = fs_delete("large.bin");
    printf("  Deleted file of %d bytes (%d blocks)\n", size, (size + BLOCK_SIZE - 1) / BLOCK_SIZE);
    assert(result == 0);
    
    // Verify it's gone
    char buffer[100];
    result = fs_read("large.bin", buffer, sizeof(buffer));
    assert(result == -1);
    
    free(data);
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_delete_and_reuse_space() {
    printf("Test 5: Delete and Reuse Space\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    // Create and write first file
    assert(fs_create("first.txt") == 0);
    char data1[] = "First file data that uses some blocks";
    assert(fs_write("first.txt", data1, sizeof(data1)) == 0);
    
    // Create second file
    assert(fs_create("second.txt") == 0);
    char data2[] = "Second file";
    assert(fs_write("second.txt", data2, sizeof(data2)) == 0);
    
    // Delete first file
    assert(fs_delete("first.txt") == 0);
    
    // Create new file - should reuse freed space
    assert(fs_create("reused.txt") == 0);
    char data3[] = "This should reuse the freed blocks from first.txt";
    assert(fs_write("reused.txt", data3, sizeof(data3)) == 0);
    
    // Verify we can read the new file
    char buffer[100];
    int bytes_read = fs_read("reused.txt", buffer, sizeof(buffer));
    assert(bytes_read == sizeof(data3));
    assert(memcmp(buffer, data3, sizeof(data3)) == 0);
    
    printf("  Successfully reused space from deleted file\n");
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_delete_multiple_files() {
    printf("Test 6: Delete Multiple Files\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    // Create multiple files
    const int num_files = 5;
    char filename[MAX_FILENAME];
    char data[] = "Test data";
    
    for(int i = 0; i < num_files; i++) {
        sprintf(filename, "file%d.txt", i);
        assert(fs_create(filename) == 0);
        assert(fs_write(filename, data, sizeof(data)) == 0);
    }
    
    // Verify all files exist
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    assert(count == num_files);
    
    // Delete all files
    for(int i = 0; i < num_files; i++) {
        sprintf(filename, "file%d.txt", i);
        assert(fs_delete(filename) == 0);
        printf("  Deleted %s\n", filename);
    }
    
    // Verify all files are gone
    count = fs_list(filenames, 10);
    assert(count == 0);
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_invalid_parameters() {
    printf("Test 7: Invalid Parameters\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    // NULL filename
    printf("  Testing NULL filename: ");
    assert(fs_delete(NULL) == -2);
    printf("✓\n");
    
    // Empty filename
    printf("  Testing empty filename: ");
    assert(fs_delete("") == -2);
    printf("✓\n");
    
    // Too long filename
    printf("  Testing too long filename: ");
    char long_name[100];
    memset(long_name, 'a', sizeof(long_name));
    long_name[99] = '\0';
    assert(fs_delete(long_name) == -2);
    printf("✓\n");
    
    fs_unmount();
    
    // Not mounted
    printf("  Testing when not mounted: ");
    assert(fs_delete("test.txt") == -2);
    printf("✓\n");
    
    printf("  ✓ All Passed\n\n");
}

void test_delete_persistence() {
    printf("Test 8: Delete Persistence (After Unmount/Mount)\n");
    
    // Format first for clean start
    assert(fs_format("test_disk.img") == 0);
    
    // Create files
    assert(fs_mount("test_disk.img") == 0);
    assert(fs_create("keep.txt") == 0);
    assert(fs_create("delete.txt") == 0);
    char data[] = "Data";
    assert(fs_write("keep.txt", data, sizeof(data)) == 0);
    assert(fs_write("delete.txt", data, sizeof(data)) == 0);
    
    // Delete one file
    assert(fs_delete("delete.txt") == 0);
    fs_unmount();
    
    // Mount again and verify
    assert(fs_mount("test_disk.img") == 0);
    
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    assert(count == 1);
    assert(strcmp(filenames[0], "keep.txt") == 0);
    
    // Verify deleted file is still gone
    char buffer[100];
    assert(fs_read("delete.txt", buffer, sizeof(buffer)) == -1);
    
    // Verify kept file still exists
    assert(fs_read("keep.txt", buffer, sizeof(buffer)) == sizeof(data));
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

void test_stress_create_delete() {
    printf("Test 9: Stress Test - Create/Delete Cycle\n");
    
    assert(fs_format("test_disk.img") == 0);  // Clean start
    assert(fs_mount("test_disk.img") == 0);
    
    const int cycles = 10;
    char data[BLOCK_SIZE * 2]; // 2 blocks of data
    memset(data, 'X', sizeof(data));
    
    for(int i = 0; i < cycles; i++) {
        // Create file
        assert(fs_create("stress.txt") == 0);
        
        // Write data
        assert(fs_write("stress.txt", data, sizeof(data)) == 0);
        
        // Delete file
        assert(fs_delete("stress.txt") == 0);
        
        printf("  Cycle %d completed\n", i + 1);
    }
    
    // Verify no files remain
    char filenames[10][MAX_FILENAME];
    int count = fs_list(filenames, 10);
    assert(count == 0);
    
    fs_unmount();
    printf("  ✓ Passed\n\n");
}

int main() {
    printf("=== Testing fs_delete Implementation ===\n\n");
    
    test_basic_delete();
    test_delete_nonexistent();
    test_delete_empty_file();
    test_delete_large_file();
    test_delete_and_reuse_space();
    test_delete_multiple_files();
    test_invalid_parameters();
    test_delete_persistence();
    test_stress_create_delete();
    
    printf("=== All Tests Passed! ===\n");
    return 0;
}