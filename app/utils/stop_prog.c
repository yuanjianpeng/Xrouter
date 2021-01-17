#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "libbase/filesystem.h"

#define MAX_MS	5000

int main(int argc, char **argv)
{
	char pid[32];
	char exe[64];
	char bin[256];
	int pid_no;
	int i = 0;
	int bin_len;

	if (argc != 3)
		return 1;

	if (read_txt_file(argv[2], pid, sizeof(pid)) < 0)
		return 0;

	sprintf(exe, "/proc/%s/exe",  pid);
	pid_no = atoi(pid);
	bin_len = strlen(argv[1]);

	while (i < MAX_MS) {
		int n = readlink(exe, bin, sizeof(bin)-1);
		if (n <= 0)
			return 0;
		if (bin_len != n || memcmp(bin, argv[1], n))
			return 0;
		kill(pid_no, SIGTERM);
		usleep(2000);
		i += 2;
	}

	return 1;
}

