/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "error.h"
#include <errno.h>
#include <unistd.h>

static const char *errstr_table[ERR_MAX] = {
	[ERR_OK]			= "No error",

	[ERR_INVARG]		= "Invalid argument",
	[ERR_NOTFOUND]		= "Not found",
	[ERR_LOCKED]		= "Locked already",
	[ERR_OVERFLOW]		= "Overflow",
	[ERR_MSGTRUNC]		= "Msg truncated",
	[ERR_PARTIAL]		= "Partial data",
	[ERR_INVRES]		= "Invalid response",
	[ERR_NOATTR]		= "No attributes",
	[ERR_OUTOFSEQ]		= "Out of sequence",
	[ERR_NOTMATCH]		= "Not match",

	[ERR_SOCKET]		= "Create socket failed",
	[ERR_BIND]			= "Bind socket failed",
	[ERR_RECVMSG]		= "Recvmsg failed",
	[ERR_ERRMSG]		= "Netlink error msg contain error",
	[ERR_WRITE]			= "Write failed",
	[ERR_SEEK]			= "Seek failed",
	[ERR_READ]			= "Read failed",
	[ERR_FCNTL]			= "Fcntl failed",
	[ERR_OPEN]			= "Open failed",
	[ERR_FLOCK]			= "Flock failed",
	[ERR_MKDIR]			= "Mkdir failed",
	[ERR_IOCTL]			= "Ioctl failed",
	[ERR_IOCTLS]		= "Ioctl set failed",
	[ERR_SETSOCKOPT]	= "Setsockopt failed",
	[ERR_IFNAM2IDX]		= "Ifname to index failed",

};

const char *err_str(int err)
{
	if (err < 0 || err >= ERR_MAX)
		return "Unknown error";
	return errstr_table[err];
}

void close2(int fd)
{
	int saved = errno;
	close(fd);
	if (errno != saved)
		errno = saved;
}

