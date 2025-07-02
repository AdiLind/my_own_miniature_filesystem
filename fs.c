#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

// global vars
static int disk_file_descriptor = -1;
static superblock current_superblock = {0}; // hold the superblock data in cache in memory

// #### helper functions declaration #####
int find_inode_by_name(const char* i_name);
int find_free_inode();
void write_inode_to_disk(int inode_index, const inode* i_node);
int compare_strings(const char* str1, const char* str2);

int fs_format(const char* disk_path)
{
    //consts
    const int METADATA_BLOCKS_COUNT = 10;
    const int INODE_TABLE_START_BLOCK = 2;

    // we will use local file descriptor for formatting the disk
    int format_file_descrptor = open(disk_path, O_RDWR | O_CREAT | O_TRUNC, 0644); // i take this part from the task instructions
    if (format_file_descrptor < 0) {
        return -1; // failed while opening / creating file
    }

    //init the block allocation bitmap, inode table and empty block buffer
    unsigned char block_allocation_bitmap[BLOCK_SIZE] = {0};
    char empty_block_buffer[BLOCK_SIZE] = {0};
    inode uninit_inode_table = {0};

    //creating the Virtual Disk of 10MB by 2560 blocks of 4KB each => 2560*4096
    for(int i =0; i < MAX_BLOCKS; i++) {
        if (write(format_file_descrptor, empty_block_buffer, BLOCK_SIZE) != BLOCK_SIZE) {
            close(format_file_descrptor);
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
    lseek(format_file_descrptor, 0, SEEK_SET);
    if (write(format_file_descrptor, &filesystem_superblock, sizeof(superblock)) != sizeof(superblock)) {
        close(format_file_descrptor);
        return -1; // failed while writing superblock
    }

    //mark the bits of the metadata blocks as used (from the task instructions)
    for(int block_index = 0; block_index < METADATA_BLOCKS_COUNT; block_index++) {
        block_allocation_bitmap[block_index / 8] |= (1 << (block_index % 8));
    }

    //write block allocation bitmap to block 1
    lseek(format_file_descrptor, BLOCK_SIZE, SEEK_SET);
    if (write(format_file_descrptor, block_allocation_bitmap, BLOCK_SIZE) != BLOCK_SIZE)
    {
        close(format_file_descrptor);
        return -1; 
    }
    // set the inode table to be unused (block 2 to block 9)
    for (int inode_index = 0; inode_index < MAX_FILES; inode_index++) {

        off_t cur_inode_pos = (INODE_TABLE_START_BLOCK * BLOCK_SIZE) + (inode_index * sizeof(inode));
        lseek(format_file_descrptor, cur_inode_pos, SEEK_SET);
        
        if (write(format_file_descrptor, &uninit_inode_table, sizeof(inode)) != sizeof(inode)) {
            close(format_file_descrptor);
            return -1;
        }

    }
    close(format_file_descrptor);
    return 0; 
}

int fs_mount(const char* disk_path)
{

    if( disk_file_descriptor >= 0) {
        return -1; // already mounted -> error
    }

    disk_file_descriptor = open(disk_path, O_RDWR);
    if(disk_file_descriptor < 0) {
        return -1; // failed while open the disk file
    }

    lseek(disk_file_descriptor, 0, SEEK_SET); //move to the beginning of the file
    int bytes_read = read(disk_file_descriptor, &current_superblock, sizeof(superblock));
   // printf("DEBUG: read() returned %d bytes (expected %lu)\n", bytes_read, sizeof(superblock));

    //now we check if we can read the superblock - read function return how many bytes were read
    if(bytes_read != sizeof(superblock)) {
        close(disk_file_descriptor);
        disk_file_descriptor = -1;
        return -1; 
    }

    // printf("DEBUG: Superblock values:\n");
    // printf("  total_blocks: %d (expected %d)\n", current_superblock.total_blocks, MAX_BLOCKS);
    // printf("  block_size: %d (expected %d)\n", current_superblock.block_size, BLOCK_SIZE);
    // printf("  total_inodes: %d (expected %d)\n", current_superblock.total_inodes, MAX_FILES);

    //verify the superblock values
    if(current_superblock.total_blocks != MAX_BLOCKS ||
        current_superblock.block_size != BLOCK_SIZE ||
        current_superblock.total_inodes != MAX_FILES)
        {
            close(disk_file_descriptor);
            disk_file_descriptor = -1;
            return -1; // invalid superblock values - not a valid filesystem
        }
    
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

int fs_create(const char* filename)
{
    if(disk_file_descriptor < 0) {
        return -3; // not mounted - general error (-3)
    }

    //check validation of the filename
    if(filename == NULL || strlen(filename) == 0 || strlen(filename) > MAX_FILENAME) {
        return -3; 
    }

    if(find_inode_by_name(filename) >= 0) {
        return -1; // file already exists
    }

    int free_inode_index = find_free_inode();
    if(free_inode_index < 0) {
        return -2; // no free inodes - error code (-2)  
    }
    //TODO::Lavie - go over all the error codes and make sure they are consistent accourding to the HW file of all functions

    //create the inode for the new file
    inode new_inode = {0};
    strncpy(new_inode.name, filename, sizeof(new_inode.name) - 1);
    new_inode.used = 1; // mark as used

    write_inode_to_disk(free_inode_index, &new_inode);

    // update superblock and free inodes count
    current_superblock.free_inodes--;
    // write(disk_file_descriptor, &current_superblock, sizeof(superblock)); - we already write the superblock in fs_unmount so we dont should do it again

    return 0; // hopa - success a new file was created :)
}





// #### helper functions #####
int find_inode_by_name(const char* i_name)
{
    if(disk_file_descriptor < 0) {
        return -1; 
    }

    inode current_inode;
    for(int i = 0; i < MAX_FILES; i++) {
        off_t cur_inode_pos = (2 * BLOCK_SIZE) + (i * sizeof(inode));
        lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);

        int count_of_reading_buf = read(disk_file_descriptor, &current_inode, sizeof(inode));
        if(count_of_reading_buf != sizeof(inode)) {
            return -1; // error while reading the inode
        }

        //looking for the inode with the given name
        if(current_inode.used && compare_strings(current_inode.name, i_name) == 0) {
            return i; // return the indx the inode
        }
    }
    return -1; //there is no inode with this i_name
}

int compare_strings(const char* str1, const char* str2)
{
    if(str1 == NULL || str2 == NULL) {
        return 0; // TODO::Adi - null == null -> True (?) - **should check this**
    }

    while (*str1 && *str1 == *str2)
    {
        str1++, str2++;
    }

    return *str1 - *str2;
}

int find_free_inode()
{
    if(disk_file_descriptor < 0) {
        return -1;
    }

    inode current_inode;
    for(int i = 0; i < MAX_FILES; i++) {
        off_t cur_inode_pos = (2 * BLOCK_SIZE) + (i * sizeof(inode));
        lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);

        int count_of_reading_buf = read(disk_file_descriptor, &current_inode, sizeof(inode));
        if(count_of_reading_buf != sizeof(inode)) {
            return -1;
        }

        if(!current_inode.used) {
            return i; //return the first free inode
        }
    }
    return -1; // no free inodes available
}

void write_inode_to_disk(int i_inode_num, const inode* i_source) 
{
    if(disk_file_descriptor < 0 || i_inode_num < 0 || i_inode_num >= MAX_FILES) {
        return; // invalid state
    }

    off_t cur_inode_pos = (2 * BLOCK_SIZE) + (i_inode_num * sizeof(inode));
    lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);
    write(disk_file_descriptor, i_source, sizeof(inode));
}