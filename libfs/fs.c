#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

// keeps track of whether fs is mounted or not
static int fs_mounted = 0;

struct superblock{
	char signature[8];
	u_int16_t total_blk_count;
	u_int16_t rdir_blk;
	u_int16_t data_blk;
	u_int16_t data_blk_count;
	u_int8_t fat_blk_count;
	u_int8_t padding[4079];
};

struct file_descriptor{
	int used;
	size_t offset;
	int rootIndex;
};

static struct superblock infoSuperblock;

static struct file_descriptor fds[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
	if (block_disk_open(diskname) == -1){
		return -1;
	}
	char buf[4096];

	if (block_read(0, buf) == -1){
		block_disk_close();
		return -1;
	}
	memcpy(&infoSuperblock, buf, sizeof(struct superblock));

	if (strncmp(infoSuperblock.signature, "ECS150FS", 8) != 0){
		block_disk_close();
		return -1;
	}

	if (infoSuperblock.total_blk_count != block_disk_count()){
		block_disk_close();
		return -1;
	}

	for (int i = 0; i < FS_OPEN_MAX_COUNT; i ++){
		fds[i].used = 0;
		fds[i].offset = 0;
		fds[i].rootIndex = 0;
	}
	fs_mounted = 1;
	return 0;
}

int fs_umount(void)
{	
	int stat = block_disk_close();
	if (stat == 0){
		fs_mounted = 0;
	}
	return stat;
}

int fs_info(void)
{
	if (block_disk_count() == -1){
		return -1;
	}


	int freeFat = 0;
	int totalEntries = infoSuperblock.data_blk_count;
	for (int i = 0; i < infoSuperblock.fat_blk_count; i++){
		char buf[4096];
		if (block_read(i + 1, buf) == -1){
			block_disk_close();
			return -1;
		}

		u_int16_t *entriesFat = (u_int16_t *)buf;
		for (int j = 0; j < 2048; j++){
			if (j >= totalEntries){
				break;
			}
			if (entriesFat[j] == 0){
				freeFat += 1;
			}
		}
		totalEntries -= 2048;
	}

	int freeRdir = 0;
	char buf[4096];
	if (block_read(infoSuperblock.rdir_blk, buf) == -1){
		block_disk_close();
		return -1;
	}
	for (int i = 0; i < 128; i++){
		if (buf[i*32] == '\0'){
			freeRdir += 1;
		}
	}


	printf("FS Info:\n");
	printf("total_blk_count=%d\n", infoSuperblock.total_blk_count);
	printf("fat_blk_count=%d\n", infoSuperblock.fat_blk_count);
	printf("rdir_blk=%d\n", infoSuperblock.rdir_blk);
	printf("data_blk=%d\n", infoSuperblock.data_blk);
	printf("data_blk_count=%d\n", infoSuperblock.data_blk_count);
	printf("fat_free_ratio=%d/%d\n", freeFat, infoSuperblock.data_blk_count);     
	printf("rdir_free_ratio=%d/128\n", freeRdir);
	return 0;
}

int fs_create(const char *filename)
{
	if (block_disk_count() == -1 || filename == NULL || strlen(filename) >= FS_FILENAME_LEN){
		return -1;
	}

	char buf[4096];
	if (block_read(infoSuperblock.rdir_blk, buf) == -1){
		block_disk_close();
		return -1;
	}

	// Check to see that file does not already exist.
	for (int i = 0; i < 128; i++){
		if (buf[i*32] != '\0'){
			int index = i*32;
			int match = 1;
			
			for (size_t j = 0; j < strlen(filename); j++){
				if (buf[index + j] != filename[j]){
					match = 0;
					break;
				}
			}

			if (match && buf[index + strlen(filename)] == '\0'){
				return -1; 
			}
		}
	}

	for (int i = 0; i < 128; i++){
		if (buf[i*32] == '\0'){
			int index = i*32;
			for (size_t j = 0; j < strlen(filename); j++){
				buf[index + j] = filename[j];
			}
			buf[index + strlen(filename)] = '\0';

			buf[index + 16] = 0;
			buf[index + 17] = 0;
			buf[index + 18] = 0;
			buf[index + 19] = 0;

			buf[index + 20] = 0xFF;
			buf[index + 21] = 0xFF;

			return block_write(infoSuperblock.rdir_blk, buf);
		}
	}

	return -1; // Root directory already contains max number of files.
}

int fs_delete(const char *filename)
{
	// Need to come back for the condition when file is open.

	if (block_disk_count() == -1 || filename == NULL || strlen(filename) >= FS_FILENAME_LEN){
		return -1;
	}

	// Need to free file descriptors as well

	char buf[4096];
	if (block_read(infoSuperblock.rdir_blk, buf) == -1){
		block_disk_close();
		return -1;
	}

	for (int i = 0; i < 128; i++){
		if (buf[i*32] != '\0'){
			int index = i*32;
			int match = 1;
			
			for (size_t j = 0; j < strlen(filename); j++){
				if (buf[index + j] != filename[j]){
					match = 0;
					break;
				}
			}

			if (match && buf[index + strlen(filename)] == '\0'){
				buf[index] = '\0';

				for (int j = 16; j < 32; j++){
					buf[index + j] = 0;
				}

				return block_write(infoSuperblock.rdir_blk, buf);
			}
		}
	}

	return -1; // No file with that name
}

int fs_ls(void)
{
	// Just need to figure out what the output is supposed to look like.
}

int fs_open(const char *filename)
{
	if (block_disk_count() == -1 || filename == NULL || strlen(filename) >= FS_FILENAME_LEN){
		return -1;
	}

	char buf[4096];
	if (block_read(infoSuperblock.rdir_blk, buf) == -1){
		return -1;
	}

	for (int i = 0; i < 128; i++){
		if (buf[i*32] != '\0'){
			int index = i*32;
			int match = 1;
			
			for (size_t j = 0; j < strlen(filename); j++){
				if (buf[index + j] != filename[j]){
					match = 0;
					break;
				}
			}

			if (match && buf[index + strlen(filename)] == '\0'){
				for (int j = 0; j < FS_OPEN_MAX_COUNT; j++){
					if (fds[j].used == 0){
						fds[j].used = 1;
						fds[j].offset = 0;
						fds[j].rootIndex = index;
						return j;
					}
				}
				return -1; // All file descriptors are used;
			}
		}
	}

	return -1; // File name not found.
}

int fs_close(int fd)
{
	if (block_disk_count() == -1 || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].used == 0){
		return -1;
	}

	fds[fd].used = 0;
	fds[fd].offset = 0;
	fds[fd].rootIndex = 0;

	return 0;

}

int fs_stat(int fd)
{
	if (block_disk_count() == -1 || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].used == 0){
		return -1;
	}

	char buf[4096];
	if (block_read(infoSuperblock.rdir_blk, buf) == -1){
		return -1;
	}

	u_int32_t size;
	memcpy(&size, &buf[fds[fd].rootIndex + 16], sizeof(u_int32_t));
	return size;
}

int fs_lseek(int fd, size_t offset)
{
	if (block_disk_count() == -1 || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fds[fd].used == 0 || (int)offset > fs_stat(fd)){
		return -1;
	}

	fds->offset = offset;
	return 0;
}

// Helper function, it gets the next block's index as the name suggests
uint16_t get_next_block(uint16_t index) {
    uint8_t fat_block[BLOCK_SIZE];

    int fat_block_num = index / 2048;
    int fat_offset = (index % 2048) * 2;

    if (block_read(1 + fat_block_num, fat_block) == -1) {
        return 0xFFFF; // Error, indicating EOF
	}

    uint16_t value;
    memcpy(&value, &fat_block[fat_offset], sizeof(uint16_t));
    return value;
}

// Helper function, it sets the FAT index with value(next FAT entry)
void set_fat_entry(uint16_t index, uint16_t value) {
    uint8_t fat_block[BLOCK_SIZE];

    int fat_block_num = index / 2048;
    int fat_offset = (index % 2048) * 2;

    // Read the correct FAT block
    if (block_read(1 + fat_block_num, fat_block) == -1) {
        return;
	}

    // Update the FAT entry
    memcpy(&fat_block[fat_offset], &value, sizeof(uint16_t));

    // Write it back to disk
    block_write(1 + fat_block_num, fat_block);
}

// Helper function, it allocates space for a block
uint16_t allocate_block() {
    uint8_t fat_block[BLOCK_SIZE];

    for (int i = 0; i < infoSuperblock.fat_blk_count; i++) {
        if (block_read(1 + i, fat_block) == -1) {
            return 0xFFFF; // Failed to read
		}

        for (int j = 0; j < 2048; j++) {
            uint16_t val;
            memcpy(&val, &fat_block[j * 2], sizeof(uint16_t));

            if (val == 0) {
                uint16_t new_val = 0xFFFF;
                memcpy(&fat_block[j * 2], &new_val, sizeof(uint16_t));
                block_write(1 + i, fat_block);
                return i * 2048 + j;
            }
        }
    }
    return 0xFFFF; // No free block
}

int fs_write(int fd, void *buf, size_t count)
{
    if (!fs_mounted || buf == NULL || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !fds[fd].used) {
        return -1;
	}

    struct file_descriptor *fd_entry = &fds[fd];

    // Load root directory block
    uint8_t rdir_block[BLOCK_SIZE];
    if (block_read(infoSuperblock.rdir_blk, rdir_block) == -1) {
        return -1;
	}

    // Read file size
    uint32_t size;
    memcpy(&size, &rdir_block[fd_entry->rootIndex + 16], sizeof(uint32_t));

    // Get starting data block
    uint16_t block;
    memcpy(&block, &rdir_block[fd_entry->rootIndex + 20], sizeof(uint16_t));

    // If file is empty, allocate first block
    if (block == 0xFFFF) {
        block = allocate_block();

		// Failed to allocate
        if (block == 0xFFFF) {
            return 0;
		}
        memcpy(&rdir_block[fd_entry->rootIndex + 20], &block, sizeof(uint16_t));
    }

    // Traverse to the first writing block
    size_t skip = fd_entry->offset / BLOCK_SIZE;
    uint16_t prev = 0xFFFF;

    for (size_t i = 0; i <= skip; i++) {

		// When writing past EOF, extend the file
        if (block == 0xFFFF) {
            uint16_t new_blk = allocate_block();

			// If failed to allocate the next block, write nothing and return
            if (new_blk == 0xFFFF) {
                return 0;
			}

			// Update the new_block as EOF
            set_fat_entry(prev, new_blk);
            set_fat_entry(new_blk, 0xFFFF);
            block = new_blk;
        }

		// Skip until the first writing block
		if (i < skip) {
			prev = block;
			block = get_next_block(block);
		}
    }

    size_t offset = fd_entry->offset % BLOCK_SIZE;
    size_t bytes_written = 0;
    uint8_t temp_block[BLOCK_SIZE];

    while (bytes_written < count && block != 0xFFFF) {
        if (offset > 0 || count - bytes_written < BLOCK_SIZE) {
            block_read(infoSuperblock.data_blk + block, temp_block);
		}
        else {
            memset(temp_block, 0, BLOCK_SIZE);
		}

        size_t to_write = BLOCK_SIZE - offset;
        if (to_write > count - bytes_written)
            to_write = count - bytes_written;

        memcpy(temp_block + offset, (uint8_t *)buf + bytes_written, to_write);
        block_write(infoSuperblock.data_blk + block, temp_block);

        bytes_written += to_write;
        offset = 0;

        if (bytes_written < count) {
            uint16_t next = get_next_block(block);
            if (next == 0xFFFF) {
                next = allocate_block();
                if (next == 0xFFFF)
                    break;

                set_fat_entry(block, next);
                set_fat_entry(next, 0xFFFF);
            }
            block = next;
        }
    }

    fd_entry->offset += bytes_written;

    if (fd_entry->offset > size) {
        memcpy(&rdir_block[fd_entry->rootIndex + 16], &fd_entry->offset, sizeof(uint32_t));
    }

    block_write(infoSuperblock.rdir_blk, rdir_block);
    return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
    if (!fs_mounted || buf == NULL || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !fds[fd].used) {
        return -1;
	}

    struct file_descriptor *fd_entry = &fds[fd];

    // Load root directory block
    uint8_t rdir_block[BLOCK_SIZE];
    if (block_read(infoSuperblock.rdir_blk, rdir_block) == -1) {
        return -1;
	}

    // Read file size
    uint32_t size;
    memcpy(&size, &rdir_block[fd_entry->rootIndex + 16], sizeof(uint32_t));

	// Read nothing if it goes beyond EOF
    if (fd_entry->offset >= size) {
        return 0;
	}

    // Get starting data block
    uint16_t block;
    memcpy(&block, &rdir_block[fd_entry->rootIndex + 20], sizeof(uint16_t));


	// Prevent go out of bound
    if (fd_entry->offset + count > size) {
        count = size - fd_entry->offset;
	}

    size_t bytes_read = 0;
    uint8_t temp_block[BLOCK_SIZE];

    // Traverse to the first reading block
    size_t skip = fd_entry->offset / BLOCK_SIZE;
    for (size_t i = 0; i < skip && block != 0xFFFF; i++) {
        block = get_next_block(block);
    }

	// Nothing to read, return 0
    if (block == 0xFFFF) {
        return 0;
	}

    size_t offset = fd_entry->offset % BLOCK_SIZE;
    while (bytes_read < count && block != 0xFFFF) {
		// Read the correct block to temp_block, in case of having offset != 0
        block_read(infoSuperblock.data_blk + block, temp_block);
        size_t to_read = BLOCK_SIZE - offset;

		// Prevent go out of bound
        if (to_read > count - bytes_read) {
            to_read = count - bytes_read;
		}

		// Read data into buf
        memcpy((uint8_t *)buf + bytes_read, temp_block + offset, to_read);
        bytes_read += to_read;
        offset = 0; // Offset stays 0 after reading the first block
        block = get_next_block(block);
    }

	// Update offset and return the # of bytes read
    fd_entry->offset += bytes_read;
    return bytes_read;
}