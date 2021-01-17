#ifndef LIBBASE_NETLINK_H
#define LIBBASE_NETLINK_H

#include <stdint.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/genetlink.h>

#define NLA_PUT_U32(buf, payload, type, data) \
	do { \
		struct nlattr *nla = (struct nlattr *)(buf + NLMSG_SPACE(payload)); \
		nla->nla_type = type; \
		nla->nla_len = RTA_LENGTH(4); \
		*(uint32_t *)RTA_DATA(nla) = data; \
		payload = NLMSG_ALIGN(payload) + RTA_LENGTH(4); \
	} while (0)

#define NLA_PUT_STR(buf, payload, type, str) \
	do { \
		struct nlattr *nla = (struct nlattr *)(buf + NLMSG_SPACE(payload)); \
		nla->nla_type = type; \
		nla->nla_len = RTA_LENGTH(strlen(str)+1); \
		strcpy(RTA_DATA(nla), str); \
		payload = NLMSG_ALIGN(payload) + RTA_LENGTH(strlen(str)+1); \
	} while (0)

#define GENL_PAYLOAD_LEN sizeof(struct genlmsghdr)

int nl_sock_init(int proto);
int nl_recv(int sock, char *buf, int bufsiz);

int rtnl_dump(int family, uint32_t msg_type, char *buf, int bufsiz);
int rtnl_req(int msg_type, char *buf, int bufsiz, int payload_len);

int genl_req(int sock, int id, int cmd, char *buf, int bufsiz, int payload_len);
int genl_family_id(int sock, const char *family_name);

#endif

