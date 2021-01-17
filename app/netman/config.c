#include <string.h>
#include <arpa/inet.h>
#include "log/log.h"
#include "config/config.h"
#include "libbase/net.h"
#include "netman.h"

static int parse_proto(const char *proto)
{
	if (!strcmp(proto, "none"))
		return PROTO_NONE;
	else if (!strcmp(proto, "static"))
		return PROTO_STATIC;
	else if (!strcmp(proto, "dhcp"))
		return PROTO_DHCP;
	else if (!strcmp(proto, "pppoe"))
		return PROTO_PPPOE;
	return -1;
}

static int parse_ifname(const char *val, char (*ifname)[IFNAMSIZ], int n)
{
	int i = 0;
	char *e;
	const char *v = val;

	while (1) {
		e = strchr(v, ' ');
		if (!e) {
			if (strlen(v) >= IFNAMSIZ)
				return -1;
			strcpy(ifname[i], v);
			break;
		}
		if (e == v || e - v >= IFNAMSIZ)
			return -1;
		memcpy(ifname[i], v, e - v);
		ifname[i][e-v] = '\0';
		if (++i >= n)
			return -1;
		v = e + 1;
	}
	return 0;
}

static int read_proto(struct proto *proto, const char *sec, struct config_context *ctx)
{
	const char *val;

	/* proto */
	if ((val = config_get(ctx, sec, "proto")) == NULL) {
		plog(NETMAN, ERR, "no proto");
		return -1;
	}

	proto->proto = parse_proto(val);
	if (proto->proto == -1) {
		plog(NETMAN, ERR, "invalid proto %s", proto);
		return -1;
	}

	if (proto->proto == PROTO_STATIC)
	{
		/* netmask */
		if ((val = config_get(ctx, sec, "netmask")) == NULL) {
			plog(NETMAN, ERR, "no netmask");
			return -1;
		}
		proto->netmask = (uint32_t)inet_addr(val);
		if (check_netmask(proto->netmask) < 0) {
			plog(NETMAN, ERR, "invalid netmask: %s", val);
			return -1;
		}

		/* ip */
		if ((val = config_get(ctx, sec, "ip")) == NULL) {
			plog(NETMAN, ERR, "no ip");
			return -1;
		}
		proto->ip = (uint32_t)inet_addr(val);
		if (check_subnet_ip(proto->ip, proto->netmask) < 0) {
			plog(NETMAN, ERR, "invalid ip %s netmask %08x", val, proto->netmask);
			return -1;
		}

		/* optioanl: gateway */
		val = config_get(ctx, sec, "gateway");
		if (val)
			proto->gateway = (uint32_t)inet_addr(val);
	}

	/* ifnames */
	val = config_get(ctx, sec, "ifname");
	if (val && parse_ifname(val, proto->ifname, sizeof(proto->ifname)/sizeof(proto->ifname[0]))) {
		plog(NETMAN, ERR, "invalid ifname %s", val);
		return -1;
	}

	return 0;
}

int read_config(struct netman_context *ctx)
{
	struct config_context cfg;
	int ret = -1;

	if (config_start(&cfg, 0) < 0) {
		plog(NETMAN, ERR, "config start failed");
		return -1;
	}

	if (read_proto(&ctx->lan, "lan", &cfg) < 0) {
		plog(NETMAN, ERR, "read lan config failed");
		goto err;
	}

	if (read_proto(&ctx->wan, "wan", &cfg) < 0) {
		plog(NETMAN, ERR, "read wan config failed");
		goto err;
	}

	ret = 0;

err:
	config_end(&cfg);
	return ret;
}

