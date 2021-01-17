/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef LIBBASE_ERROR_H
#define LIBBASE_ERROR_H


enum {
	ERR_OK,

	ERR_INVARG,
	ERR_NOTFOUND,
	ERR_LOCKED,
	ERR_OVERFLOW,
	ERR_MSGTRUNC,
	ERR_PARTIAL,
	ERR_INVRES,
	ERR_NOATTR,		/* no expected attribute in netlink response msg */
	ERR_OUTOFSEQ,
	ERR_NOTMATCH,

	ERR_SOCKET,
	ERR_BIND,
	ERR_RECVMSG,
	ERR_ERRMSG,
	ERR_WRITE,
	ERR_SEEK,
	ERR_READ,
	ERR_FCNTL,
	ERR_OPEN,
	ERR_FLOCK,
	ERR_MKDIR,
	ERR_IOCTL,
	ERR_IOCTLS,
	ERR_SETSOCKOPT,
	ERR_IFNAM2IDX,

	ERR_MAX,
};

const char *err_str(int err);

/* the errno is not modified */
void close2(int fd);

#endif

