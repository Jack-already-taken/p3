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
	uint32_t offset;
};

// Flag to determine if fs is mounted or not
bool mount = false;

struct Superblock superblock;
uint16_t *FAT;
struct RootEntry rootEntries[FS_FILE_MAX_COUNT];
int FATLength;

/* Phase 3 */
/**
 * fdTable is the datastructure used to keep track of fd, which are integers returned by fs_open, used by fs_close, fs_read, fs_write, etc.
 * fd denotes a file that is currently open for reading and writing
 * you may find in FileDescriptor struct that there's only fields for entryIndex and offset, but none for fd
 * this is because fd is represented by the index to the FileDescriptor within fdTable
 * 
 * Example usage:
 * if we have the fd, and want to know where is the file associated with fd located in rootEntries, using:
 * 
 * fdTable[fd].entryIndex
 * 
 * will give us the index in rootEntries that contains the information about the file
 * say if we wnat to access the name of the file associated with fd, we would use:
 * 
 * rootEntries[fdTable[fd].entryIndex].filename
*/
struct FileDescriptor fdTable[FS_OPEN_MAX_COUNT];

/**
 * Find last FAT block of a file
 * return FAT block index, or FAT_EOC if file length is 0
*/
int findFATEnd(int fd)
{
	int FATEnd = rootEntries[fdTable[fd].entryIndex].dataStartIndex;
	if (FATEnd == FAT_EOC)
	{
		return FAT_EOC;
	}
	for (; FAT[FATEnd] != FAT_EOC; FATEnd = FAT[FATEnd]);
	return FATEnd;
}

int findFATStart(int fd, long *offset)
{
	int FATStart = rootEntries[fdTable[fd].entryIndex].dataStartIndex;
	while (*offset >= BLOCK_SIZE)
	{
		*offset -= BLOCK_SIZE;
		FATStart = FAT[FATStart];
	}
	return FATStart;
}

/**
 * Allocate a new block for file pointed to by fd
 * return FAT index of new space if successful
 * return -1 if no space to allocate
*/
int falloc(int fd)
{
	// Find index for the new block
	int i = 0;
	for (; i < FATLength && FAT[i] != 0; i++);
	if (i == FATLength)
	{
		return -1;
	}
	FAT[i] = FAT_EOC;
	int FATEnd = findFATEnd(fd);
	if (FATEnd == FAT_EOC)
	{
		rootEntries[fdTable[fd].entryIndex].dataStartIndex = i;
	}
	else
	{
		FAT[FATEnd] = i;
	}
	return i;
}

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
	// Copy the FAT and Root directory back to the original disk
	for (unsigned int i = 0; i < superblock.FATLen; i++)
	{
		block_write(i+1, &FAT[i * FAT_PER_BLOCK]);
	}

	block_write(superblock.rootDir_Index, rootEntries);
	

	// try to close the disk file
	if (block_disk_close() == -1) 
	{
		return -1;
	}

	free(FAT);
	mount = false;
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
	if (!mount || strlen(filename) >= FS_FILENAME_LEN || filename == NULL)
	{
		return -1;
	}

	int i = 0;
	while(i < FS_FILE_MAX_COUNT)
	{
		if (strcmp(rootEntries[i].filename, filename) == 0)
		{
			return -1;
		}
		i++;
	}

	i = 0;
	while (i < FS_FILE_MAX_COUNT)
	{
		// cannot use filename == NULL to check for \0, use strlen instead
		if (strlen(rootEntries[i].filename) == 0)  
		{
			// -> is not the way to access elements in an array, change all of them to index
			strcpy(rootEntries[i].filename, filename);
			rootEntries[i].fileSize = 0;
			rootEntries[i].dataStartIndex = FAT_EOC;
			return 0;
		}
		i++;
	}

	// Reaching this line means no free space in rootEntries can be found
	// Therefore meaning that there already exists 128 files
	return -1;
}


int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	// same checking as fs_create
	if (!mount || strlen(filename) >= FS_FILENAME_LEN || filename == NULL)
	{
		return -1;
	}

	// Implements checking if file to be deleted is currently open. This depends on datastrucure for fd
	// fdTable stores the open file's index in rootEntries in its entryIndex field
	// therefore, use entryIndex to access the filename of each open file in rootEntries and check if
	// it is the same as the filename of the file to be deleted
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
	{
		if (strcmp(rootEntries[fdTable[i].entryIndex].filename, filename) == 0)
		{
			return -1;
		}
	}

	int i = 0;
	while (i < FS_FILE_MAX_COUNT) // replace filename null check with index i's checking
	{
		if (strcmp(rootEntries[i].filename, filename) == 0) // strcmp returns 0 if two strings match
		{
			uint16_t fatIndex = rootEntries[i].dataStartIndex;
			if (fatIndex != FAT_EOC)
			{
				while (FAT[fatIndex] != FAT_EOC)
				{
					uint16_t tempfatIndex = 0;
					tempfatIndex = FAT[fatIndex];
					FAT[fatIndex] = 0;
					fatIndex = tempfatIndex;
				}
				FAT[fatIndex] = 0;
			}

			// also reset the filename to show a space is free in rootEntries
			// setting first character to \0 is sufficient
			rootEntries[i].filename[0] = '\0';
			return 0;
		}
		i++;
	}

	// file name is not in rootEntries
	return -1;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	if (!mount)
	{
		return -1;
	}
	int i = 0;
	printf("FS Ls:\n");
	while (i < FS_FILE_MAX_COUNT)
	{
		//maybe need a loop for filename[]
		if (strlen(rootEntries[i].filename) != 0)
			printf("file: %s, size: %d, data_blk: %d\n", rootEntries[i].filename, rootEntries[i].fileSize, rootEntries[i].dataStartIndex);
		i++;
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	// return -1 if disk is not mounted and filename is invalid
	if (!mount || strlen(filename) == 0 || strlen(filename) >= FS_FILENAME_LEN)
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
	for (; fd < FS_OPEN_MAX_COUNT && fdTable[fd].entryIndex != FD_EMPTY; fd++);
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

	fdTable[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	
	/* TODO: Phase 4 */
	// Check if fd is valid and if disk is mounted
	if (!mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fdTable[fd].entryIndex == FD_EMPTY || buf == NULL)
	{
		return -1;
	}

	long remainingByte = count;
	char bounce[BLOCK_SIZE];
	// Find block location of offset to start
	long offset = fdTable[fd].offset;
	int FATIndex = findFATStart(fd, &offset);
	if (FATIndex == FAT_EOC)
	{
		int result = falloc(fd);
		if (result == -1)
		{
			//need to fix??
			fdTable[fd].offset += count - remainingByte;
			if (rootEntries[fdTable[fd].entryIndex].fileSize < fdTable[fd].offset)
			{
				rootEntries[fdTable[fd].entryIndex].fileSize = fdTable[fd].offset;
			}
			return count - remainingByte;
		}
		FATIndex = rootEntries[fdTable[fd].entryIndex].dataStartIndex;
	}
	block_read(superblock.dataB_startIndex + FATIndex, bounce);

	// Case 1: write first block
	long readByte;
	if (BLOCK_SIZE - offset - 1 < remainingByte)
	{
		readByte = BLOCK_SIZE - offset - 1;
	}
	else 
	{
		readByte = count;
	}
	remainingByte -= readByte;
	memcpy(bounce + offset, buf, readByte);
	block_write(superblock.dataB_startIndex + FATIndex, bounce);
	
	// Case 2: write intermediate full blocks
	while (remainingByte > BLOCK_SIZE)
	{
		if (FAT[FATIndex] == FAT_EOC)
		{
			int result = falloc(fd);
			if (result == -1)
			{
				//need to fix??
				fdTable[fd].offset += count - remainingByte;
				if (rootEntries[fdTable[fd].entryIndex].fileSize < fdTable[fd].offset)
				{
					rootEntries[fdTable[fd].entryIndex].fileSize = fdTable[fd].offset;
				}
				return count - remainingByte;
			}
		}
		FATIndex = FAT[FATIndex];
		block_write(superblock.dataB_startIndex + FATIndex, buf + (count - remainingByte));
		remainingByte -= BLOCK_SIZE;
	}

	// Case 3: write last block
	if (remainingByte > 0)
	{
		if (FAT[FATIndex] == FAT_EOC)
		{
			int result = falloc(fd);
			if (result == -1)
			{
				//need to fix??
				fdTable[fd].offset += count - remainingByte;
				if (rootEntries[fdTable[fd].entryIndex].fileSize < fdTable[fd].offset)
				{
					rootEntries[fdTable[fd].entryIndex].fileSize = fdTable[fd].offset;
				}
				return count - remainingByte;
			}
		}
		FATIndex = FAT[FATIndex];
		block_read(superblock.dataB_startIndex + FATIndex, bounce);
		memcpy(bounce,  buf + (count - remainingByte), remainingByte);
		block_write(superblock.dataB_startIndex + FATIndex, bounce);
		remainingByte -= remainingByte;
	}

	fdTable[fd].offset += count - remainingByte;
	if (rootEntries[fdTable[fd].entryIndex].fileSize < fdTable[fd].offset)
	{
		rootEntries[fdTable[fd].entryIndex].fileSize = fdTable[fd].offset;
	}
	return count - remainingByte;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	if (!mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || fdTable[fd].entryIndex == FD_EMPTY || buf == NULL) {
		return -1;
	}
	
	long remainingByte = count;
	long readByte = 0;
	char bounce[BLOCK_SIZE];
	long offset = fdTable[fd].offset; // starting offset from the first reading
	int FATIndex = findFATStart(fd, &offset);

	if (rootEntries[fdTable[fd].entryIndex].fileSize - fdTable[fd].offset < remainingByte) {
		remainingByte = rootEntries[fdTable[fd].entryIndex].fileSize - fdTable[fd].offset;
	}
	readByte = remainingByte;

	//first read
	if(readByte == 0) {
		block_read(superblock.dataB_startIndex + FATIndex, bounce);
		// the first read block is not the end of file but reading ends in a block
		if(remainingByte < (BLOCK_SIZE - offset)) {
			memcpy(buf, bounce + offset, remainingByte);
			fdTable[fd].offset += readByte;
			return readByte;
		}
		// read until the end of the first block
		else {
			memcpy(buf, bounce + offset, (BLOCK_SIZE - offset)); 
			remainingByte = remainingByte - (BLOCK_SIZE - offset);
			FATIndex = FAT[FATIndex];
		}
	}
	while (remainingByte > BLOCK_SIZE) {
		// read whole block
		block_read(superblock.dataB_startIndex + FATIndex, buf + (readByte - remainingByte));
		remainingByte = remainingByte - BLOCK_SIZE;
		FATIndex = FAT[FATIndex];
	}
	// read end of block 
	if(remainingByte > 0) {
		block_read(superblock.dataB_startIndex + FATIndex, bounce);
		memcpy(buf + (readByte - remainingByte), bounce, remainingByte);
	}

	// file size and actual file offset. 
	fdTable[fd].offset += readByte;
	return readByte;
}
