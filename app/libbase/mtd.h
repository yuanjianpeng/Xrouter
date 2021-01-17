#ifndef LIBBASE_MTD_H
#define LIBBASE_MTD_H

int mtd_find(const char *partition_name);
int mtd_open(int part_id, int flags);
int mtd_info(int fd, int *size, int *erasesize, int *type);
int mtd_erase(int fd, int offset, int len);
int mtd_read(int fd, int offset, char *data, int size);
int mtd_write(int fd, int offset, int erasesize, char *data, int size);

#endif

