#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

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

	return 0;
}

int fs_umount(void)
{
	return block_disk_close();
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

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}
