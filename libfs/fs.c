#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_PER_BLOCK 2048
#define FAT_EOC 0xffff
#define FD_EMPTY -1

/* TODO: Phase 1 */

// packed attribute to keep the struct size stable
struct __attribute__((__packed__)) Superblock
{
	uint64_t signature;
	uint16_t blockCount;
	uint16_t rootDir_Index;
	uint16_t dataB_startIndex;
	uint16_t dataBCount;
	uint16_t FATLen;
	int8_t padding[4079];
};

struct __attribute__((__packed__)) RootEntry
{
	char filename[16];
	uint32_t fileSize;
	uint16_t dataStartIndex;
	int8_t padding[10];
};

/* phase 3 */
struct FileDescriptor
{
	int entryIndex;
	int offset;
};

// Flag to determine if fs is mounted or not
bool mount = false;

struct Superblock superblock;
uint16_t *FAT;
struct RootEntry rootEntries[FS_FILE_MAX_COUNT];
int FATLength;

/* Phase 3 */
struct FileDescriptor fdTable[FS_OPEN_MAX_COUNT];
int openFileCount = 0;

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
	uint32_t FATLen = (uint32_t)superblock.dataBCount * 2 / BLOCK_SIZE;
	if ((uint32_t)superblock.dataBCount * 2 % BLOCK_SIZE != 0)
	{
		FATLen += 1;
	}
	if (FATLen != superblock.FATLen || FATLen + 1 != superblock.rootDir_Index 
	|| superblock.rootDir_Index + 1 != superblock.dataB_startIndex)
	{
		return -1;
	}

	// initialize FAT
	FATLength = superblock.dataBCount;
	FAT = malloc(sizeof(uint16_t) * superblock.FATLen * FAT_PER_BLOCK);
	for (unsigned int i = 0; i < superblock.FATLen; i++)
	{
		block_read(i+1, &FAT[i * FAT_PER_BLOCK]);
	}

	// read root block
	block_read(superblock.rootDir_Index, rootEntries);

	// initialize fdTable so that all entries are available
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
	{
		fdTable[i].entryIndex = FD_EMPTY;
	}

	mount = true;

	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	// Check if disk was mounted
	if (!mount)
	{
		return -1;
	}

	// try to close the disk file
	if (block_disk_close() == -1) 
	{
		return -1;
	}
	
	free(FAT);
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	if (!mount)
	{
		return -1;
	}

	int freeFAT = 0, freeRootEntries = 0;
	for (int i = 0; i < FATLength; i++) 
	{
		if (FAT[i] == 0) 
		{
			freeFAT++;
		}
	}

	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if (strlen(rootEntries[i].filename) == 0) {
			freeRootEntries++;
		}
	}
	printf("FS Info:\ntotal_blk_count=%d\nfat_blk_count=%d\nrdir_blk=%d\ndata_blk=%d\ndata_blk_count=%d\n"
	"fat_free_ratio=%d/%d\nrdir_free_ratio=%d/%d\n", superblock.blockCount, superblock.FATLen, superblock.rootDir_Index,
	superblock.dataB_startIndex, superblock.dataBCount, freeFAT, FATLength, freeRootEntries, FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	if (!mount || filename >= FS_FILENAME_LEN || filename == NULL || block_disk_count == FS_FILE_MAX_COUNT)
	{
		return -1;
	}
	int i = 0;
	while(i < FS_FILE_MAX_COUNT)
	{
		if (!strcmp(rootEntries[i].filename, filename))
		{
			return -1;
		}
		i++;
	}
	i = 0;
	while (i < FS_FILE_MAX_COUNT)
	{
		if (rootEntries[i].filename == NULL)
		{
			strcpy(rootEntries->filename, filename);//need to check
			rootEntries->fileSize = 0;
			rootEntries->dataStartIndex = FAT_EOC;
			return 0;
		}
		i++;
	}

	return (int)(*filename);
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	// if file @filename is currently open return -1
	if (!mount || filename >= FS_FILENAME_LEN || filename == NULL)
	{
		return -1;
	}
	int i = 0;
	while (!rootEntries[i].filename)
	{
		if (!strcmp(rootEntries[i].filename, filename))
		{
			uint16_t fatIndex = rootEntries[i].dataStartIndex;
			while (FAT[fatIndex] != FAT_EOC)
			{
				uint16_t tempfatIndex = 0;
				tempfatIndex = FAT[fatIndex];
				FAT[fatIndex] = 0;
				fatIndex = tempfatIndex;
			}
			FAT[fatIndex] = 0;
			return 0;
		}
		else //filename is not on rootEntries
		{
			return -1;
		}
		i++;
	}
	return (int)(*filename);
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	int i = 0;
	while (rootEntries[i].filename != NULL)
	{
		printf("%s/n", rootEntries[i].filename);
		i++;
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	// return -1 if disk is not mounted and filename is invalid
	if (!mount || strlen(filename) == 0 || strlen(filename) > FS_FILENAME_LEN-1 || openFileCount == FS_OPEN_MAX_COUNT)
	{
		return -1;
	}

	// Find filename in root directory
	int fileIndex = 0;
	while (fileIndex < FS_FILE_MAX_COUNT && strcmp(rootEntries[fileIndex].filename, filename) != 0)
	{
		fileIndex++;
	}
	
	// Condition for if file not found in root directory
	if (fileIndex == FS_FILE_MAX_COUNT)
	{
		return -1;
	}

	// Find a valid fd
	int fd = 0;
	for (; fd < FS_OPEN_MAX_COUNT && fdTable[fd].entryIndex != -1; fd++);
	if (fd == FS_OPEN_MAX_COUNT)
	{
		return -1;
	}
	fdTable[fd].entryIndex = fileIndex;
	fdTable[fd].offset = 0;
	
	return fd;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	// Check if fd is valid and if disk is mounted
	if (!mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fdTable[fd].entryIndex == FD_EMPTY)
	{
		return -1;
	}

	fdTable[fd].entryIndex = FD_EMPTY;

	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	// Check if fd is valid and if disk is mounted
	if (!mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fdTable[fd].entryIndex == FD_EMPTY)
	{
		return -1;
	}

	return rootEntries[fdTable[fd].entryIndex].fileSize;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	// Check if fd is valid and if disk is mounted
	if (!mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fdTable[fd].entryIndex == FD_EMPTY)
	{
		return -1;
	}

	// Check if new offset is greater than current file size
	if (offset >= rootEntries[fdTable[fd].entryIndex].fileSize)
	{
		return -1;
	}

	rootEntries[fdTable[fd].entryIndex].fileSize = offset;
	return 0;
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

