#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fs.h>

#define ASSERT(cond, func)                               \
do {                                                     \
	if (!(cond)) {                                       \
		fprintf(stderr, "Function '%s' failed\n", func); \
		exit(EXIT_FAILURE);                              \
	}                                                    \
} while (0)

int main(int argc, char *argv[])
{
	int ret;
	char *diskname;
	int fd;
	FILE *fdFile = fopen("test_fs.c", "r");
	char data[9230];
    char refdata[4096 * 10];

	if (argc < 1) {
		printf("Usage: %s <diskimage>\n", argv[0]);
		exit(1);
	}

	/* Mount disk */
	diskname = argv[1];
	ret = fs_mount(diskname);
	ASSERT(!ret, "fs_mount");

	/* Create file and open */
	ret = fs_create("test_fs.c");
	ASSERT(!ret, "fs_create");

	fd = fs_open("test_fs.c");
	ASSERT(fd >= 0, "fs_open");

	/* Write some data */
	char* out = fgets(data, sizeof(data), fdFile);
	if (!out) {
		return -1;
	}
	memcpy(refdata, data, sizeof(data));
	ret = fs_write(fd, data, sizeof(data));
	ASSERT(ret == sizeof(data), "fs_write");

	/* Read some data */
	fs_lseek(fd, 500);
	ret = fs_read(fd, data, 8000);
	ASSERT(ret == 8000, "fs_read");
	ASSERT(!strncmp(data + 500, refdata + 500, 8000), "fs_read");

	/* Close file and unmount */
	fs_close(fd);
	fs_umount();

	return 0;
}