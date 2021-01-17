
#include <sys/types.h>
#include <dirent.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xbus.h"

#define COMM "xbus"

void usage()
{
	fprintf(stderr, COMM " list\n");
	fprintf(stderr, COMM " help [<module>]\n");
	fprintf(stderr, COMM " call <module> <command> [args...]\n");
	exit(1);
}

int call(int argc, char **argv)
{
	char buf[XBUS_BUF_SIZE];
	int buflen = 0;
	int ret;

	for (ret = 1; ret < argc; ret++)
		buflen += snprintf(&buf[buflen], sizeof(buf) - buflen, "%s\n", argv[ret]);

	ret = xbus_request(argv[0], XBUS_TIMEOUT, buf, sizeof(buf), buflen);
	if (ret <= 0) {
		fprintf(stderr, "xbus request `%s\" failed, ret %d, errstr: %s\n", argv[0], ret, strerror(errno));
	}
	else {
		buflen = 0;
		do
		{
			int err = write(STDOUT_FILENO, &buf[buflen], ret - buflen);
			if (err <= 0) {
				if (err < 0 && errno == EINTR)
					continue;
				break;
			}
			buflen += err;
		}
		while (buflen < ret);
	}

	return 0;
}

int enum_dir(const char *path, void (* cb)(struct dirent *))
{
	struct dirent *dirent;
	DIR *dirp;
	int err = 0;
	
	if ((dirp = opendir(XBUS_DIR)) == NULL) {
		fprintf(stderr, "opendir %s failed: %s\n", XBUS_DIR, strerror(errno));
		return 1;
	}

	while (1) {
		errno = 0;
		dirent = readdir(dirp);
		if (dirent == NULL) {
			if (errno) {
				err = 1;
				fprintf(stderr, "readdir failed: %s\n", strerror(errno));
			}
			closedir(dirp);
			break;
		}
		if (dirent->d_type == DT_SOCK)
			cb(dirent);
	}

	return err;
}

void print_name(struct dirent *dirent)
{
	printf("%s\n", dirent->d_name);
}

void print_help(struct dirent *dirent)
{
	char *args[2];
	args[0] = dirent->d_name;
	args[1] = "help";
	printf("===\n%s:\n", args[0]);
	call(2, args);
	printf("\n");
}

int list()
{
	return enum_dir(XBUS_DIR, print_name);
}

int help(int argc, char **argv)
{
	char *args[2];
	if (argc == 0)
		return enum_dir(XBUS_DIR, print_help);
	args[0] = argv[0];
	args[1] = "help";
	return call(2, args);
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcmp(argv[1], "list"))
			return list();
		else if (!strcmp(argv[1], "help"))
			return help(argc-2, argv+2);
		else if (!strcmp(argv[1], "call") && argc >= 4)
			return call(argc-2, argv+2);
	}

	usage();
	return 1;
}

