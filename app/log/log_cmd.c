/*
 * Xrouter log util 
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "log.h"

static void usage(const char *err)
{
	if (err)
		fprintf(stderr, "%s", err);

	fprintf(stderr, "usage:\n");
	fprintf(stderr, "log list                         list modules and levels\n");
	fprintf(stderr, "log enable <module list>         enable log\n");
	fprintf(stderr, "log disable <module list>        disable log\n");
	fprintf(stderr, "log clear <module list>          clear log\n");
	fprintf(stderr, "log status <module list>         show status\n");
	fprintf(stderr, "log write <mod> <lvl> <msg...>   write log\n");
	fprintf(stderr, "log read [<module list>]         read log\n");
	fprintf(stderr, "log monitor <module list>        monitor log\n");
	fprintf(stderr, "\nmodule list example:\n");
	fprintf(stderr, "common,err,info,dbg netman\n\n");

	exit(1);
}

enum {
	OP_EN, OP_DI, OP_CL, OP_ST,
};

static int get_module_id(const char *mod)
{
	int i;
	for (i = 0; i < MOD_MAX; i++)
		if (!strcmp(mod, plog_module_name[i])) {
			return i;
		}
	return -1;
}

static int get_level_id(const char *lvl)
{
	int i;
	for (i = 0; i < LVL_MAX; i++)
		if (!strcmp(lvl, plog_level_name[i]))
			return i;
	return -1;
}

static void list_module()
{
	int i;
	for (i = 0; i < MOD_MAX; i++)
		printf("%d %s\n", i, plog_module_name[i]);
}

static void list_level()
{
	int i;
	for (i = 0; i < LVL_MAX; i++)
		printf("%d %s\n", i, plog_level_name[i]);
}

static int write_log(int module, int level, int argc, char **argv)
{
	char buf[MAX_LOG_LINE+1];
	int i, ret, n;

	n = 0;
	for (i = 0; i < argc; i++) {
		ret = snprintf(&buf[n], sizeof(buf)-n, i > 0 ? " %s" : "%s", argv[i]);
		if (ret < 0) {
			fprintf(stderr, "snprintf failed\n");
			return 1;
		}
		n += ret;
		if (n >= MAX_LOG_LINE) {
			n = MAX_LOG_LINE;
			break;
		}
	}
	buf[n] = '\0';

	return plog(module, level, "%s", buf);
}

static int read_log(uint32_t *enable)
{
	struct log_read_context ctx;
	char *line;
	int len;
	int ret;
	int module, level;
	int n = sizeof("[xxxxxx yyyy]") - 1;

	if (plog_read_open(&ctx, enable) < 0)
		return 1;
	
	while (1) {
		ret = plog_read(&ctx, &module, &level, &line, &len);
		if (ret <= 0)
			break;

		line -= n;
		len += n;
		if (n-1 != snprintf(line, MAX_EXTRA_HEAD, "[%6s %4s",
				plog_module_name[module],
				plog_level_name[level]))
			break;
		/* replace the '\0' to ']' */
		line[n-1] = ']';

		write(STDOUT_FILENO, line, len);
	}

	plog_read_close(&ctx);
	return 0;
}

static int monitor_log(uint32_t *enable)
{
	int fd;
	char buf[1024];
	int n, ret, done;
	
	fd = plog_monitor_open(enable);
	if (fd < 0) {
		fprintf(stderr, "open fifo failed: ret %d, err %s\n", fd, strerror(errno));
		return -1;
	}

	while (1) {
		ret = read(fd, buf, sizeof(buf)-1);
		if (ret <= 0)
			break;
		done = 0;
		while (done < ret)
		{
			n = write(STDOUT_FILENO, &buf[done], ret-done);
			if (n <= 0)
				break;
			done += n;
		}
	}

	return 0;
}

static int parse_mod_list(uint32_t *enable, int argc, char **argv)
{
	int i;
	char *tmp, *savedptr;
	int mod_id, lvl_id;
	uint32_t en;

	if (argc == 0) {
		memset(enable, -1, sizeof(uint32_t)*MOD_MAX);
		return 0;
	}

	memset(enable, 0, sizeof(uint32_t)*MOD_MAX);

	for (i = 0; i < argc; i++) {
		tmp = strtok_r(argv[i], ",", &savedptr);
		if (!tmp || (mod_id = get_module_id(tmp)) == -1)
			return -1;

		en = 0;
		do {
			tmp = strtok_r(NULL, ",", &savedptr);
			if (tmp) {
				if ((lvl_id = get_level_id(tmp)) == -1)
					return -1;
				en |= (1 << lvl_id);
			}
		} while (tmp);

		enable[mod_id] = en ? en : -1;
	}

	return 0;
}

static void op_file(uint32_t *enable, int op)
{
	int i, j;
	for (i = 0; i < MOD_MAX; i++) {
		for (j = 0; j < LVL_MAX; j++) {
			if (!(enable[i] & (1 << j)))
				continue;
			if (op == OP_EN)
				plog_level_enable(i, j);
			else if (op == OP_DI)
				plog_level_disable(i, j);
			else if (op == OP_CL)
				plog_clear(i, j);
			else if (op == OP_ST) {
				printf("%8s %4s: %10s | %8s\n",
					plog_module_name[i],
					plog_level_name[j],
					plog_level_unmaskable(j) ? "" : "Maskable",
					plog_level_enabled(i, j) ? "Enabled" : "Disabled");
			}
		}
	}
}

int main(int argc, char **argv)
{
	uint32_t enable[MOD_MAX];
	int module = 0, level = 0;

	if (argc == 1 || !strcmp(argv[1], "help")
		|| !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		usage(NULL);

	if (!strcmp(argv[1], "list")
		|| !strcmp(argv[1], "enable")
		|| !strcmp(argv[1], "disable")
		|| !strcmp(argv[1], "clear")
		|| !strcmp(argv[1], "status")
		|| !strcmp(argv[1], "read")
		|| !strcmp(argv[1], "monitor"))
	{
		if (parse_mod_list(enable, argc - 2, argv + 2)) {
			fprintf(stderr, "invalid module list\n");
			return 1;
		}
	}

	if (!strcmp(argv[1], "write")) {

		if (argc < 4) {
			usage("invalid arguments\n");
		}

		module = get_module_id(argv[2]);
		if (module == -1) {
			fprintf(stderr, "invalid module\n");
			return 1;
		}

		level = get_level_id(argv[3]);
		if (level == -1) {
			fprintf(stderr, "invalid level\n");
			return 1;
		}
	}
	
	if (!strcmp(argv[1], "list")) {
		printf("modules:\n");
		list_module();
		printf("\nlevels:\n");
		list_level();
		return 0;
	}

	else if (!strcmp(argv[1], "enable")) {
		op_file(enable, OP_EN);
		return 0;
	}
	else if (!strcmp(argv[1], "disable")) {
		op_file(enable, OP_DI);
		return 0;
	}
	else if (!strcmp(argv[1], "clear")) {
		op_file(enable, OP_CL);
		return 0;
	}
	else if (!strcmp(argv[1], "status")) {
		op_file(enable, OP_ST);
		return 0;
	}

	else if (!strcmp(argv[1], "read")) {
		return read_log(enable);
	}

	else if (!strcmp(argv[1], "monitor"))  {
		return monitor_log(enable);
	}

	else if (!strcmp(argv[1], "write")) {
		int ret = write_log(module, level, argc-4, argv+4);
		if (ret == -6) {
			fprintf(stderr, "this level log of this module is diabled\n");
			return 1;
		}
		else if (ret < 0) {
			fprintf(stderr, "write log failed, ret %d, err %s\n", ret, strerror(errno));
			return 1;
		}
		return 0;
	}

	usage("invalid arguments\n");
	return 1;
}

