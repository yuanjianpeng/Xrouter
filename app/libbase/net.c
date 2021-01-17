
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <linux/netlink.h>
#include <linux/if_addr.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/un.h>
#include "net.h"
#include "netlink.h"
#include "error.h"

int get_prefixlen(uint32_t netmask)
{
	int i = 0;
	uint32_t _netmask = ntohl(netmask);

	for (i = 0; i < 32; i++) {
		if (_netmask & (1 << i))
			break;
	}
	return 32-i;
}

uint32_t get_netmask(int prefixlen)
{
	uint32_t subnet_mask = (1 << (32 - prefixlen)) - 1;
	return htonl(~subnet_mask);
}

uint32_t get_subnet(uint32_t ip, int prefixlen)
{
	uint32_t subnet_mask = (1 << (32 - prefixlen)) - 1;
	return htonl(ntohl(ip)&~subnet_mask);
}

uint32_t get_broadcast(uint32_t ip, int prefixlen)
{
	uint32_t subnet_mask = (1 << (32 - prefixlen)) - 1;
	return htonl(ntohl(ip)|subnet_mask);
}

int check_netmask(uint32_t netmask)
{
	int i, zero = 0;
	uint32_t _netmask = ntohl(netmask);

	// 0.0.0.0, 255.255.255.255 not valid netmask
	if (_netmask == 0 || _netmask == -1)
		return -ERR_INVARG;
	
	for (i = 31; i >= 0; i--) {
		if ( _netmask & (1 << i)) {
			if (zero)
				return -ERR_INVARG;
		}
		else 
			zero = 1;
	}

	return 0;
}

/*
 * all-zeros subnet and all-ones subnet is reserved
 * and can't be asggined to device
 * https://www.ietf.org/rfc/rfc950.txt
 * https://www.cisco.com/c/en/us/support/docs/ip/dynamic-address-allocation-resolution/13711-40.html
 *
 * all reserved ip address
 * https://en.wikipedia.org/wiki/Reserved_IP_addresses
 */
int check_subnet_ip(uint32_t ip, uint32_t netmask)
{
	uint32_t _ip = ntohl(ip);
	uint32_t _netmask = ntohl(netmask);

	uint32_t subnet = _ip & ~ _netmask;
	if (subnet == 0 || subnet == ~ _netmask)
		return -ERR_INVARG;
	return 0;
}

int set_sock_rx_timeout(int sock, int timeout)
{
	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = 1000 * (timeout % 1000);
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ?
		-ERR_SETSOCKOPT : 0;
}

static int if_up_down(const char *ifname, int up)
{
	struct ifreq ifr = {0};
	int ret = 0;
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0)
		return -ERR_SOCKET;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
		ret = -ERR_IOCTL;
		goto out;
	}
	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~ IFF_UP;
	if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
		ret = -ERR_IOCTLS;
		goto out;
	}
out:
	close2(sock);
	return ret;
}

/* return
	-1: socket() failed
	-2: ioctl() get flags failed
	-3: ioctl() set flags failed
	0: ok
 */
int if_up(const char *ifname)
{
	return if_up_down(ifname, 1);
}

int if_down(const char *ifname)
{
	return if_up_down(ifname, 0);
}

int add_br(const char *br)
{
	int err, sock;
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
		return -ERR_SOCKET;
	err = ioctl(sock, SIOCBRADDBR, br);
	close2(sock);
	return err < 0 ? -ERR_IOCTL : 0;
}

int del_br(const char *br)
{
	int err, sock;
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
		return -ERR_SOCKET;
	err = ioctl(sock, SIOCBRDELBR, br);
	close2(sock);
	return err < 0 ? -ERR_IOCTL : 0;
}

int add_brif(const char *br, const char *ifname)
{
	struct ifreq ifr = {0};
	int err, sock, ifindex;
	if ((ifindex = if_nametoindex(ifname)) == 0)
		return -ERR_IFNAM2IDX;
	if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
		return -ERR_SOCKET;
	strncpy(ifr.ifr_name, br, IFNAMSIZ-1);
	ifr.ifr_ifindex = ifindex;
	err = ioctl(sock, SIOCBRADDIF, &ifr);
	close2(sock);
	return err < 0 ? -ERR_IOCTL: 0;
}

int del_brif(const char *br, const char *ifname)
{
	struct ifreq ifr = {0};
	int err, sock, ifindex;
	if ((ifindex = if_nametoindex(ifname)) == 0)
		return -ERR_IFNAM2IDX;
	if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
		return -ERR_SOCKET;
	strncpy(ifr.ifr_name, br, IFNAMSIZ-1);
	ifr.ifr_ifindex = ifindex;
	err = ioctl(sock, SIOCBRDELIF, &ifr);
	close2(sock);
	return err < 0 ? -ERR_IOCTL : 0;
}

/* this function restore the errno no matter changed or not */
int if_in_br(const char *br, const char *ifname)
{
	char path[128];
	int save_errno = errno;
	int ret;
	sprintf(path, "/sys/class/net/%s/brif/%s", br, ifname);
	ret = access(path, F_OK);
	errno = save_errno;
	return ret ? 0 : 1;
}

/* it's ugly to pass a pre-allocated addr buffer to it.
	but for almost 99.999% condition, a 10 entry buffer will hold all address */
int get_ipv4_addr(uint32_t ifindex, struct ipv4_addr *addrs, int addr_n)
{
	char buf[2048];
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	int ret;
	int n = 0;

	ret = rtnl_dump(AF_INET, RTM_GETADDR, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, (unsigned int)ret);
			nlh = NLMSG_NEXT(nlh, ret))
	{
		struct ifaddrmsg *ifa;
		struct rtattr *rta;
		int rtasize;
		struct ipv4_addr *addr;

		if (nlh->nlmsg_type == NLMSG_DONE || nlh->nlmsg_type == NLMSG_ERROR)
			break;

		ifa = NLMSG_DATA(nlh);
		rta = (struct rtattr *)((char *)NLMSG_DATA(nlh) + NLMSG_ALIGN(sizeof(struct ifaddrmsg)));
		rtasize = NLMSG_PAYLOAD(nlh, sizeof(struct ifaddrmsg));

		if (ifindex && ifindex != ifa->ifa_index)
			continue;

		/* overflow */
		if (n >= addr_n)
			return -ERR_OVERFLOW;

		addr = &addrs[n++];
		memset(addr, 0, sizeof(*addr));

		addr->family = ifa->ifa_family;
		addr->prefixlen = ifa->ifa_prefixlen;
		addr->scope = ifa->ifa_scope;
		addr->flags = ifa->ifa_flags;

		for ( ; RTA_OK(rta, rtasize); rta = RTA_NEXT(rta, rtasize)) {
			switch (rta->rta_type) {
			case IFA_ADDRESS:
				addr->address = *(uint32_t *)RTA_DATA(rta);
				break;
			case IFA_LOCAL:
				addr->local = *(uint32_t *)RTA_DATA(rta);
				break;
			case IFA_BROADCAST:
				addr->broadcast = *(uint32_t *)RTA_DATA(rta);
				break;
			case IFA_FLAGS:
				addr->flags = *(uint32_t *)RTA_DATA(rta);
				break;
			case IFA_LABEL:
				strncpy(addr->label, RTA_DATA(rta), IFNAMSIZ-1);
				addr->label[IFNAMSIZ-1] = '\0';
				break;
			}
		}
	}

	return n;
}

/* For one request, kernel will delete all adddress match local (ignore prefixlen) 
 */
int del_ipv4_addr(uint32_t ifindex, uint32_t ipv4)
{
	char buf[512];
	int payload = 0;
	struct ifaddrmsg *ifa = NLMSG_DATA((struct nlmsghdr *)buf);

	memset(ifa, 0, sizeof(*ifa));
	ifa->ifa_family = PF_INET;
	ifa->ifa_index = ifindex;
	payload += NLMSG_ALIGN(sizeof(*ifa));

	if (ipv4)
		NLA_PUT_U32(buf, payload, IFA_LOCAL, ipv4);

	return rtnl_req(RTM_DELADDR, buf, sizeof(buf), payload);
}

/*
	note, kernel compare two if address using following keys
		same prefixlen
		same local address
		same (adress & netmask)
	see: find_matching_ifa() net/ipv4/devinet.c
 */
int add_ipv4_addr(uint32_t ifindex, uint32_t ipv4, int prefixlen, int noprefixroute)
{
	char buf[512];
	int payload = 0;
	struct ifaddrmsg *ifa = NLMSG_DATA((struct nlmsghdr *)buf);

	memset(ifa, 0, sizeof(*ifa));
	ifa->ifa_family = PF_INET;
	ifa->ifa_index = ifindex;
	ifa->ifa_scope = RT_SCOPE_UNIVERSE;
	ifa->ifa_prefixlen = prefixlen;
	payload += NLMSG_ALIGN(sizeof(*ifa));

	NLA_PUT_U32(buf, payload, IFA_LOCAL, ipv4);
	if (prefixlen < 32)
		NLA_PUT_U32(buf, payload, IFA_BROADCAST, htonl(ntohl(ipv4)|((1<<(32-prefixlen))-1)));
	if (noprefixroute)
		NLA_PUT_U32(buf, payload, IFA_FLAGS, IFA_F_NOPREFIXROUTE);

	return rtnl_req(RTM_NEWADDR, buf, sizeof(buf), payload);
}

int get_ipv4_route(struct ipv4_route *routes, int route_n)
{
	char buf[3000];
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	int ret;
	int n = 0;

	ret = rtnl_dump(AF_INET, RTM_GETROUTE, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, (unsigned int)ret);
			nlh = NLMSG_NEXT(nlh, ret))
	{
		struct rtmsg *rtm;
		struct rtattr *rta;
		int rtasize;
		struct ipv4_route *route;

		if (nlh->nlmsg_type == NLMSG_DONE || nlh->nlmsg_type == NLMSG_ERROR)
			break;

		rtm = NLMSG_DATA(nlh);
		rta = (struct rtattr *)((char *)NLMSG_DATA(nlh) + NLMSG_ALIGN(sizeof(struct rtmsg)));
		rtasize = NLMSG_PAYLOAD(nlh, sizeof(struct rtmsg));

		/* overflow */
		if (n >= route_n)
			return -ERR_OVERFLOW;

		if (rtm->rtm_table == RT_TABLE_LOCAL)
			continue;

		route = &routes[n++];
		memset(route, 0, sizeof(*route));

		route->family = rtm->rtm_family;
		route->dst_len = rtm->rtm_dst_len;
		route->src_len = rtm->rtm_src_len;
		route->tos = rtm->rtm_tos;
		route->scope = rtm->rtm_scope;
		route->protocol = rtm->rtm_protocol;
		route->type = rtm->rtm_type;
		route->flags = rtm->rtm_flags;

		for ( ; RTA_OK(rta, rtasize); rta = RTA_NEXT(rta, rtasize)) {
			switch (rta->rta_type) {
			case RTA_TABLE:
				route->table = *(uint32_t *)RTA_DATA(rta);
				break;
			case RTA_DST:
				route->dst = *(uint32_t *)RTA_DATA(rta);
				break;
			case RTA_PRIORITY:
				route->priority = *(uint32_t *)RTA_DATA(rta);
				break;
			case RTA_PREFSRC:
				route->prefsrc = *(uint32_t *)RTA_DATA(rta);
				break;
			case RTA_GATEWAY:
				route->gw = *(uint32_t *)RTA_DATA(rta);
				break;
			case RTA_OIF:
				route->oif = *(uint32_t *)RTA_DATA(rta);
				break;
			}
		}
	}

	return n;
}

int add_ipv4_route(uint32_t dst, int dst_len, uint32_t src,
		uint32_t gw, uint32_t oif, uint32_t priority)
{
	char buf[512];
	int payload = 0;
	struct rtmsg *rtm = NLMSG_DATA((struct nlmsghdr *)buf);

	memset(rtm, 0, sizeof(*rtm));
	rtm->rtm_family = PF_INET;
	rtm->rtm_dst_len = dst_len;
	rtm->rtm_table = RT_TABLE_MAIN;
	rtm->rtm_protocol = RTPROT_BOOT;
	rtm->rtm_scope = gw ? RT_SCOPE_UNIVERSE : RT_SCOPE_LINK;
	rtm->rtm_type = RTN_UNICAST;
	payload += NLMSG_ALIGN(sizeof(*rtm));

	NLA_PUT_U32(buf, payload, RTA_DST, dst);
	if (priority)
		NLA_PUT_U32(buf, payload, RTA_PRIORITY, priority);
	if (src)
		NLA_PUT_U32(buf, payload, RTA_PREFSRC, src);
	if (gw)
		NLA_PUT_U32(buf, payload, RTA_GATEWAY, gw);
	NLA_PUT_U32(buf, payload, RTA_OIF, oif);

	return rtnl_req(RTM_NEWROUTE, buf, sizeof(buf), payload);
}

/* For one request, kernel only delete the first matched route entry
	match algorithm:
	required:
		dst, dst_len, table, tos must match
	optional (no consideration for multipath route)
		match type, if provided type is not 0
		match scope, if provided scope is not RT_SCOPE_NOWHERE (255)
		match prefsrc, if provided prefsrc is not 0
		match protocol, if provided protocol is not 0
		match priority, if provided priority is not 0
		match oif, if provided oif is no 0
		match gateway, if provided gw is no 0
 */
int del_ipv4_route(uint32_t dst, int dst_len, uint32_t src,
		uint32_t gw, uint32_t oif, uint32_t priority)
{
	char buf[512];
	int payload = 0;
	struct rtmsg *rtm = NLMSG_DATA((struct nlmsghdr *)buf);

	memset(rtm, 0, sizeof(*rtm));
	rtm->rtm_family = PF_INET;
	rtm->rtm_dst_len = dst_len;
	rtm->rtm_scope = RT_SCOPE_NOWHERE;
	rtm->rtm_table = RT_TABLE_MAIN;
	payload += NLMSG_ALIGN(sizeof(*rtm));

	NLA_PUT_U32(buf, payload, RTA_DST, dst);
	if (priority)
		NLA_PUT_U32(buf, payload, RTA_PRIORITY, priority);
	if (src)
		NLA_PUT_U32(buf, payload, RTA_PREFSRC, src);
	if (gw)
		NLA_PUT_U32(buf, payload, RTA_GATEWAY, gw);
	if (oif)
		NLA_PUT_U32(buf, payload, RTA_OIF, oif);

	return rtnl_req(RTM_DELROUTE, buf, sizeof(buf), payload);
}

