#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define ROOT_ENTRY_SIZE 128
#define FAT_PER_BLOCK 2048
#define FAT_EOC 0xffff

/* TODO: Phase 1 */

// packed attribute to keep the struct size stable
struct __attribute__((__packed__)) Superblock
{
	uint64_t signature;
	uint16_t blockCount;
	uint16_t rootIndex;
	uint16_t dataIndex;
	uint16_t dataCount;
	uint8_t FATLen;
	int8_t padding[4079];
};

struct __attribute__((__packed__)) FATBlock
{
	uint16_t index[2048];
};

struct __attribute__((__packed__)) RootEntry
{
	char filename[16];
	uint32_t fileSize;
	uint16_t dataStartIndex;
	int8_t padding[10];
};

struct __attribute__((__packed__)) RootBlock
{
	struct RootEntry entries[128];
};

struct Superblock superblock;
uint16_t *FAT;
struct RootEntry *rootEntries;
int FATLength;

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	// open disk
	int out = block_disk_open(diskname);
	if (out == -1)
	{
		return -1;
	}
	
	// read superblock
	block_read(0, &superblock);
	
	// perform signature checking
	char* signature = "ECS150FS";
	if (strncmp((char *)&superblock.signature, signature, 8) != 0)
	{
		return -1;
	}

	// check if number of blocks is correct
	if (block_disk_count() != superblock.blockCount)
	{
		return -1;
	}

	// check the validity of FAT and Root blocks
	uint32_t FATLen = (uint32_t)superblock.dataCount * 2 / BLOCK_SIZE;
	if ((uint32_t)superblock.dataCount * 2 % BLOCK_SIZE != 0)
	{
		FATLen += 1;
	}
	if (FATLen != superblock.FATLen || FATLen + 1 != superblock.rootIndex 
	|| superblock.rootIndex + 1 != superblock.dataIndex)
	{
		return -1;
	}

	// initialize FAT
	FATLength = superblock.FATLen * FAT_PER_BLOCK;
	FAT = malloc(sizeof(uint16_t) * FATLength);
	FAT[0] = FAT_EOC;

	// read root block
	rootEntries = malloc(sizeof(struct RootEntry) * ROOT_ENTRY_SIZE);

	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	// Check if disk was mounted
	if (FAT == NULL || rootEntries == NULL)
	{
		return -1;
	}

	// try to close the disk file
	if (block_disk_close() == -1) 
	{
		return -1;
	}
	
	free(FAT);
	free(rootEntries);
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	int freeFAT = 0, freeRootEntries = 0;
	for (int i = 0; i < FATLength; i++) 
	{
		if (FAT[i] == 0) 
		{
			freeFAT++;
		}
	}

	for (int i = 0; i < ROOT_ENTRY_SIZE; i++)
	{
		if (strlen(rootEntries[i].filename) == 0) {
			freeRootEntries++;
		}
	}
	printf("FS Info:\ntotal_blk_count=%d\nfat_blk_count=%d\nrdir_blk=%d\ndata_blk=%d\ndata_blk_count=%d\n"
	"fat_free_ratio=%d/%d\nrdir_free_ratio=%d/%d\n", superblock.blockCount, superblock.FATLen, superblock.rootIndex,
	superblock.dataIndex, superblock.dataCount, freeFAT, FATLength, freeRootEntries, ROOT_ENTRY_SIZE);

	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	return (int) (*filename);
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	return (int) (*filename);
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	return (int) (*filename);
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	return fd;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	return fd;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	return fd + offset;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return fd + (*(int*)(buf)) + count;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return fd + (*(int*)(buf)) + count;
}

