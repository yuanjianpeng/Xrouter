/*
 * Xrouter log library 
 * 2020, YuanJianpeng
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/file.h>
#include "log.h"
#include "libbase/filesystem.h"
#include "libbase/base.h"

#define POS_SIZE	10	/* 000000000\n */

/* runtime modify logsize is not supported, because
	this value is compiled into shared library */
static const int logsize[MOD_MAX][LVL_MAX] = 
{
	{ 65536, 65536, 65536 },	/* common */
	{ 8192, 8192, 8192 },		/* config */
	{ 8192, 8192, 8192 },		/* netmanager */
};

const char *plog_module_name[MOD_MAX] = {
	"common", "config", "netman",
};

const char *plog_level_name[LVL_MAX] = {
	"err", "info", "dbg",
};

int plog_sigpipe_ignore = 1;
unsigned int plog_to_stderr;

////////////////////// ctrl

static inline void level_ctrl_path(char *path, int module, int level)
{
	sprintf(path, LOG_CTRL_DIR "enable-%s-%s",
			plog_module_name[module],
			plog_level_name[level]);
}

static inline void pipe_ctrl_path(char *path, int module, int level)
{
	sprintf(path, LOG_CTRL_DIR "pipe-%s-%s",
			plog_module_name[module],
			plog_level_name[level]);
}

static inline void log_file_path(char *path, int module, int level)
{
	sprintf(path, LOG_DIR "%s.%s.log",
			plog_module_name[module],
			plog_level_name[level]);
}

void plog_clear(int module, int level)
{
	char path[128];
	log_file_path(path, module, level);
	unlink(path);
}

int plog_level_unmaskable(int level)
{
	if (level == ERR || level == INFO)
		return 1;
	return 0;
}

int plog_level_enabled(int module, int level)
{
	char path[128];
	if (plog_level_unmaskable(level))
		return 1;
	level_ctrl_path(path, module, level);
	return !access(path, F_OK);
}

void plog_level_enable(int module, int level)
{
	char path[128];
	if (plog_level_unmaskable(level))
		return;
	level_ctrl_path(path, module, level);
	creat(path, 0777);
}

void plog_level_disable(int module, int level)
{
	char path[128];
	if (plog_level_unmaskable(level))
		return;
	level_ctrl_path(path, module, level);
	unlink(path);
}

static int pipe_enabled(int module, int level)
{
	char path[128];
	pipe_ctrl_path(path, module, level);
	return !access(path, F_OK);
}

static void pipe_enable(int module, int level, int en)
{
	char path[128];
	pipe_ctrl_path(path, module, level);
	if (en)
		creat(path, 0777);
	else
		unlink(path);
}

static int add_prefix(char *msg, int msgsize)
{
	struct timespec ts;
	int ret;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		return -1;

	ret = snprintf(msg, msgsize, "[%07u.%03u] ",
			(uint32_t)ts.tv_sec,
			((uint32_t)(ts.tv_nsec/1000000))%1000);

	if (ret <= 0)
		return -2;
	
	return ret;
}

static int parse_prefix(char *msg, int msglen, uint32_t *sec, uint32_t *ms)
{
	char *pos, *tmp, *savedptr;

	if (msg[0] != '[')
		return -1;

	pos = strnchr(msg, msglen, ']');
	if (!pos || pos == (msg + msglen - 1) || *(pos+1) != ' ')
		return -2;

	*sec = strtoul(msg+1, &tmp, 10);
	if (*tmp != '.' || tmp != msg + 8)
		return -3;
	
	*ms = strtoul(tmp+1, &savedptr, 10);
	if (*savedptr != ']' || savedptr != tmp + 4)
		return -4;
	
	return 0;
}

///////////////////////////// pipe

static void log_to_pipe(int module, int level, char *msg, int len)
{
	int fd;
	struct sigaction oldaction = {0};
	int n = sizeof("[123456 1234 12345]") - 1;

	fd = open(LOG_PIPE, O_WRONLY|O_NONBLOCK);
	if (fd == -1)
		return;
	
	if (!pipe_enabled(module, level))
		goto out;

	if (plog_sigpipe_ignore) {
		struct sigaction newaction = {0};
		newaction.sa_handler = SIG_IGN;
		if (-1 == sigaction(SIGPIPE, &newaction, &oldaction))
			goto out;
	}

	msg -= n;
	len += n;
	/* cuz we can monitor some modules at a time,
		so we want which module and level but also the pid of this msg */
	if (n-1 != snprintf(msg, MAX_EXTRA_HEAD, "[%6s %4s %5d",
			plog_module_name[module],
			plog_level_name[level], getpid()))
		goto out;
	/* replace the '\0' to ']' */
	msg[n-1] = ']';

	/* kernel insure that multiple write to the same pipe
		with msg size less than PIPE_BUF, are not interleaved */
	write(fd, msg, len);

	if (plog_sigpipe_ignore)
		sigaction(SIGPIPE, &oldaction, NULL);

out:
	close(fd);
}

int plog_monitor_open(uint32_t *enable)
{
	int i, j, fd;

	unlink(LOG_PIPE);

	for (i = 0; i < MOD_MAX; i++) {
		for (j = 0; j < LVL_MAX; j++) {
			pipe_enable(i, j, enable[i] & (1 << j));
		}
	}

	if (mkfifo(LOG_PIPE, 0777) == -1)
		return -1;

	fd = open(LOG_PIPE, O_RDWR);
	if (fd == -1)
		return -2;
	
	return fd;
}

////////////////////////////////// file


/* return
	-1: open log file failed
	-2: lock log file failed
	-3: read log file failed
	fd
 */
static int log_file_open(int module, int level, int rd, int *pos)
{
	char path[128];
	int ret, len, fd;

	log_file_path(path, module, level);
	fd = open(path, rd ? O_RDONLY : O_RDWR|O_CREAT, 0777);
	if (fd == -1)
		return -1;

	if (flock(fd, LOCK_EX) < 0) {
		ret = -2;
		goto out;
	}

	len = safe_read(fd, -1, path, POS_SIZE);
	if (len < 0) {
		ret = -3;
		goto out;
	}
	if (len == POS_SIZE) {
		path[POS_SIZE-1] = '\0';
		*pos = strtol(path, 0, 10);
	}
	else
		*pos = 0;

	ret = 0;

out:
	if (ret < 0) {
		close(fd);
		return ret;
	}
	return fd;
}

/* return
	-4: write log failed
	-5: write pos failed
	0: ok
 */
static int log_file_write(int fd, int pos, int maxsz, char *msg, int len)
{
	int n;
	char tmp[32];

	if (pos <= POS_SIZE)
		pos = POS_SIZE;
	
	if (pos + len > maxsz) {
		ftruncate(fd, pos);
		pos = POS_SIZE;
	}
	
	if (safe_write(fd, pos, msg, len) < 0)
		return -4;

	n = sprintf(tmp, "%09d\n", pos + len);
	if (safe_write(fd, 0, tmp, n) < 0)
		return -5;
	
	return 0;
}

/* return 
	< 0: error
	0: end of file
	1: ok
 */
static int log_file_read(struct __log_file *log)
{
	int ret;
	int len;
	char *end;

	while (1)
	{
		ret = safe_read(log->fd, log->rpos, log->line, MAX_LOG_LINE);
		if (ret < 0)
			return ret;

		else if (ret == 0) {
			if (log->pos != POS_SIZE) {
				log->rpos = POS_SIZE;
				continue;
			}

			/* all log readed */
			return 0;
		}

		end = strnchr(log->line, ret, '\n');
		if (end == NULL)
			return -1;
		len = end - log->line;

		if (parse_prefix(log->line, len, &log->sec, &log->msec) < 0)
		{
			if (log->rpos < log->pos && log->rpos + len >= log->pos)
				return 0;

			log->rpos += len;
			continue;
		}

		if (log->rpos < log->pos && log->rpos + len >= log->pos)
			log->end = 1;

		log->len = len;
		log->rpos += len;
		return 1;
	}
}

/* return
	-1: open log file failed
	-2: lock log file failed
	-3: read log file failed
	-4: write log failed
	-5: write pos failed
	-6: log is not enabled
	0: ok
 */
static int log_to_file(int module, int level, char *msg, int len)
{
	int fd, pos, maxsz, ret, saved_errno;

	if (!plog_level_enabled(module, level))
		return -6;

	maxsz = logsize[module][level];
	if (maxsz == 0)
		maxsz = 8192;

	fd = log_file_open(module, level, 0, &pos);
	if (fd < 0)
		return fd;
	
	ret = log_file_write(fd, pos, maxsz, msg, len);
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return ret;
}

/* always return 0 */
int plog_read_open(struct log_read_context *ctx, uint32_t *enable)
{
	int i, j, fd, pos;
	char *line;

	memset(ctx, 0, sizeof(struct log_read_context));

	for (i = 0; i < MOD_MAX; i++)
	{
		for (j = 0; j < LVL_MAX; j++)
		{
			if (!(enable[i] & (1 << j)))
				continue;

			fd = log_file_open(i, j, 1, &pos);
			if (fd < 0)
				continue;

			line = malloc(MAX_EXTRA_HEAD+MAX_LOG_LINE+1);
			if (line == NULL) {
				close(fd);
				continue;
			}

			if (pos < POS_SIZE)
				pos = POS_SIZE;

			ctx->log[ctx->n].fd =fd;
			ctx->log[ctx->n].line = line + MAX_EXTRA_HEAD;
			ctx->log[ctx->n].rpos = ctx->log[ctx->n].pos = pos;
			ctx->log[ctx->n].module = i;
			ctx->log[ctx->n].level = j;
			ctx->n++;
		}
	}

	return 0;
}

/* return
	0 no more log
	1 get a log
 */
int plog_read(struct log_read_context *ctx, int *module, int *level,
		char **line, int *len)
{
	int i;
	struct __log_file *log; 
	int sel = -1;
	uint32_t sec, msec;
	int ret;

	// get an oldest log from all log files
	for (i = 0; i < ctx->n; i++)
	{
		log = &ctx->log[i];

		if (log->end && !log->has_line)
			continue;

		if (!log->has_line) {
			ret = log_file_read(log);
			if (ret <= 0) {
				log->end = 1;
				continue;
			}
			log->has_line = 1;
		}

		if (sel == -1 || log->sec < sec || 
			(log->sec == sec && log->msec < msec))
		{
			sel = i;
			sec = log->sec;
			msec = log->msec;
		}
	}

	if (sel == -1)
		return 0;

	ctx->log[sel].has_line = 0;

	*line = ctx->log[sel].line;
	*len = ctx->log[sel].len;
	*module = ctx->log[sel].module;
	*level = ctx->log[sel].level;

	return 1;
}

void plog_read_close(struct log_read_context *ctx)
{
	int i;
	for (i = 0; i < ctx->n; i++) {
		close(ctx->log[i].fd);
		free(ctx->log[i].line-MAX_EXTRA_HEAD);
	}
}

/* return
	-1: open log file failed
	-2: lock log file failed
	-3: read log file failed
	-4: write log failed
	-5: write pos failed
	-6: log is not enabled
	-7: invalid module or level
	-8: add prefix failed, maybe get clock failed
	0: ok
 */
int plog(int module, int level, const char *fmt, ...)
{
	va_list ap;
	char buf[MAX_EXTRA_HEAD+MAX_LOG_LINE+1];
	char *msg;
	int i, n, pn;

	if (module < 0 || module >= MOD_MAX
		|| level < 0 || level >= LVL_MAX)
		return -7;
	
	msg = &buf[MAX_EXTRA_HEAD];
	pn = add_prefix(msg, MAX_LOG_LINE);
	if (pn < 0)
		return -8;

	va_start(ap, fmt);
	/* 
		snprintf always write null-terminating '\0' to the output string.
		write max `size' bytes (include '\0').
		the return value is the bytes needed to hold the result (excluding the '\0')
	 */
	n = vsnprintf(msg+pn, MAX_LOG_LINE+1-pn, fmt, ap);
	va_end(ap);

	if (n < 0)
		return -9;

	n += pn;
	/* truncate too long output */
	if (n >= MAX_LOG_LINE)
		n = MAX_LOG_LINE;

	/* ensure a \n at tail, if no, add one */
	if (msg[n-1] != '\n') {
		if (n == MAX_LOG_LINE)
			msg[n-1] = '\n';
		else
			msg[n++] = '\n';
	}

	/* replace middle \r\n to space */
	for (i = 0; i < n - 1; i++) {
		if (msg[i] == '\r' || msg[i] == '\n')
			msg[i] = ' ';
	}

	if (plog_to_stderr)
		write(STDERR_FILENO, msg+pn, n-pn);
	
	log_to_pipe(module, level, msg, n);
	return log_to_file(module, level, msg, n);
}

