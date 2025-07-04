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
void read_inode_from_disk(int i_inode_index, inode* i_inode_buffer);
int find_free_block();
void mark_block_as_used(int block_index);
void mark_block_as_free(int block_index);
int validate_block_number_and_filesystem(int i_block_num);
int read_bitmap_from_disk(unsigned char* i_bitmap_buffer);
int write_bitmap_to_disk(const unsigned char* i_bitmap_buffer);
//fs_writer helper functions declaration
int validate_write_operation_parameters(const char* filename, const void* data, int size);
int check_available_space_for_write_operation(int blocks_needed, int current_file_size);
int free_file_existing_blocks(inode* file_inode);
int allocate_blocks_for_file(inode* file_inode, int blocks_needed);
int write_data_to_allocated_blocks(inode* file_inode, const void* data, int size, int blocks_needed);


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

int fs_list(char filenames[][MAX_FILENAME], int max_files)
{
    if(disk_file_descriptor < 0) {
        return -1;
    }

    inode current_inode;
    int num_of_files_found = 0;

    for(int i = 0; i < MAX_FILES && num_of_files_found < max_files; i++) {
        off_t cur_inode_pos = (2 * BLOCK_SIZE) + (i * sizeof(inode));
        lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);

        int count_of_reading_buf = read(disk_file_descriptor, &current_inode, sizeof(inode));
        
        if(count_of_reading_buf != sizeof(inode)) {
            return -1; 
        }

        if(current_inode.used) {
            strncpy(filenames[num_of_files_found], current_inode.name, MAX_FILENAME);
            filenames[num_of_files_found][MAX_FILENAME - 1] = '\0'; // ensure null termination
            num_of_files_found++;
        }
    }
    return num_of_files_found; 
}

int fs_write(const char* filename, const void* data, int size)
{
   int isValidInput = validate_write_operation_parameters(filename, data, size);
    if(isValidInput != 0) {
        return isValidInput; // return the error code
    }

    int inode_index = find_inode_by_name(filename);
    if(inode_index < 0) {
        return -1; // file not found
    }

    inode current_file_inode;
    read_inode_from_disk(inode_index, &current_file_inode);

    int blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE; 
    int free_space_check = check_available_space_for_write_operation(blocks_needed, current_file_inode.size);
    if(free_space_check < 0) {
        return free_space_check; // return the error code
    }

    // free existing blocks if the file is being resized
    int free_blocks_result = free_file_existing_blocks(&current_file_inode);
    if(free_blocks_result < 0) {
        return free_blocks_result; // return the error code
    }
    // allocate new blocks for the file
    int allocate_blocks_result = allocate_blocks_for_file(&current_file_inode, blocks_needed);
    if(allocate_blocks_result < 0) {
        return allocate_blocks_result; 
    }

    int write_data_result = write_data_to_allocated_blocks(&current_file_inode, data, size, blocks_needed);
    if(write_data_result < 0) {
        return write_data_result; 
    }

    current_file_inode.size = size;
    write_inode_to_disk(inode_index, &current_file_inode);

    return 0;
    
}

int fs_read(const char* filename, void* buffer, int size)
{
    if(disk_file_descriptor < 0) {
        return -1; // maybe should be -3? - not mounted but if it is not mounted we cannot read the file so it also -1 error
    }

    if(filename == NULL || buffer == NULL || size <= 0 || strlen(filename) == 0 || strlen(filename) > MAX_FILENAME) {
        return -3; // invalid parameters
    }

    int inode_index = find_inode_by_name(filename);
    if(inode_index < 0) {
        return -1; // file not found
    }

    inode current_file_inode;
    read_inode_from_disk(inode_index, &current_file_inode);
    // If requested size is larger than file size, only read available bytes
    int bytes_to_read = (size < current_file_inode.size) ? size : current_file_inode.size;
    if(bytes_to_read == 0) {
        return 0; // nothing to read
    }

    int blocks_to_read = (bytes_to_read + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // Read data from the allocated blocks
    char* read_buffer = (char*)buffer;
    int total_bytes_read = 0;

    for(int i = 0; i < blocks_to_read; i++) {
        int block_number = current_file_inode.blocks[i];

        if(block_number < 10 || block_number >= MAX_BLOCKS) {
            return -3; // invalid block index
        }

        int remaining_bytes = bytes_to_read - total_bytes_read;
        int bytes_from_this_block = (remaining_bytes > BLOCK_SIZE) ? BLOCK_SIZE : remaining_bytes;
        //temp buffer for reading all block data
        char temp_block_buffer[BLOCK_SIZE] = {0};
        off_t block_offset = block_number * BLOCK_SIZE;
        if(lseek(disk_file_descriptor, block_offset, SEEK_SET) < 0) {
            return -3;
        }

        //read full block data
        int bytes_read = read(disk_file_descriptor, temp_block_buffer, BLOCK_SIZE);
        if(bytes_read != BLOCK_SIZE) {
            return -3;
        }

        //copy only the needed bytes to the read buffer
        memcpy(read_buffer + total_bytes_read, temp_block_buffer, bytes_from_this_block);
        total_bytes_read += bytes_from_this_block;
    }
    return total_bytes_read; 
}

int fs_delete(const char* filename)
{
    if(disk_file_descriptor < 0) {
        return -2; // not mounted maybe should be -1 ? 
    }

    if(filename == NULL || strlen(filename) == 0 || strlen(filename) > MAX_FILENAME) {
        return -2; 
    }

    int inode_index = find_inode_by_name(filename);
    if(inode_index < 0) {
        return -1; // file not found
    }

    inode file_inode_to_delete;
    read_inode_from_disk(inode_index, &file_inode_to_delete);
    // free the blocks allocated for the file
    int free_blocks_result = free_file_existing_blocks(&file_inode_to_delete);
    if(free_blocks_result < 0) {
        return -2; 
    }

    memset(&file_inode_to_delete, 0, sizeof(inode)); // clear the inode data
    file_inode_to_delete.used = 0; // mark inode as free
    write_inode_to_disk(inode_index, &file_inode_to_delete);

    current_superblock.free_inodes++;
    return 0; 
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


int find_free_block()
{
    if (validate_block_number_and_filesystem(0) != 0) {
        return -1;
    }

    unsigned char block_allocation_bitmap[BLOCK_SIZE];
    if (read_bitmap_from_disk(block_allocation_bitmap) != 0) {
        return -1; // error reading bitmap
    }

    //LOOKING FOR THE FIRST FREE BLOCK
    // we start looking from block 10 because blocks 0-9 are used for superblock, bitmap and inode table
    for(int block_index = 10; block_index < MAX_BLOCKS; block_index++) 
    {
        if(!(block_allocation_bitmap[block_index / 8] & (1 << (block_index % 8)))) 
        {
            return block_index; 
        }
    }
    return -1; //didnt found a free block available
}

void write_inode_to_disk(int inode_index, const inode* i_node)
{
    if(disk_file_descriptor < 0 || inode_index < 0 || inode_index >= MAX_FILES || i_node == NULL) 
    {
        return;
    }

    off_t cur_inode_pos = (2 * BLOCK_SIZE) + (inode_index * sizeof(inode));
    lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);
    write(disk_file_descriptor, i_node, sizeof(inode));
}

void mark_block_as_used(int block_index)
{
    if (validate_block_number_and_filesystem(block_index) != 0) {
        return; // invalid parameters
    }

    unsigned char block_allocation_bitmap[BLOCK_SIZE];
    if (read_bitmap_from_disk(block_allocation_bitmap) != 0) {
        return; // error reading bitmap
    }


    //mark the block as used (bit =1) (from the task instructions - bitwise manipulation)
    block_allocation_bitmap[block_index / 8] |= (1 << (block_index % 8));
    write_bitmap_to_disk(block_allocation_bitmap);
}

void mark_block_as_free(int block_index)
{
    if (validate_block_number_and_filesystem(block_index) != 0) {
        return; 
    }
    unsigned char block_allocation_bitmap[BLOCK_SIZE];
    if (read_bitmap_from_disk(block_allocation_bitmap) != 0) {
        return; // error reading bitmap
    }

    //mark the block as free (bit =0) from the task instructions - bitwise manipulation)
    block_allocation_bitmap[block_index / 8] &= ~(1 << (block_index % 8));
    write_bitmap_to_disk(block_allocation_bitmap);
}

int validate_block_number_and_filesystem(int i_block_num)
{
    if(disk_file_descriptor < 0 || i_block_num < 0 || i_block_num >= MAX_BLOCKS) 
    {
        return -1;
    }

    return 0; // valid block number and filesystem
}

int read_bitmap_from_disk(unsigned char* i_bitmap_buffer)
{
    if(disk_file_descriptor < 0 || i_bitmap_buffer == NULL) 
    {
        return -1;
    }

    lseek(disk_file_descriptor, BLOCK_SIZE, SEEK_SET);
    int num_of_reading_byte = read(disk_file_descriptor, i_bitmap_buffer, BLOCK_SIZE);
    if(num_of_reading_byte != BLOCK_SIZE) 
    {
        return -1;
    }

    return 0; 
}

int write_bitmap_to_disk(const unsigned char* i_bitmap_buffer)
{
    if(disk_file_descriptor < 0 || i_bitmap_buffer == NULL) 
    {
        return -1;
    }

    lseek(disk_file_descriptor, BLOCK_SIZE, SEEK_SET);
    int num_of_writing_byte = write(disk_file_descriptor, i_bitmap_buffer, BLOCK_SIZE);
    if(num_of_writing_byte != BLOCK_SIZE) 
    {
        return -1;
    }

    return 0; 
}


void read_inode_from_disk(int i_inode_index, inode* i_inode_buffer)
{
    if(disk_file_descriptor < 0 || i_inode_index < 0 || i_inode_index >= MAX_FILES || i_inode_buffer == NULL) 
    {
        return; // invalid parameters
    }

    off_t cur_inode_pos = (2 * BLOCK_SIZE) + (i_inode_index * sizeof(inode));
    lseek(disk_file_descriptor, cur_inode_pos, SEEK_SET);
    read(disk_file_descriptor, i_inode_buffer, sizeof(inode));
}

int validate_write_operation_parameters(const char* filename, const void* data, int size) 
{
    if(disk_file_descriptor < 0) {
        return -3; // not mounted
    }

    if(filename == NULL || data == NULL || size <= 0 || strlen(filename) == 0 || strlen(filename) > MAX_FILENAME) {
        return -3; // invalid parameters
    }

    //maximum file size limit is using all the 12 blocks * 4KB = 48KB)
    if (size > MAX_DIRECT_BLOCKS * BLOCK_SIZE) {
        return -2; // file too large for the filesystem
    }
    
    return 0; // validation passed
}

int check_available_space_for_write_operation(int blocks_needed, int current_file_size) 
{
    int current_file_blocks = (current_file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed > current_superblock.free_blocks + current_file_blocks) {
        return -2; // not enough space available
    }
    
    return 0; // enough space
}

int free_file_existing_blocks(inode* file_inode) 
{
    if (file_inode == NULL) {
        return -3; // invalid parameter
    }
    
    int old_blocks_count = (file_inode->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int i = 0; i < old_blocks_count; i++) {
        if (file_inode->blocks[i] != 0 && file_inode->blocks[i] < MAX_BLOCKS) {
            mark_block_as_free(file_inode->blocks[i]);
            current_superblock.free_blocks++;
            file_inode->blocks[i] = 0; // clear the pointer
        }
        
    }
    
    return 0;
}

int allocate_blocks_for_file(inode* file_inode, int blocks_needed) 
{
    if (file_inode == NULL || blocks_needed <= 0) {
        return -3; 
    }
    
    //allocate the new blocks we need
    for (int block_iterator = 0; block_iterator < blocks_needed; block_iterator++) {
        int new_block_number = find_free_block();
        if (new_block_number < 0) { //we didnt find a free block while we allready allocated some blocks
            //we need to free the blocks we already allocated
            for (int rollback_iterator = 0; rollback_iterator < block_iterator; rollback_iterator++) {
                mark_block_as_free(file_inode->blocks[rollback_iterator]);
                current_superblock.free_blocks++;
                file_inode->blocks[rollback_iterator] = 0;
            }
            return -2; // allocation failed
        }
        
        mark_block_as_used(new_block_number);
        file_inode->blocks[block_iterator] = new_block_number;
        current_superblock.free_blocks--;
    }
    
    return 0; // success
    
}

int write_data_to_allocated_blocks(inode* file_inode, const void* data, int size, int blocks_needed) 
{
    if (file_inode == NULL || data == NULL || size <= 0 || blocks_needed <= 0) {
        return -3;
    }
    
    const char* data_bytes = (const char*)data;
    int bytes_written = 0;
    
    for (int block_iterator = 0; block_iterator < blocks_needed; block_iterator++) {
        int block_number = file_inode->blocks[block_iterator];
        
        // Calculate bytes to write in this block
        int bytes_to_write_in_this_block = (size - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (size - bytes_written);
        
        char block_buffer[BLOCK_SIZE];
        memset(block_buffer, 0, BLOCK_SIZE);  //adding 0 for partial blocks
        //copy data to buffer
        memcpy(block_buffer, data_bytes + bytes_written, bytes_to_write_in_this_block);
        
        //write block to disk
        off_t block_position = block_number * BLOCK_SIZE;
        lseek(disk_file_descriptor, block_position, SEEK_SET);
        if (write(disk_file_descriptor, block_buffer, BLOCK_SIZE) != BLOCK_SIZE) {
            return -3; //other error
        }
        
        bytes_written += bytes_to_write_in_this_block;
    }
    
    return 0;
}
    