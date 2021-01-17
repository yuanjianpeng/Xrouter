#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/phy.h>
#include <net/genetlink.h>

#include "switch.h"

extern int switch_reg_read(int reg, u32 *val);
extern int switch_reg_write(int reg, u32 val);

static char *bus;
static struct mii_bus *mii_bus;
static struct genl_family switch_fam;

struct reply_data
{
	int seq, val;
};

static int read_phy_status(int addr, struct reply_data *data)
{
	int ctl, status, lpa, adv, ctl1000, stat1000;
	int link, autoneg, autoneg_done, speed, duplex;
	struct phystate *state = (struct phystate *)&data->val;

	mutex_lock(&mii_bus->mdio_lock);

	ctl = mii_bus->read(mii_bus, addr, MII_BMCR);
	status = mii_bus->read(mii_bus, addr, MII_BMSR);

	autoneg = (ctl & BMCR_ANENABLE) ? 1 : 0;
	link = (status & BMSR_LSTATUS) ? 1: 0;
	autoneg_done = (status & BMSR_ANEGCOMPLETE) ? 1 : 0;

	if (autoneg) {
		if (!autoneg_done)
			link = 0;
		if (link) {
			speed = SPEED_10;
			duplex = DUPLEX_HALF;
			stat1000 = mii_bus->read(mii_bus, addr, MII_STAT1000);
			ctl1000 = mii_bus->read(mii_bus, addr, MII_CTRL1000);
			if ((stat1000 & ctl1000 << 2) & (LPA_1000FULL | LPA_1000HALF)) {
				speed = SPEED_1000;
				if ((stat1000 & ctl1000 << 2) & LPA_1000FULL)
					duplex = DUPLEX_FULL;
			}
			else {
				lpa = mii_bus->read(mii_bus, addr, MII_LPA);
				adv = mii_bus->read(mii_bus, addr, MII_ADVERTISE);
				if (lpa & adv & (LPA_100FULL | LPA_100HALF)) {
					speed = SPEED_100;
					if (lpa & adv & LPA_100FULL)
						duplex = DUPLEX_FULL;
				}
				else
					if (lpa & adv & LPA_10FULL)
						duplex = DUPLEX_FULL;
			}
		}
	}
	else if (link) {
		duplex = (ctl & BMCR_FULLDPLX) ? 1 : 0;
		if (ctl & BMCR_SPEED1000)
			speed = SPEED_1000;
		else if (ctl & BMCR_SPEED100)
			speed = SPEED_100;
		else
			speed = SPEED_10;
	}

	data->val = 0;
	state->link = link;
	if (link) {
		state->duplex = duplex;
		state->speed = speed;
	}

	mutex_unlock(&mii_bus->mdio_lock);
	return 0;
}

static int switch_reply(struct genl_info *info, int cmd, struct reply_data *data)
{
	void *hdr;
	struct sk_buff *msg;
	
	msg = nlmsg_new(512, GFP_KERNEL);
	if (msg == NULL)
		return -ENOMEM;

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq, &switch_fam, 0, cmd);
	if (IS_ERR(hdr))
		return -1;

	nla_put_u32(msg, SWITCH_ATTR_SEQ, data->seq);

	switch (cmd) {
	case SWITCH_CMD_PHY_READ:
	case SWITCH_CMD_SW_READ:
	case SWITCH_CMD_PHY_STATUS:
		nla_put_u32(msg, SWITCH_ATTR_VAL, data->val);
		break;
	}

	genlmsg_end(msg, hdr);
	return genlmsg_reply(msg, info);
}

static int switch_cmd_phy_read(struct sk_buff *skb, struct genl_info *info)
{
	struct reply_data data;
	u32 addr, reg;

	if (mii_bus == NULL)
		return -ENODEV;

	if (!info->attrs[SWITCH_ATTR_SEQ] || !info->attrs[SWITCH_ATTR_ADDR]
			|| !info->attrs[SWITCH_ATTR_REG])
		return -EINVAL;
	
	data.seq = nla_get_u32(info->attrs[SWITCH_ATTR_SEQ]);
	addr = nla_get_u32(info->attrs[SWITCH_ATTR_ADDR]);
	reg = nla_get_u32(info->attrs[SWITCH_ATTR_REG]);

	mutex_lock(&mii_bus->mdio_lock);
	data.val = mii_bus->read(mii_bus, addr, reg);
	mutex_unlock(&mii_bus->mdio_lock);

	return switch_reply(info, SWITCH_CMD_PHY_READ, &data);
}

static int switch_cmd_phy_write(struct sk_buff *skb, struct genl_info *info)
{
	struct reply_data data;
	u32 addr, reg, val;

	if (mii_bus == NULL)
		return -ENODEV;

	if (!info->attrs[SWITCH_ATTR_SEQ] || !info->attrs[SWITCH_ATTR_ADDR]
			|| !info->attrs[SWITCH_ATTR_REG]
			|| !info->attrs[SWITCH_ATTR_VAL])
		return -EINVAL;
	
	data.seq = nla_get_u32(info->attrs[SWITCH_ATTR_SEQ]);
	addr = nla_get_u32(info->attrs[SWITCH_ATTR_ADDR]);
	reg = nla_get_u32(info->attrs[SWITCH_ATTR_REG]);
	val = nla_get_u32(info->attrs[SWITCH_ATTR_VAL]);

	mutex_lock(&mii_bus->mdio_lock);
	mii_bus->write(mii_bus, addr, reg, val);
	mutex_unlock(&mii_bus->mdio_lock);

	return switch_reply(info, SWITCH_CMD_PHY_WRITE, &data);
}

static int switch_cmd_phy_status(struct sk_buff *skb, struct genl_info *info)
{
	struct reply_data data;
	u32 addr;

	if (mii_bus == NULL)
		return -ENODEV;

	if (!info->attrs[SWITCH_ATTR_SEQ] || !info->attrs[SWITCH_ATTR_ADDR])
		return -EINVAL;
	
	data.seq = nla_get_u32(info->attrs[SWITCH_ATTR_SEQ]);
	addr = nla_get_u32(info->attrs[SWITCH_ATTR_ADDR]);

	if (read_phy_status(addr, &data) < 0)
		return -ENOMEDIUM;

	return switch_reply(info, SWITCH_CMD_PHY_STATUS, &data);
}

static int switch_cmd_sw_read(struct sk_buff *skb, struct genl_info *info)
{
	struct reply_data data;
	u32 reg;

	if (!info->attrs[SWITCH_ATTR_SEQ] || !info->attrs[SWITCH_ATTR_REG])
		return -EINVAL;
	
	data.seq = nla_get_u32(info->attrs[SWITCH_ATTR_SEQ]);
	reg = nla_get_u32(info->attrs[SWITCH_ATTR_REG]);

	if (switch_reg_read(reg, &data.val))
		return -EINVAL;

	return switch_reply(info, SWITCH_CMD_SW_READ, &data);
}

static int switch_cmd_sw_write(struct sk_buff *skb, struct genl_info *info)
{
	struct reply_data data;
	u32 reg, val;

	if (!info->attrs[SWITCH_ATTR_SEQ] || !info->attrs[SWITCH_ATTR_REG]
			|| !info->attrs[SWITCH_ATTR_VAL])
		return -EINVAL;
	
	data.seq = nla_get_u32(info->attrs[SWITCH_ATTR_SEQ]);
	reg = nla_get_u32(info->attrs[SWITCH_ATTR_REG]);
	val = nla_get_u32(info->attrs[SWITCH_ATTR_VAL]);

	if (switch_reg_write(reg, val))
		return -EINVAL;

	return switch_reply(info, SWITCH_CMD_SW_WRITE, &data);
}

static const struct nla_policy switch_policy_phy_read[SWITCH_ATTR_MAX+1] = {
	[SWITCH_ATTR_SEQ] = { .type = NLA_U32 },
	[SWITCH_ATTR_ADDR] = { .type = NLA_U32 },
	[SWITCH_ATTR_REG] = { .type = NLA_U32 },
};

static const struct nla_policy switch_policy_phy_write[SWITCH_ATTR_MAX+1] = {
	[SWITCH_ATTR_SEQ] = { .type = NLA_U32 },
	[SWITCH_ATTR_ADDR] = { .type = NLA_U32 },
	[SWITCH_ATTR_REG] = { .type = NLA_U32 },
	[SWITCH_ATTR_VAL] = { .type = NLA_U32 },
};

static const struct nla_policy switch_policy_phy_status[SWITCH_ATTR_MAX+1] = {
	[SWITCH_ATTR_SEQ] = { .type = NLA_U32 },
	[SWITCH_ATTR_ADDR] = { .type = NLA_U32 },
};

static const struct nla_policy switch_policy_sw_read[SWITCH_ATTR_MAX+1] = {
	[SWITCH_ATTR_SEQ] = { .type = NLA_U32 },
	[SWITCH_ATTR_REG] = { .type = NLA_U32 },
};

static const struct nla_policy switch_policy_sw_write[SWITCH_ATTR_MAX+1] = {
	[SWITCH_ATTR_SEQ] = { .type = NLA_U32 },
	[SWITCH_ATTR_REG] = { .type = NLA_U32 },
	[SWITCH_ATTR_VAL] = { .type = NLA_U32 },
};

static struct genl_ops switch_ops[] = {
	{
		.cmd = SWITCH_CMD_PHY_READ,
		.doit = switch_cmd_phy_read,
		.policy = switch_policy_phy_read,
	},
	{
		.cmd = SWITCH_CMD_PHY_WRITE,
		.doit = switch_cmd_phy_write,
		.policy = switch_policy_phy_write,
	},
	{
		.cmd = SWITCH_CMD_PHY_STATUS,
		.doit = switch_cmd_phy_status,
		.policy = switch_policy_phy_status,
	},
	{
		.cmd = SWITCH_CMD_SW_READ,
		.doit = switch_cmd_sw_read,
		.policy = switch_policy_sw_read,
	},
	{
		.cmd = SWITCH_CMD_SW_WRITE,
		.doit = switch_cmd_sw_write,
		.policy = switch_policy_sw_write,
	},
};

static struct genl_family switch_fam = {
	.name = GENL_SWITCH_NAME,
	.hdrsize = 0,
	.version = 0,
	.maxattr = SWITCH_ATTR_MAX,
	.module = THIS_MODULE,
	.ops = switch_ops,
	.n_ops = ARRAY_SIZE(switch_ops),
};

static int match_mii_bus(struct device *dev, void *data)
{
	struct mdio_device *mdio_dev = to_mdio_device(dev);

	if (!data || !strcmp(mdio_dev->bus->name, data)) {
		printk("switch: use mii bus %s\n", mdio_dev->bus->name);
		mii_bus = mdio_dev->bus;
		return 1;
	}

	return 0;
}

static int __init switch_init(void)
{
	int ret = bus_for_each_dev(&mdio_bus_type, NULL, bus, match_mii_bus);
	if (ret <= 0)
		return ret ? : -ENODEV;
	
	{
		int i = 0;
		struct reply_data data;
		for (i = 0; i < 5; i++)
			read_phy_status(i, &data);
	}

	return genl_register_family(&switch_fam);
}

static void __exit switch_exit(void)
{
	genl_unregister_family(&switch_fam);
}

module_init(switch_init);
module_exit(switch_exit);

module_param(bus, charp, 0644);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jianpeng Yuan");
MODULE_DESCRIPTION("switch, phy register r/w module");

