/*
 * Xrouter log library 
 * 2020, YuanJianpeng
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#define LOG_DIR			"/run/log/"
#define LOG_CTRL_DIR	LOG_DIR "ctrl/"
#define LOG_PIPE		LOG_CTRL_DIR "pipe"
#define LOG_PIPE_MASK	LOG_CTRL_DIR "pipe_mask"
#define MAX_LOG_LINE	384

/* for monitor/read, add extra prefix to log, 
   so that user can get usefull information, e.g. pid, module name */
#define MAX_EXTRA_HEAD	32

enum
{
	LOGE_OK = 0,
	LOGE_DISABLED,	/* this log level is disabled */
};

enum 
{
	CMN, CFG, NETMAN,
	MOD_MAX,
};

enum
{
	ERR, INFO, DBG,
	LVL_MAX,
};

/* when log is written to pipe, if the reader close the pipe,
	the default handler of SIGPIPE will exit the process, 
	set this variable, to let plog routine ignore SIGPIPE
	before write to pipe, and restore the old handler after write.
 */
extern int plog_sigpipe_ignore;

/* control whether log to stderr */
extern unsigned int plog_to_stderr;

extern const char *plog_module_name[MOD_MAX];
extern const char *plog_level_name[LVL_MAX];

/* keep the size smallest possible */
struct __log_file
{
	char *line;
	int fd;
	int pos, rpos;
	int len;
	uint32_t sec, msec;
	char end;
	char has_line;
	char module, level;
};

struct log_read_context
{
	int n;
	struct __log_file log[MOD_MAX*LVL_MAX];
};

void plog_clear(int module, int level);
int plog_level_unmaskable(int level);
int plog_level_enabled(int module, int level);
void plog_level_enable(int module, int level);
void plog_level_disable(int module, int level);

/* return 0, ok, -n failed */
int plog(int module, int level, const char *fmt, ...);

/* create fifo & open it 
	-1: mkfifo() failed, check errno for the failed reason.
	-2: open() fifo failed, check errno for the failed reason.
	else: return fd of the opened fifo
 */
int plog_monitor_open(uint32_t *enable);

int plog_read_open(struct log_read_context *ctx, uint32_t *enable);

/* return
	0 no more log
	1 get a log
	note: the line is not null-terminated, but 1 byte is reserved 
		a '\0' can be appended to the tail safely.
 */
int plog_read(struct log_read_context *ctx, int *module, int *level,
		char **line, int *len);

void plog_read_close(struct log_read_context *ctx);

#endif

