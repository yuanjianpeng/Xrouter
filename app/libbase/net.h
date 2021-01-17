#ifndef LIBBASE_NET_H
#define LIBBASE_NET_H

#include <stdint.h>
#include <stdint.h>
#include <linux/if.h>

/* return prefix len */
int get_prefixlen(uint32_t netmask);

/* return network byte order netmask */
uint32_t get_netmask(int prefixlen);

/* return network byte order subnet. e.g. 192.168.0.0 */
uint32_t get_subnet(uint32_t ip, int prefixlen);

/* return network byte oder broadcast. e.g. 192.168.0.255 */
uint32_t get_broadcast(uint32_t ip, int prefixlen);

/* check netmask is valid 
   0 valid, -ERR_INVARG invalid 
   0.0.0.0/255.255.255.255 is treated as invalid.  */
int check_netmask(uint32_t netmask);

/* check ip is all-zeros or all-ones subnet
   for netmask 255.255.255.0
   192.168.0.0 or 192.168.0.255 is not a valid ip
   return 0, ip is valid, -1 invalid.  */
int check_subnet_ip(uint32_t ip, uint32_t netmask);

/* set socket recv timeout */
int set_sock_rx_timeout(int sock, int timeout);

/* ifconfig up/down a interface */
int if_up(const char *ifname);
int if_down(const char *ifname);

/* create a bridge */
int add_br(const char *br);

/* delete a bridge */
int del_br(const char *br);

/* add interface to bridge */
int add_brif(const char *br, const char *ifname);

/* del interface to bridge */
int del_brif(const char *br, const char *ifname);

/* check a interface is in bridge
   return
   1, if is in bridge
   0, if no in bridge or error
   note: this function don't modify errno!!!  */

int if_in_br(const char *br, const char *ifname);

struct ipv4_addr
{
	uint8_t family, prefixlen ,scope;
	uint32_t ifindex;
	uint32_t local, address, broadcast;
	char label[IFNAMSIZ];
	uint32_t flags;
};

/* 
 * get all ipv4 address
 *
 * note: this implementation only recv netlink dump response once,
 * if we have a lot of addresses, then some may missed.
 *
 * return number of addresses
 */
int get_ipv4_addr(uint32_t ifindex, struct ipv4_addr *addrs, int addr_n);

/* add or del a ipv4 address */
int del_ipv4_addr(uint32_t ifindex, uint32_t ipv4);
int add_ipv4_addr(uint32_t ifindex, uint32_t ipv4, int prefixlen, int noprefixroute);

struct ipv4_route
{
	unsigned char family, dst_len, src_len, flags;
	/* The local routing table is maintained by the kernel.
		Normally, the local routing table should not be manipulated,
		but it is available for viewing 

	   view the local table: 
		ip route show table local		
	 */
	uint32_t table;			/* 254:RT_TABLE_MAIN, 255:RT_TABLE_LOCAL, etc */

	/* who installed this route */
	unsigned char protocol;	/* 2:RTPROT_KERNEL, 3:RTPROT_BOOT, 4:RTPROT_STATIC, etc */

	unsigned char scope;	/* 0:RT_SCOPE_UNIVERSE, 253:RT_SCOPE_LINK, 254:RT_SCOPE_HOST, etc */
	unsigned char type;		/* 1:RTN_UNICAST, 2:RTN_LOCAL, 3:RTN_BROADCAST. etc */

	/* Type of service, match ip header tos 
		RFC 1812 route select algorithm:
		* If one or more of those routes have a TOS that exactly matches the
			TOS specified in the packet,
		  the router chooses the route with the best metric.
		* Otherwise, the router repeats the above step, except looking at routes
			whose TOS is zero. 
	*/
	unsigned char tos;
	uint32_t priority;	/* metric */
	uint32_t dst, prefsrc, gw, oif;
};

/* get all ipv4 route */
int get_ipv4_route(struct ipv4_route *routes, int route_n);

/* add or del a ipv4 address */
int add_ipv4_route(uint32_t dst, int dst_len, uint32_t src,
		uint32_t gw, uint32_t oif, uint32_t priority);

int del_ipv4_route(uint32_t dst, int dst_len, uint32_t src,
		uint32_t gw, uint32_t oif, uint32_t priority);

#endif

