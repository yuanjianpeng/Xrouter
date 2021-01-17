#include "unistd.h"
#include "switch.h"
#include "libbase/netlink.h"
#include "libbase/error.h"

static int switch_family_id = -1;

struct switch_data
{
	int set[SWITCH_ATTR_MAX];
	uint32_t val[SWITCH_ATTR_MAX];
};

static inline void set_data(struct switch_data *data, int type, uint32_t val)
{
	data->set[type] = 1;
	data->val[type] = val;
}

static int switch_req(int sock, int cmd, struct switch_data *data)
{
	char buf[256];
	int ret, payload = GENL_PAYLOAD_LEN;
	struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
	struct genlmsghdr *genlh = NLMSG_DATA(nlh);
	struct rtattr *rta;
	int rtasize;
	static volatile uint32_t _seq = 0;
	uint32_t seq = __sync_add_and_fetch(&_seq, 1);

	NLA_PUT_U32(buf, payload, SWITCH_ATTR_SEQ, seq);
	
	for (ret = SWITCH_ATTR_ADDR; ret <= SWITCH_ATTR_VAL; ret++) {
		if (data->set[ret]) {
			NLA_PUT_U32(buf, payload, ret, data->val[ret]);
		}
	}

	ret = genl_req(sock, switch_family_id, cmd, buf, sizeof(buf), payload);
	if (ret < 0)
		return ret;
	
	if (genlh->cmd != cmd)
		return -ERR_NOTMATCH;

	memset(data, 0, sizeof(*data));

	rta = (struct rtattr *)(buf + NLMSG_SPACE(GENL_PAYLOAD_LEN));
	rtasize = NLMSG_PAYLOAD((struct nlmsghdr *)buf, GENL_PAYLOAD_LEN);

	for ( ; RTA_OK(rta, rtasize); rta = RTA_NEXT(rta, rtasize)) {
		if (rta->rta_type >= SWITCH_ATTR_SEQ && rta->rta_type <= SWITCH_ATTR_VAL) {
			data->set[rta->rta_type] = 1;
			data->val[rta->rta_type] = *(uint32_t *)RTA_DATA(rta);
		}
	}

	if (!data->set[SWITCH_ATTR_SEQ])
		return -ERR_INVRES;

	if (data->val[SWITCH_ATTR_SEQ] != seq)
		return -ERR_OUTOFSEQ;

	return 0;
}

int switch_open()
{
	int sock = nl_sock_init(NETLINK_GENERIC);
	if (sock < 0)
		return -1;

	switch_family_id = genl_family_id(sock, GENL_SWITCH_NAME);
	if (switch_family_id < 0) {
		close(sock);
		return -2;
	}

	return sock;
}

void switch_close(int sock)
{
	close(sock);
}

int switch_phy_read(int sock, int phy_addr, int reg, int *val)
{
	int ret;
	struct switch_data data = { 0 };

	set_data(&data, SWITCH_ATTR_ADDR, phy_addr);
	set_data(&data, SWITCH_ATTR_REG, reg);

	ret = switch_req(sock, SWITCH_CMD_PHY_READ, &data);
	if (ret < 0)
		return ret;

	if (!data.set[SWITCH_ATTR_VAL])
		return -ERR_INVRES;
	
	*val = data.val[SWITCH_ATTR_VAL];
	return 0;
}

int switch_phy_write(int sock, int phy_addr, int reg, int val)
{
	struct switch_data data = { 0 };

	set_data(&data, SWITCH_ATTR_ADDR, phy_addr);
	set_data(&data, SWITCH_ATTR_REG, reg);
	set_data(&data, SWITCH_ATTR_VAL, val);

	return switch_req(sock, SWITCH_CMD_PHY_WRITE, &data);
}

int switch_phy_status(int sock, int phy_addr, int *link, int *speed, int *duplex)
{
	int ret;
	struct switch_data data = { 0 };
	struct phystate *state = (struct phystate *)&data.val[SWITCH_ATTR_VAL];

	set_data(&data, SWITCH_ATTR_ADDR, phy_addr);

	ret = switch_req(sock, SWITCH_CMD_PHY_STATUS, &data);
	if (ret < 0)
		return ret;

	if (!data.set[SWITCH_ATTR_VAL])
		return -ERR_INVRES;
	
	*link = state->link;
	*speed = state->speed;
	*duplex = state->duplex;
	return 0;
}

int switch_sw_read(int sock, int reg, int *val)
{
	int ret;
	struct switch_data data = { 0 };

	set_data(&data, SWITCH_ATTR_REG, reg);

	ret = switch_req(sock, SWITCH_CMD_SW_READ, &data);
	if (ret < 0)
		return ret;

	if (!data.set[SWITCH_ATTR_VAL])
		return -ERR_INVRES;
	
	*val = data.val[SWITCH_ATTR_VAL];
	return 0;
}

int switch_sw_write(int sock, int reg, int val)
{
	struct switch_data data = { 0 };

	set_data(&data, SWITCH_ATTR_REG, reg);
	set_data(&data, SWITCH_ATTR_VAL, val);

	return switch_req(sock, SWITCH_CMD_SW_WRITE, &data);
}

