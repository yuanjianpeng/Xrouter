#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "system.h"
#include "error.h"

int r_meminfo(unsigned long *total, unsigned long *free)
{
	char tmp[128];
	FILE *f = fopen("/proc/meminfo", "r");
	if (f == NULL) {
		return -ERR_OPEN;
	}
	while (fgets(tmp, sizeof(tmp), f)) {
		if (!strncmp(tmp, "MemTotal:", 9)) {
			*total = strtoul(tmp + 9, NULL, 10);
		}
		else if (!strncmp(tmp, "MemFree:", 8)) {
			*free = strtoul(tmp + 8, NULL, 10);
			break;
		}
	}
	fclose(f);
	return 0;
}

int r_uptime(unsigned long *sec)
{
	char tmp[128];
	FILE *f = fopen("/proc/uptime", "r");
	if (!f)
		return -ERR_OPEN;
	if (fgets(tmp, sizeof(tmp), f)) {
		*sec = strtoul(tmp, NULL, 10);
	}
	fclose(f);
	return 0;
}

int r_cpustat(struct cpu_time *time, int cpu_n)
{
	char tmp[256];
	int n = 0;
	char *next;
	FILE *f = fopen("/proc/stat", "r");
	if (!f)
		return -ERR_OPEN;
	while (fgets(tmp, sizeof(tmp), f)) {
		if (!strncmp(tmp, "cpu", 3)) {
			time[n].user = strtoull(tmp+4, &next, 10);
			time[n].nice = strtoull(next, &next, 10);
			time[n].system = strtoull(next, &next, 10);
			time[n].idle = strtoull(next, &next, 10);
			time[n].iowait = strtoull(next, &next, 10);
			time[n].irq = strtoull(next, &next, 10);
			time[n].softirq = strtoull(next, &next, 10);
			if (++n >= cpu_n)
				break;
		}
		else
			break;
	}
	fclose(f);

	return n;
}

