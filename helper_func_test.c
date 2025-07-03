// Test file for helper functions (if you want to test them separately)
#include <stdio.h>
#include <assert.h>
#include "fs.h"

void test_find_inode() {
    printf("Testing find_inode helper...\n");
    
    fs_format("test_helpers.img");
    fs_mount("test_helpers.img");
    
    // Should not find non-existent file
    assert(find_inode_by_name("nothere.txt") == -1);
    
    // Create a file and find it
    fs_create("findme.txt");
    int inode_num = find_inode_by_name("findme.txt");
    assert(inode_num >= 0 && inode_num < MAX_FILES);
    
    // Case sensitive test
    assert(find_inode_by_name("FINDME.txt") == -1);
    assert(find_inode_by_name("findme.TXT") == -1);
    
    fs_unmount();
    unlink("test_helpers.img");
    printf("find_inode tests PASSED\n");
}

void test_find_free_inode() {
    printf("Testing find_free_inode helper...\n");
    
    fs_format("test_helpers.img");
    fs_mount("test_helpers.img");
    
    // Should find many free inodes initially
    int free1 = find_free_inode();
    assert(free1 >= 0);
    
    // Create files and check free inodes decrease
    for (int i = 0; i < 10; i++) {
        char name[20];
        sprintf(name, "test%d", i);
        fs_create(name);
    }
    
    int free2 = find_free_inode();
    assert(free2 > free1); // Should get a higher inode number
    
    fs_unmount();
    unlink("test_helpers.img");
    printf("find_free_inode tests PASSED\n");
}

int main() {
    test_find_inode();
    test_find_free_inode();
    printf("\nAll helper function tests passed!\n");
    return 0;
}