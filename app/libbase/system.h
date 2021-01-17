#ifndef LIBBASE_SYSTEM_H
#define LIBBASE_SYSTEM_H

struct cpu_time
{
	unsigned long long user, nice, system, idle,
		iowait, irq, softirq;
};

int r_meminfo(unsigned long *total, unsigned long *free);
int r_uptime(unsigned long *sec);
int r_cpustat(struct cpu_time *time, int cpu_n);

#endif

