#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

// global vars
static int disk_file_descriptor = -1;
static superblock current_superblock = {0}; // hold the superblock data in cache in memory

int fs_format(const char* disk_path)
{
    //consts
    const int METADATA_BLOCKS_COUNT = 10;
    const int INODE_TABLE_START_BLOCK = 2;


    disk_file_descriptor = open(disk_path, O_RDWR | O_CREAT | O_TRUNC, 0644); // i take this part from the task instructions
    if (disk_file_descriptor < 0) {
        return -1; // failed while opening / creating file
    }

    //init the block allocation bitmap, inode table and empty block buffer
    unsigned char block_allocation_bitmap[BLOCK_SIZE] = {0};
    char empty_block_buffer[BLOCK_SIZE] = {0};
    inode uninit_inode_table = {0};

    //creating the Virtual Disk of 10MB by 2560 blocks of 4KB each => 2560*4096
    for(int i =0; i < MAX_BLOCKS; i++) {
        if (write(disk_file_descriptor, empty_block_buffer, BLOCK_SIZE) != BLOCK_SIZE) {
            close(disk_file_descriptor);
            return -1;
        }
    }

    //Initialize superblock
    superblock filesystem_superblock = {
        .total_blocks = MAX_BLOCKS,
        .block_size = BLOCK_SIZE,
        .free_blocks = MAX_BLOCKS - 10, //superblock and block bitmap took 2 blocks and 8 for inode table
        .total_inodes = MAX_FILES,
        .free_inodes = MAX_FILES
    };

    //write superblock to block 0
    lseek(disk_file_descriptor, 0, SEEK_SET);
    if (write(disk_file_descriptor, &filesystem_superblock, sizeof(superblock)) != sizeof(superblock)) {
        close(disk_file_descriptor);
        return -1; // failed while writing superblock
    }

    //mark the bits of the metadata blocks as used (from the task instructions)
    for(int block_index = 0; block_index < METADATA_BLOCKS_COUNT; block_index++) {
        block_allocation_bitmap[block_index / 8] |= (1 << (block_index % 8));
    }

    //write block allocation bitmap to block 1
    lseek(disk_file_descriptor, BLOCK_SIZE, SEEK_SET);
    if (write(disk_file_descriptor, block_allocation_bitmap, BLOCK_SIZE) != BLOCK_SIZE)
    {
        close(disk_file_descriptor);
        return -1; 
    }
    // set the inode table to be unused (block 2 to block 9)
    for (int inode_index = 0; inode_index < MAX_FILES; inode_index++) {

        off_t cur_inode_pos = (INODE_TABLE_START_BLOCK * BLOCK_SIZE) + (inode_index * sizeof(inode));
        lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);
        
        if (write(disk_file_descriptor, &uninit_inode_table, sizeof(inode)) != sizeof(inode)) {
            close(disk_file_descriptor);
            return -1;
        }

    }
    close(disk_file_descriptor);
    return 0; 
}

int fs_mount(const char* disk_path)
{
    printf("DEBUG: Entering fs_mount\n");

    if( disk_file_descriptor >= 0) {
        printf("DEBUG: Already mounted\n");
        return -1; // already mounted -> error
    }

    disk_file_descriptor = open(disk_path, O_RDWR);
    printf("DEBUG: open() returned fd=%d\n", disk_file_descriptor);
    if(disk_file_descriptor < 0) {
        return -1; // failed while open the disk file
    }

    lseek(disk_file_descriptor, 0, SEEK_SET); //move to the beginning of the file
    int bytes_read = read(disk_file_descriptor, &current_superblock, sizeof(superblock));
    printf("DEBUG: read() returned %d bytes (expected %lu)\n", bytes_read, sizeof(superblock));

    //now we check if we can read the superblock - read function return how many bytes were read
    if(bytes_read != sizeof(superblock)) {
        close(disk_file_descriptor);
        disk_file_descriptor = -1;
        return -1; 
    }

    printf("DEBUG: Superblock values:\n");
    printf("  total_blocks: %d (expected %d)\n", current_superblock.total_blocks, MAX_BLOCKS);
    printf("  block_size: %d (expected %d)\n", current_superblock.block_size, BLOCK_SIZE);
    printf("  total_inodes: %d (expected %d)\n", current_superblock.total_inodes, MAX_FILES);

    //verify the superblock values
    if(current_superblock.total_blocks != MAX_BLOCKS ||
        current_superblock.block_size != BLOCK_SIZE ||
        current_superblock.total_inodes != MAX_FILES)
        {
            printf("DEBUG: Invalid superblock values\n");
            close(disk_file_descriptor);
            disk_file_descriptor = -1;
            return -1; // invalid superblock values - not a valid filesystem
        }
    
    printf("DEBUG: Mount successful\n");
    // if we reachto this line, we successfully mounted the filesystem
    return 0;
}

void fs_unmount()
{
    if(disk_file_descriptor < 0) {
        return; // not mounted 
    }

    //write the superblock back to disk
    lseek(disk_file_descriptor, 0, SEEK_SET);
    write(disk_file_descriptor, &current_superblock, sizeof(superblock));

    close(disk_file_descriptor);
    disk_file_descriptor = -1; // reset the file descriptor
}