#include "libswitch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log/log.h"
#include "libbase/error.h"
#include <errno.h>

void usage()
{
	fprintf(stderr, "sw phy read <phy addr> <reg>\n");
	fprintf(stderr, "sw phy write <phy addr> <reg> <val>\n");
	exit(1);
}

int phy_read(const char *addr, const char *reg)
{
	int _addr = strtoul(addr, NULL, 0);
	int _reg = strtoul(reg, NULL, 0);
	int val = 0;
	int ret, sock = switch_open();
	if (sock < 0) {
		plog(CMN, ERR, "switch open failed: %s", err_str(sock));
		return 1;
	}
	ret = switch_phy_read(sock, _addr, _reg, &val);
	if (ret < 0) {
		plog(CMN, ERR, "switch phy read failed: %s: %s",
			err_str(ret), strerror(errno));
	}
	else
		printf("phy %d reg 0x%x val 0x%04x\n", _addr, _reg, val);
	switch_close(sock);
	return ret ? 1 : 0;
}

int phy_write(const char *addr, const char *reg, const char *val)
{
	unsigned long _addr = strtoul(addr, NULL, 0);
	unsigned long _reg = strtoul(reg, NULL, 0);
	unsigned long _val = strtoul(val, NULL, 0);
	int ret, sock = switch_open();
	if (sock < 0) {
		plog(CMN, ERR, "switch open failed: %s", err_str(sock));
		return 1;
	}
	ret = switch_phy_write(sock, _addr, _reg, _val);
	if (ret < 0) {
		plog(CMN, ERR, "switch phy write failed: %s: %s",
			err_str(ret), strerror(errno));
	}
	switch_close(sock);
	return ret ? 1 : 0;
}

int phy_status(const char *addr)
{
	int _addr = strtoul(addr, NULL, 0);
	int link, speed, duplex;
	int ret, sock = switch_open();
	if (sock < 0) {
		plog(CMN, ERR, "switch open failed: %s", err_str(sock));
		return 1;
	}
	ret = switch_phy_status(sock, _addr, &link, &speed, &duplex);
	if (ret < 0) {
		plog(CMN, ERR, "switch phy read failed: %s: %s",
			err_str(ret), strerror(errno));
	}
	else {
		if (link) {
			printf("phy %d link Up %dMbps %s Duplex\n", _addr,
				speed, duplex ? "Full" : "Half");
		}
		else
			printf("phy %d link Down\n", _addr);
	}
	switch_close(sock);
	return ret ? 1 : 0;
}

int sw_read(const char *reg)
{
	int _reg = strtoul(reg, NULL, 0);
	int val = 0;
	int ret, sock = switch_open();
	if (sock < 0) {
		plog(CMN, ERR, "switch open failed: %s", err_str(sock));
		return 1;
	}
	ret = switch_sw_read(sock, _reg, &val);
	if (ret < 0) {
		plog(CMN, ERR, "switch read failed: %s: %s",
			err_str(ret), strerror(errno));
	}
	else
		printf("switch reg 0x%04x val 0x%08x\n", _reg, val);
	switch_close(sock);
	return ret ? 1 : 0;
}

int sw_write(const char *reg, const char *val)
{
	unsigned long _reg = strtoul(reg, NULL, 0);
	unsigned long _val = strtoul(val, NULL, 0);
	int ret, sock = switch_open();
	if (sock < 0) {
		plog(CMN, ERR, "switch open failed: %s", err_str(sock));
		return 1;
	}
	ret = switch_sw_write(sock, _reg, _val);
	if (ret < 0) {
		plog(CMN, ERR, "switch write failed: %s: %s",
			err_str(ret), strerror(errno));
	}
	switch_close(sock);
	return ret ? 1 : 0;
}
int main(int argc, char **argv)
{
	plog_to_stderr = 1;

	if (argc == 5 && !strcmp(argv[1], "phy")
		&& !strcmp(argv[2], "read"))
		return phy_read(argv[3], argv[4]);

	if (argc == 6 && !strcmp(argv[1], "phy")
		&& !strcmp(argv[2], "write"))
		return phy_write(argv[3], argv[4], argv[5]);

	if (argc == 4 && !strcmp(argv[1], "phy")
		&& !strcmp(argv[2], "status"))
		return phy_status(argv[3]);

	if (argc == 4 && !strcmp(argv[1], "switch")
		&& !strcmp(argv[2], "read"))
		return sw_read(argv[3]);

	if (argc == 5 && !strcmp(argv[1], "switch")
		&& !strcmp(argv[2], "write"))
		return sw_write(argv[3], argv[4]);

	usage();
	return 0;
}
