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

static struct superblock infoSuperblock;

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
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}
