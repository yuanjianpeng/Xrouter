#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "log/log.h"
#include "xbus/xbus.h"
#include "netman.h"
#include "libbase/net.h"
#include "libbase/filesystem.h"
#include "libbase/netlink.h"
#include <linux/rtnetlink.h>

static int daemonlize = 1;
static volatile int normal_exit = 0;
static int pid_fd = -1;
struct netman_context netman_ctx;

static void usage()
{
	fprintf(stderr, "usage: %s [-d]\n", "netmand");
	exit(EXIT_FAILURE);
}

static void exit_clear()
{
	if (!normal_exit)
		plog(NETMAN, ERR, "abnormal exit");
	
	if (netman_ctx.wan.dhcpc_pid > 0)
		kill(netman_ctx.wan.dhcpc_pid, SIGTERM);

	unlink(NETMAND_PID);
}

static void sig_handler(int signum)
{
	switch (signum)
	{
		case SIGINT:
		case SIGTERM:
			normal_exit = 1;
			/* exit at main loop, to prevent 
				lan/wan setting is breaked */
			break;
		default:
			normal_exit = 0;
			exit(0);
	};
}

static int fd_init(struct netman_context *ctx)
{
	ctx->cmd_fd = xbus_bind(XBUS_MOD_NETMAN);
	if (ctx->cmd_fd < 0) {
		plog(NETMAN, ERR, "xbus_bind() failed, ret: %d, err: %s", ctx->cmd_fd, strerror(errno));
		return -1;
	}

	ctx->link_sock = nl_sock_init(NETLINK_ROUTE);
	if (ctx->link_sock < 0) {
		plog(NETMAN, ERR, "rtlink_sock_init() failed\n");
		return -1;
	}

	return 0;
}

static int config_init(struct netman_context *ctx)
{
	if (read_config(ctx) < 0) {
		plog(NETMAN, ERR, "read config failed");
		return -1;
	}

	strcpy(ctx->lan.name, "lan");
	ctx->lan.is_bridge = 1;
	strcpy(ctx->lan.l2if, "br-lan");
	
	ctx->wan.is_wan = 1;
	strcpy(ctx->wan.name, "wan");
	if (ctx->wan.proto != PROTO_NONE)
		strcpy(ctx->wan.l2if, ctx->wan.ifname[0]);

	return 0;
}

/* up all protos before enter main loop */
static int init_up_proto(struct netman_context *ctx)
{
	int ret;

	ret = setup_proto(&ctx->lan, EVENT_UP, NULL);
	if (ret < 0) {
		plog(NETMAN, ERR, "set proto %s up failed", ctx->lan.name);
	}

	ret = setup_proto(&ctx->wan, EVENT_UP, NULL);
	if (ret < 0) {
		plog(NETMAN, ERR, "set proto %s up failed", ctx->wan.name);
	}

	return 0;
}

static int process_cmd(char *buf, int bufsize, int buflen, void *priv)
{
	char *argv[32];
	int i, argc, ret = -100;

	static struct cmd_cb {
		const char *name;
		int (* cb)(struct netman_context *, char **, int);
	} cmd_cb[] = {
		{ "dhcp", process_dhcp_cmd },
	};

	argc = xbus_parse_req(buf, argv, sizeof(argv)/sizeof(argv[0]));
	if (argc <= 0) {
		plog(NETMAN, ERR, "parse xbus request arguments failed");
		goto out;
	}
	
	for (i = 0; i < sizeof(cmd_cb)/sizeof(cmd_cb[0]); i++) {
		if (!strcmp(argv[0], cmd_cb[i].name)) {
			ret = cmd_cb[i].cb((struct netman_context *)priv, &argv[1], argc-1);
			break;
		}
	}

out:
	return sprintf(buf, "%d\n", ret);
}

static int process_link(struct netman_context *ctx)
{
	char buf[1024];
	int ret;
	struct nlmsghdr *nlh;

	ret = nl_recv(ctx->link_sock, buf, sizeof(buf));
	if (ret <= 0)
		return 1;

	for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, (unsigned int)ret);
			nlh = NLMSG_NEXT(nlh, ret))
	{
		struct ifinfomsg *ifi = NLMSG_DATA(nlh);
		const char *ifname = NULL;
		struct rtattr *rta = (struct rtattr *)((char *)NLMSG_DATA(nlh) + NLMSG_ALIGN(sizeof(*ifi)));
		int rtasize = NLMSG_PAYLOAD(nlh, sizeof(*ifi));
		int link_up = 0;

		if (nlh->nlmsg_type != RTM_NEWLINK)
			continue;

		if (ifi->ifi_flags & IFF_LOWER_UP)
			link_up = 1;

		for ( ; RTA_OK(rta, rtasize); rta = RTA_NEXT(rta, rtasize)) {
			if (rta->rta_type == IFLA_IFNAME) {
				ifname = RTA_DATA(rta);
				break;
			}
		}

		printf("%s %s\n", ifname, link_up ? "up" : "down");
	}
	return 0;
}

static void init_signals()
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

	/* abnormal terminate */
	signal(SIGABRT, sig_handler);
	signal(SIGBUS, sig_handler);
	signal(SIGFPE, sig_handler);
	signal(SIGSEGV, sig_handler);
	signal(SIGSYS, sig_handler);
	signal(SIGILL, sig_handler);
}

static void wait_subprocess(struct netman_context *ctx)
{
	if (ctx->wan.dhcpc_pid > 0) {
		int ret = waitpid(ctx->wan.dhcpc_pid, NULL, WNOHANG);
		if (ret == ctx->wan.dhcpc_pid) {
			plog(NETMAN, INFO, "dhcpc exit");
			ctx->wan.dhcpc_pid = 0;
		}
	}
}

int main(int argc, char **argv)
{
	int ret;
	fd_set rfds;
	struct timeval tv;
	int maxfd;
	char buf[512];

	int opt;
	while ((opt = getopt(argc, argv, "p:dh")) != -1 ) {
		switch(opt) {
		case 'd':
			daemonlize = 0;
			break;
		default:
			usage();
		}
	}

	/* lock to prevent multiple instance */
	pid_fd = open_lock_file(NETMAND_PID);
	if (pid_fd < 0)
		return 1;

	if (daemonlize) {
		int pid = fork();
		if (pid < 0)
			return 1;
		else if (pid > 0) {
			write_pid_file(pid_fd, pid);
			return 0;
		}

		daemon_std_fd();
	}

	init_signals();
	atexit(exit_clear);

	if (config_init(&netman_ctx) < 0) {
		plog(NETMAN, ERR, "netmand exit: read net config failed");
		return 1;
	}

	if (fd_init(&netman_ctx) < 0) {
		plog(NETMAN, ERR, "fd_init() failed");
		return 1;
	}

	if (init_up_proto(&netman_ctx) < 0) {
		plog(NETMAN, ERR, "init_up_proto failed");
		return -1;
	}

	maxfd = netman_ctx.cmd_fd;
	if (netman_ctx.link_sock > maxfd)
		maxfd = netman_ctx.link_sock;
	maxfd++;

	while (1) {

		if (normal_exit) {
			exit(0);
		}

		FD_ZERO(&rfds);
		FD_SET(netman_ctx.cmd_fd, &rfds);
		FD_SET(netman_ctx.link_sock, &rfds);

		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		ret = select(maxfd, &rfds, NULL, NULL, &tv);
		if (ret <= 0) {
			if (ret < 0 && errno != EINTR) {
				plog(NETMAN, ERR, "select failed: %s", strerror(errno));
				break;
			}
			continue;
		}

		if (FD_ISSET(netman_ctx.cmd_fd, &rfds)) {
			xbus_response(netman_ctx.cmd_fd, buf, sizeof(buf), process_cmd, &netman_ctx);
		}

		if (FD_ISSET(netman_ctx.link_sock, &rfds))
			process_link(&netman_ctx);

		wait_subprocess(&netman_ctx);
	}

	return 1;
}

