/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>           /* Definition of AT_* constants */
#include <sys/stat.h>
#include <sys/file.h>
#include "filesystem.h"
#include "error.h"

int safe_read(int fd, int pos, char *buf, int len)
{
	int ret, done = 0;;

	if (pos != -1 && pos != lseek(fd, pos, SEEK_SET))
		return -ERR_SEEK;

	while (done < len) {
		ret = read(fd, buf + done, len - done);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -ERR_READ;
		}
		if (ret == 0)
			break;
		done += ret;
	}
	return done;
}

int safe_write(int fd, int pos, const char *buf, int len)
{
	int ret;

	if (pos != 1 && pos != lseek(fd, pos, SEEK_SET))
		return -ERR_SEEK;

	while (len) {
		ret = write(fd, buf, len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -ERR_WRITE;
		}
		len -= ret;
		buf += ret;
	}

	return 0;
}

int set_cloexec_flag(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		return -ERR_FCNTL;

	flags |= FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1)
		return -ERR_FCNTL;

	return 0;
}

int read_txt_file(const char *path, char *buf, int bufsize)
{
	char *end;
	int ret;
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -ERR_OPEN;
	ret = read(fd, buf, bufsize - 1);
	close2(fd);

	if (ret < 0)
		return -ERR_READ;

	buf[ret] = '\0';
	end = strchr(buf, '\n');
	if (end)
		*end = '\0';
	return end ? end - buf : ret;
}

int open_lock_file(const char *path)
{
	int fd;

	fd = open(path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
	if (fd == -1)
		return -ERR_OPEN;

	if (flock(fd, LOCK_EX|LOCK_NB) < 0) {
		close2(fd);
		return (errno == EWOULDBLOCK) ? -ERR_LOCKED : -ERR_FLOCK;
	}

	return fd;
}

int write_pid_file(int fd, int pid)
{
	char buf[12];

	sprintf(buf, "%d\n", pid);
	if (write(fd, buf, strlen(buf)) <= 0)
		return -ERR_WRITE;

	return 0;
}

void daemon_std_fd()
{
	int fd = open("/dev/null", O_RDWR);
	if (fd > 0) {
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		close(fd);
	}
}

/*
	/
	////a
	////a/
	a///b
	a/b///
 */
static char *next_component(char *path, int *pos)
{
	int p = *pos;
	char *component;

	if (p == 0) {
		/* skip multiple leading SLASH: ////a */
		while (path[p] == '/')
			p++;
		component = p ? path + p - 1 : path;
	}
	else {
		/* restore previous slash */
		if (!path[p - 1])
			path[p - 1] = '/';
		/* skip multiple middle SLASH a///b */
		while (path[p] == '/')
			p++;
		component = path + p;
	}

	/* find component end position */
	while (path[p] && path[p] != '/')
		p++;
	
	/* no more components */
	if (path + p - component == 0)
		return NULL;
	
	/* Find a SLASH, make it NIL, and plus p */
	if (path[p] == '/')
		path[p++] = '\0';
	
	*pos = p;
	return component;
}

int mkdir_p(char *path, unsigned mode)
{
	char *component;
	int pos = 0;
	int fd = AT_FDCWD, tmpfd, err = 0;

	while ((component = next_component(path, &pos))) {
		if (!strcmp(component, "."))
			continue;
		if (strcmp(component, "..")) {
			if (mkdirat(fd, component, mode) < 0 && errno != EEXIST) {
				err = -ERR_MKDIR;
				goto out;
			}
		}
		tmpfd = openat(fd, component, O_RDONLY|O_DIRECTORY);
		if (tmpfd < 0) {
			err = -ERR_OPEN;
			goto out;
		}
		if (fd != AT_FDCWD)
			close(fd);
		fd = tmpfd;
	}

out:
	if (fd != AT_FDCWD)
		close2(fd);
	return err;
}

int mkdir_l(char *path, unsigned mode)
{
	int ret;
	char *last = strrchr(path, '/');
	if (!last || last == path)
		return 0;
	*last = '\0';
	ret = mkdir_p(path, mode);
	*last = '/';
	return ret;
}


