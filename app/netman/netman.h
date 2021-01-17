#ifndef NETMAND_H
#define NETMAND_H

#include <stdint.h>
#include <linux/if.h>

#define NETMAN_CTRL_PATH	"/run/netman/ctrl.fifo"
#define DHCPC_PID_PATH	"/run/netman/dhcpc.pid"
#define DHCPC_SCRIPT	"/lib/netman/dhcpc.sh"
#define NETMAND_PID		"/run/netman/netmand.pid"

enum {
	PROTO_NONE,
	PROTO_STATIC,
	PROTO_DHCP,
	PROTO_PPPOE,
};

enum {
	STATE_INIT,
	STATE_DIALING,
	STATE_UP,
	STATE_RELEASING,
	STATE_DOWN,	/* manual down */
};

enum {
	EVENT_UP = 0,
	EVENT_DOWN,
	EVENT_RENEW,

	EVENT_DHCP_DECONFIG,
	EVENT_DHCP_LEASEFAIL,
	EVENT_DHCP_NAK,
	EVENT_DHCP_BOUND,
	EVENT_DHCP_RENEW,

	EVENT_LINK_DOWN,
	EVENT_LINK_UP,

	EVENT_MAX,
};

struct proto
{
	int proto;
	char ifname[16][IFNAMSIZ];
	uint8_t mac[6];
	uint32_t ip;
	uint32_t netmask;
	uint32_t gateway;
	uint32_t dns[8];
	char ppp_user[32];
	char ppp_pwd[32];

#define PROTO_CONFIG_SIZE	((int)&(((struct proto *)0)->name))
	/* ----------- above are config parameters ------------- */

	char name[16];
	int is_bridge;
	int state;
	int event;
	int reason;
	char l2if[IFNAMSIZ];
	char l3if[IFNAMSIZ];
	int is_wan;

	int dhcpc_pid;
	int lease;
};

struct netman_context
{
	struct proto lan;
	struct proto wan;
	int cmd_fd;
	int link_sock;
};

int read_config(struct netman_context *ctx);

/* prototypes in proto.c */
struct proto *get_proto_by_if(struct netman_context *ctx, const char *ifname);
int setup_proto(struct proto *proto, int event, void *data);

/* prototypes in dhcp.c */
int start_dhcpc(const char *ifname, const char *prog, const char *pidfile);
int stop_dhcpc(int pid);
int process_dhcp_cmd(struct netman_context *ctx, char **argv, int argc);

#endif

