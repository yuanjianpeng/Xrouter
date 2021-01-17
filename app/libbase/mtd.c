#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include "mtd.h"
#include "error.h"

int mtd_find(const char *partition_name)
{
	char buf[256];
	char *s;
	int i;
	int name_len = strlen(partition_name);

	FILE *f = fopen("/proc/mtd", "r");
	if (f == NULL)
		return -ERR_OPEN;

	while (fgets(buf, sizeof(buf), f)) {
		if (memcmp(buf, "mtd", 3))
			continue;
		if (1 != sscanf(buf, "mtd%d:", &i))
			continue;
		s = strstr(buf, partition_name);
		if (s == NULL || *(s-1) != '"' || *(s+name_len) != '"')
			continue;
		fclose(f);
		return i;
	}

	fclose(f);
	return -ERR_NOTFOUND;
}

int mtd_open(int part_id, int flags)
{
	char name[32];
	int fd;
	sprintf(name, "/dev/mtd%d", part_id);
	fd = open(name, flags);
	return fd < 0 ? -ERR_OPEN : fd;
}

int mtd_info(int fd, int *size, int *erasesize, int *type)
{
	struct mtd_info_user mtdInfo;
	if (ioctl(fd, MEMGETINFO, &mtdInfo)) 
		return -ERR_IOCTL;
	if (size) *size = mtdInfo.size;
	if (erasesize) *erasesize = mtdInfo.erasesize;
	if (type) *type = mtdInfo.type;
	return 0;
}

int mtd_erase(int fd, int offset, int len)
{
	struct erase_info_user mtdEraseInfo;
	int err;
	mtdEraseInfo.start = offset;
	mtdEraseInfo.length = len;
	ioctl(fd, MEMUNLOCK, &mtdEraseInfo);
	err = ioctl(fd, MEMERASE, &mtdEraseInfo);
	return err < 0 ? -ERR_IOCTL : 0;
}

int mtd_read(int fd, int offset, char *data, int size)
{
	int readed, ret;

	if (offset != -1 && -1 == lseek(fd, offset, SEEK_SET))
		return -ERR_SEEK;

	readed = 0;
	while (readed < size) {
		ret = read(fd, data + readed, size - readed);
		if (ret < 0) {
			if (ret == EINTR)
				continue;
			return -ERR_READ;
		}
		else if (ret == 0)
			break;
		readed += ret;
	}

	return readed;
}

int mtd_write(int fd, int offset, int erasesize, char *data, int size)
{
	int tail, ret;

	if (offset != -1 && -1 == lseek(fd, offset, SEEK_SET))
		return -ERR_SEEK;
	if (offset == -1)
		offset = 0;
	if (offset % erasesize)
		return -ERR_INVARG;

	tail = offset + size;
	while (offset < tail) {
		ret = mtd_erase(fd, offset, erasesize);
		if (ret < 0)
			return ret;
		offset += erasesize;
	}

	offset = 0;
	while (offset < size) {
		int towrite = size - offset;
		if (towrite > erasesize) {
			towrite = erasesize;
			/* write until to next block */
			if (offset % erasesize)
				towrite = (offset+erasesize)/erasesize*erasesize - offset;
		}
		ret = write(fd, data + offset, towrite);
		if (ret < 0) {
			if (errno != EINTR) 
				return -ERR_WRITE;
			continue;
		}
		offset += ret;
	}

	return 0;
}

