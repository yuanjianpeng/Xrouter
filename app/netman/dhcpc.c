#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "log/log.h"
#include "libbase/net.h"
#include "libbase/filesystem.h"
#include "netman.h"

#define UDHCPC "/sbin/udhcpc"

/* return -1 if failed, 
	else pid of udhcpc */
int start_dhcpc(const char *ifname, const char *prog, const char *pidfile)
{
	char * const argv[] = {
		UDHCPC,
		"-i", (char *)ifname,
		"-s", (char *)prog,
		"-p", (char *)pidfile,
		"-f",		// run in foreground after obtain lease
		"-R",		// release on exit
		NULL,
	};

	char netman_pid[32];

	int pid;

	pid = fork();
	if (pid < 0) {
		return -1;
	}
	else if (pid > 0)
		return pid;

	daemon_std_fd();

	/*
		busybox udhcpc can run multiple instance.
		so the pid of udhcpc is passed to event script program and
		pass to netmand, netmand only accept dhcp event of udhcpc started up by itself.
	*/
	sprintf(netman_pid, "%d", getppid());
	setenv("NETMAN_PID", netman_pid, 1);

	execv(UDHCPC, argv);
	plog(NETMAN, ERR, "execv %s failed: %s", UDHCPC, strerror(errno));
	_exit(1);
	return 1;
}

int stop_dhcpc(int pid)
{
	return kill(pid, SIGTERM);
}

static int parse_dhcp_res(struct proto *proto, char **argv, int argc)
{
	int i, pid = 0;
	uint32_t ip = 0, gateway = 0, dns = 0;
	int prefix_len = 0, lease = 0;

	for (i = 0; i + 1 < argc; i+= 2) {
		if (!strcmp(argv[i], "-pid"))
			pid = atoi(argv[i+1]);
		else if (!strcmp(argv[i], "-ip"))
			ip = inet_addr(argv[i+1]);
		else if (!strcmp(argv[i], "-mask"))
			prefix_len = atoi(argv[i+1]);
		else if (!strcmp(argv[i], "-gw"))
			gateway = inet_addr(argv[i+1]);
		else if (!strcmp(argv[i], "-dns"))
			dns = inet_addr(argv[i+1]);
		else if (!strcmp(argv[i], "-lease"))
			lease = atoi(argv[i+1]);
	}

	if (pid && pid != getpid()) {
		plog(NETMAN, ERR, "receive others dhcp response, pid %d", pid);
		return -1;
	}

	if (!ip || prefix_len <= 0 || prefix_len >= 32) {
		plog(NETMAN, ERR, "invalid dhcp response");
		return -1;
	}

	proto->ip = ip;
	proto->netmask = get_netmask(prefix_len);
	proto->gateway = gateway;
	proto->dns[0] = dns;
	proto->lease = lease;

	return 0;
}

static int get_dhcp_event(const char *cmd)
{
	static struct 
	{
		const char *name;
		int event;
	} dhcp_event[] = {
		{ "deconfig", EVENT_DHCP_DECONFIG, },
		{ "leasefail", EVENT_DHCP_LEASEFAIL, },
		{ "nak", EVENT_DHCP_NAK, },
		{ "bound", EVENT_DHCP_BOUND, },
		{ "renew", EVENT_DHCP_RENEW, },
	};

	int i;

	for (i = 0; i < sizeof(dhcp_event)/sizeof(dhcp_event[0]); i++) {
		if (!strcmp(dhcp_event[i].name, cmd))
			return dhcp_event[i].event;
	}

	return -1;
}

/*
	dhcp deconfig|nak|bound|renew|leasefail <ifname> <options>
			-pid pid, -ip <ip>, -mask <mask> -gw <gw> -dns <dns>
			-lease lease
 */
int process_dhcp_cmd(struct netman_context *ctx, char **argv, int argc)
{
	char *cmd;
	char *ifname;
	struct proto *proto;
	void *data = NULL;
	int event;

	if (argc < 2)
		return -1;

	cmd = argv[0];
	ifname = argv[1];

	event = get_dhcp_event(cmd);
	if (event == -1) {
		plog(NETMAN, ERR, "invalid dhcp cmd %s", cmd);
		return -2;
	}

	proto = get_proto_by_if(ctx, ifname);
	if (proto == NULL) {
		plog(NETMAN, ERR, "no this proto on interface %s", ifname);
		return -3;
	}

	if (event == EVENT_DHCP_BOUND || event == EVENT_DHCP_RENEW) {
		if (parse_dhcp_res(proto, argv+2, argc-2) < 0) {
			plog(NETMAN, ERR, "parse dhcp res failed");
			return -4;
		}
	}

	if (setup_proto(proto, event, data) < 0) {
		plog(NETMAN, ERR, "setup proto failed");
		return -5;
	}

	return 0;
}

