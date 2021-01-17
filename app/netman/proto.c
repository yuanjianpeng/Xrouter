#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include "netman.h"
#include "log/log.h"
#include "libbase/net.h"
#include "libbase/error.h"

static int setup_l2_device(struct proto *proto)
{
	int ret, i;

	if (proto->is_bridge)
	{
		ret = add_br(proto->l2if);
		if (ret < 0 && errno != EEXIST) {
			plog(NETMAN, ERR, "create bridge %s failed: %d (%s)",
					proto->l2if, ret, strerror(errno));
		}
		for (i = 0; i < sizeof(proto->ifname)/sizeof(proto->ifname[0]); i++) {
			if (proto->ifname[i][0] == '\0')
				break;
			ret = add_brif(proto->l2if, proto->ifname[i]);
			if (ret < 0 && !if_in_br(proto->l2if, proto->ifname[i])) {
				plog(NETMAN, ERR, "add %s to %s failed: %d (%s)",
						proto->ifname[i], proto->l2if, ret, strerror(errno));
			}
			ret = if_up(proto->ifname[i]);
			if (ret < 0) {
				plog(NETMAN, ERR, "up %s failed, %d (%s)", proto->ifname[i], ret, strerror(errno)); 
			}
		}
	}
	
	ret = if_up(proto->l2if);
	if (ret < 0) {
		plog(NETMAN, ERR, "up %s failed, ret %d, err %s", proto->l2if, ret, strerror(errno)); 
		return -1;
	}

	return 0;
}

int set_ipv4_addr(uint32_t ifindex, uint32_t local, int prefixlen)
{
	int n, err, i, found = 0;
	struct ipv4_addr addr[10];

	n = get_ipv4_addr(ifindex, addr, 10);
	if (n < 0) {
		plog(NETMAN, ERR, "get ipv4 addr failed: %s: %s", err_str(n), strerror(errno));
		return -1;
	}

	for (i = 0; i < n; i++) {
		if (addr[i].prefixlen == prefixlen && addr[i].local == local) {
			found = 1;
			continue;
		}
		err = del_ipv4_addr(ifindex, addr[i].local);
		if (err < 0) {
			plog(NETMAN, ERR, "del ipv4 addr failed: %s: %s", err_str(err), strerror(errno));
			return -1;
		}
	}

	if (found)
		return 0;
	
	err = add_ipv4_addr(ifindex, local, prefixlen, 1);
	if (err < 0) {
		plog(NETMAN, ERR, "add ipv4 addr failed: %s: %s", err_str(err), strerror(errno));
		return -1;
	}

	return 0;
}

int set_ipv4_route(uint32_t ifindex, uint32_t ip, int prefixlen, uint32_t gw, int is_wan)
{
	struct ipv4_route route[16];
	int n, err, i;
	int default_route_found = 0, link_route_found = 0;
	uint32_t subnet = get_subnet(ip, prefixlen);

	n = get_ipv4_route(route, 16);
	if (n < 0) {
		plog(NETMAN, ERR, "get ipv4 route failed: %s: %s", err_str(n), strerror(errno));
		return -1;
	}

	for (i = 0; i < n; i++)
	{
		/* default route */
		if (is_wan && route[i].dst == 0) {
			if (gw == route[i].gw && ifindex == route[i].oif && ip == route[i].prefsrc) {
				default_route_found = 1;
				continue;
			}
			/* fall through to delete this default route */
		}
		else {
			if (ifindex != route[i].oif)
				continue;
			if (subnet == route[i].dst && prefixlen == route[i].dst_len && ip == route[i].prefsrc) {
				link_route_found = 1;
				continue;
			}
			/* fall through to delete this route */
		}


		err = del_ipv4_route(route[i].dst, route[i].dst_len,
				route[i].prefsrc, route[i].gw, route[i].oif, route[i].priority);
		if (err < 0) {
			plog(NETMAN, ERR, "del ipv4 route failed: %s: %s", err_str(err), strerror(errno));
			return -1;
		}
	}

	/* add link local route firstly,
		or default route with source ip will failed */
	if (!link_route_found) {
		err = add_ipv4_route(subnet, prefixlen, ip, 0, ifindex, 0);
		if (err < 0) {
			plog(NETMAN, ERR, "add ipv4 route failed: %s: %s", err_str(err), strerror(errno));
			return -1;
		}
	}

	if (!default_route_found && is_wan) {
		err = add_ipv4_route(0, 0, ip, gw, ifindex, 0);
		if (err < 0) {
			plog(NETMAN, ERR, "add ipv4 default route failed: %s: %s", err_str(err), strerror(errno));
			return -1;
		}
	}
	
	return 0;
}

static int apply_addr_route(struct proto *proto)
{
	int prefixlen = get_prefixlen(proto->netmask);
	const char *ifname = proto->l2if;
	uint32_t ifindex;

	if (proto->proto == PROTO_PPPOE)
		ifname = proto->l3if;
	
	if ((ifindex = if_nametoindex(ifname)) == 0) {
		plog(NETMAN, ERR, "get ifindex of %s failed: %s", ifname);
		return -1;
	}

	if (set_ipv4_addr(ifindex, proto->ip, prefixlen) < 0) {
		plog(NETMAN, ERR, "set %s ipv4 addr failed", proto->name);
		return -1;
	}

	if (set_ipv4_route(ifindex, proto->ip, prefixlen, proto->gateway, proto->is_wan) < 0) {
		plog(NETMAN, ERR, "set %s ipv4 route failed", proto->name);
		return -1;
	}

	return 0;
}

struct proto *get_proto_by_if(struct netman_context *ctx, const char *ifname)
{
	if (!strcmp(ctx->lan.l2if, ifname))
		return &ctx->lan;

	else if (!strcmp(ctx->wan.l2if, ifname))
		return &ctx->wan;
	
	return NULL;
}

int setup_proto(struct proto *proto, int event, void *data)
{
#define FSM(state, event) ((state) << 8 | (event))
	switch (FSM(proto->state, event)) {
	case FSM(STATE_INIT, EVENT_UP):
		if (setup_l2_device(proto) < 0) {
			plog(NETMAN, ERR, "setup l2 device failed, proto %s", proto->name);
		}
		if (proto->proto == PROTO_DHCP) {
			proto->dhcpc_pid = start_dhcpc(proto->l2if, DHCPC_SCRIPT, DHCPC_PID_PATH);
			if (proto->dhcpc_pid < 0) {
				plog(NETMAN, ERR, "start dhcpc failed");
			}
			proto->state = STATE_DIALING;
			break;
		}
		else 
			/* fall through to up proto */

	case FSM(STATE_DIALING, EVENT_DHCP_BOUND):
	case FSM(STATE_DIALING, EVENT_DHCP_RENEW):
		if (apply_addr_route(proto) < 0) {
			plog(NETMAN, ERR, "apply addr route failed, proto %s", proto->name);
		}
		proto->state = STATE_UP;
	}

	return 0;
}

