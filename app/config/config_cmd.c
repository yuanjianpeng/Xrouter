#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "log/log.h"

#define CMD "cfg"

enum {
	CFG_LOAD, CFG_UNLOAD, CFG_GET, CFG_SET, CFG_SAVE, CFG_ERASE, CFG_SHOW,
};

void usage(const char *err)
{
	if (err) 
		fprintf(stderr, "%s", err);
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "%s load\n", CMD);
	fprintf(stderr, "%s unload\n", CMD);
	fprintf(stderr, "%s show [<section>]\n", CMD);
	fprintf(stderr, "%s get <section> <key>\n", CMD);
	fprintf(stderr, "%s set <section> <key> [<value>]\n", CMD);
	fprintf(stderr, "%s save\n", CMD);
	fprintf(stderr, "%s erase\n", CMD);
	fprintf(stderr, "%s -h|--help|help\n", CMD);
	exit(1);
}

int do_config(int op, const char *sec, const char *key, const char *value)
{
	int flag = 0;
	int ret;
	const char *val;
	struct config_context ctx;

	if (op == CFG_UNLOAD)
		flag = CFG_START_WITH_RM_SHM;
	else if (op == CFG_LOAD)
		flag = CFG_START_WITH_CREATE_SHM;

	if (op != CFG_SAVE && op != CFG_SET)
		flag |= CFG_START_WITH_LOCK;
	
	ret = config_start(&ctx, flag);
	if (ret < 0) 
		return 1;

	switch (op) {
	case CFG_LOAD:
		ret = config_load(&ctx);
		break;
	case CFG_UNLOAD:
		break;
	case CFG_GET:
		val = config_get(&ctx, sec, key);
		if (val)
			printf("%s\n", val);
		break;
	case CFG_SET:
		ret = config_set(&ctx, sec, key, value);
		break;
	case CFG_SHOW:
		config_dump(&ctx, sec);
		break;
	case CFG_ERASE:
		ret = config_erase(&ctx);
		break;
	case CFG_SAVE:
		ret = config_save(&ctx);
		break;
	default:
		ret = -1;
	}

	if (ret < 0) {
		fprintf(stderr, "operation failed\n");
	}

	ctx.changed = 0;
	config_end(&ctx);

	return ret ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc == 1)
		usage(NULL);
	
	plog_to_stderr = 1;

	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "help"))
		usage(NULL);
	
	else if (!strcmp(argv[1], "load"))
		return do_config(CFG_LOAD, NULL, NULL, NULL);

	else if (!strcmp(argv[1], "unload"))
		return do_config(CFG_UNLOAD, NULL, NULL, NULL);
	
	else if (!strcmp(argv[1], "get")) {
		if (argc != 4)
			usage("invalid argument\n");
		return do_config(CFG_GET, argv[2], argv[3], NULL);
	}

	else if (!strcmp(argv[1], "set")) {
		if (argc != 5)
			usage("invalid argument\n");
		return do_config(CFG_SET, argv[2], argv[3], argv[4]);
	}

	else if (!strcmp(argv[1], "save"))
		return do_config(CFG_SAVE, NULL, NULL, NULL);

	else if (!strcmp(argv[1], "erase"))
		return do_config(CFG_ERASE, NULL, NULL, NULL);

	else if (!strcmp(argv[1], "show")) {
		if (argc != 2 && argc != 3)
			usage("invalid argument\n");
		return do_config(CFG_SHOW, argc == 3 ? argv[2] : NULL, NULL, NULL);
	}

	usage("invalid agument\n");
	return 1;
}

