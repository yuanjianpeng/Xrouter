#ifndef SWITCH_H
#define SWITCH_H

#define GENL_SWITCH_NAME	"xswitch"

/* Command */
enum {
	SWITCH_CMD_PHY_READ,
	SWITCH_CMD_PHY_WRITE,
	SWITCH_CMD_PHY_STATUS,
	SWITCH_CMD_SW_READ,
	SWITCH_CMD_SW_WRITE,
};

/* Attributes */
enum {
	SWITCH_ATTR_UNSPEC,		/* kernel ignore attr id 0 when parse policy */
	SWITCH_ATTR_SEQ,
	SWITCH_ATTR_ADDR,
	SWITCH_ATTR_REG,
	SWITCH_ATTR_VAL,
	SWITCH_ATTR_MAX,
};

struct phystate
{
	unsigned link : 1;
	unsigned duplex : 1;
	unsigned speed: 10;
};

#endif

