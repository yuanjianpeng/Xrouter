/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/rtnetlink.h>
#include <linux/genetlink.h>
#include "netlink.h"
#include "error.h"

int nl_sock_init(int proto)
{
	struct sockaddr_nl addr;
	int nlsock ;

	if ((nlsock = socket(AF_NETLINK, SOCK_RAW, proto)) < 0)
		return -ERR_SOCKET;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;

	if (bind(nlsock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close2(nlsock);
		return -ERR_BIND;
	}

	return nlsock;
}

int nl_recv(int sock, char *buf, int bufsiz)
{
	struct msghdr msg;
	struct iovec iov;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = (void *)buf;
	iov.iov_len = bufsiz;

	ret = recvmsg(sock, &msg, 0);
	if (ret <= 0)
		return -ERR_RECVMSG;
	
	if (msg.msg_flags & MSG_TRUNC)
		return -ERR_MSGTRUNC;
	
	return ret;
}

int rtnl_dump(int family, uint32_t msg_type, char *buf, int bufsiz)
{
	int sock, ret;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct iovec iov;
	struct msghdr msg;

	if ((sock = nl_sock_init(NETLINK_ROUTE)) < 0)
		return -ERR_SOCKET;

	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
	nlh->nlmsg_type = msg_type;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;
	((struct rtgenmsg *)NLMSG_DATA(nlh))->rtgen_family = family;

	ret = write(sock, nlh, NLMSG_LENGTH(sizeof(struct rtgenmsg)));
	if (ret != NLMSG_LENGTH(sizeof(struct rtgenmsg))) {
		close2(sock);
		return ret < 0 ? -ERR_WRITE : -ERR_PARTIAL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = (void *)buf;
	iov.iov_len = bufsiz;

	/* Actually we should receive multiple times
		util a NLMSG_DONE msg is received */
	ret = recvmsg(sock, &msg, 0);
	close2(sock);

	if (ret < 0)
		return -ERR_RECVMSG;

	if (msg.msg_flags & MSG_TRUNC)
		return -ERR_MSGTRUNC;

	return ret;
}

int rtnl_req(int msg_type, char *buf, int bufsiz, int payload_len)
{
	int sock, ret;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct iovec iov;
	struct msghdr msg;
	struct nlmsgerr *errmsg;

	if ((sock = nl_sock_init(NETLINK_ROUTE)) < 0)
		return -ERR_SOCKET;

	nlh->nlmsg_len = NLMSG_LENGTH(payload_len);
	nlh->nlmsg_type = msg_type;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;

	/* this guy require NLM_F_CREATE flag */
	if (msg_type == RTM_NEWROUTE)
		nlh->nlmsg_flags |= NLM_F_CREATE;

	ret = write(sock, buf, nlh->nlmsg_len);
	if (ret != nlh->nlmsg_len) {
		close2(sock);
		return ret < 0 ? ERR_WRITE : ERR_PARTIAL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = (void *)buf;
	iov.iov_len = bufsiz;

	ret = recvmsg(sock, &msg, 0);
	close2(sock);

	if (ret < 0)
		return -ERR_RECVMSG;

	if (msg.msg_flags & MSG_TRUNC)
		return -ERR_MSGTRUNC;

	if (!NLMSG_OK(nlh, (unsigned int)ret) || nlh->nlmsg_type != NLMSG_ERROR
		|| nlh->nlmsg_len - NLMSG_HDRLEN < sizeof(*errmsg))
		return -ERR_INVRES;
	
	errmsg = NLMSG_DATA(nlh);
	if (errmsg->error) {
		errno = -errmsg->error;
		return -ERR_ERRMSG;
	}

	return ret;
}

int genl_req(int sock, int id, int cmd, char *buf, int bufsiz, int payload_len)
{
	int ext_sock = 1, ret;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct genlmsghdr *genlh = NLMSG_DATA(nlh);
	struct iovec iov;
	struct msghdr msg;

	if (sock == -1) {
		if ((sock = nl_sock_init(NETLINK_GENERIC)) < 0)
			return -ERR_SOCKET;
		ext_sock = 0;
	}

	nlh->nlmsg_len = NLMSG_LENGTH(payload_len);
	nlh->nlmsg_type = id;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;

	genlh->cmd = cmd;
	genlh->version = 0;
	genlh->reserved = 0;

	ret = write(sock, buf, nlh->nlmsg_len);
	if (ret != nlh->nlmsg_len) {
		if (!ext_sock)
			close2(sock);
		return ret < 0 ? ERR_WRITE : ERR_PARTIAL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = (void *)buf;
	iov.iov_len = bufsiz;

	ret = recvmsg(sock, &msg, 0);
	if (!ext_sock)
		close2(sock);

	if (ret < 0)
		return -ERR_RECVMSG;

	if (msg.msg_flags & MSG_TRUNC)
		return -ERR_MSGTRUNC;

	if (!NLMSG_OK(nlh, (unsigned int)ret))
		return -ERR_INVRES;

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *errmsg = NLMSG_DATA(nlh);
		errno = -errmsg->error;
		return -ERR_ERRMSG;
	} 

	return ret;
}

int genl_family_id(int sock, const char *family_name)
{
	char buf[1024];
	int ret, payload = GENL_PAYLOAD_LEN;
	struct rtattr *rta;
	int rtasize;

	NLA_PUT_STR(buf, payload, CTRL_ATTR_FAMILY_NAME, family_name);

	ret = genl_req(sock, GENL_ID_CTRL, CTRL_CMD_GETFAMILY, buf, sizeof(buf), payload);
	if (ret < 0)
		return ret;

	rta = (struct rtattr *)(buf + NLMSG_SPACE(GENL_PAYLOAD_LEN));
	rtasize = NLMSG_PAYLOAD((struct nlmsghdr *)buf, GENL_PAYLOAD_LEN);

	for ( ; RTA_OK(rta, rtasize); rta = RTA_NEXT(rta, rtasize)) {
		switch(rta->rta_type) {
		case CTRL_ATTR_FAMILY_ID:
			return *(uint16_t *)RTA_DATA(rta);
		}
	}

	return -ERR_NOATTR;
}

