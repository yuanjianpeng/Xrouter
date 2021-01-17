#include "../webcgi.h"
#include "config/config.h"

#define CGI_PATH "/cgi-bin/network/"

static int check_mode(const char *mode)
{
	if (strcmp(mode, "router") && strcmp(mode, "bridge"))
		return 1;
	return 0;
}

static int check_proto(const char *proto)
{
	if (strcmp(proto, "none") && strcmp(proto, "static")
			&& strcmp(proto, "dhcp") && strcmp(proto, "pppoe"))
		return 1;
	return 0;
}

static int get_mode(struct http_req *req, struct http_res *res)
{
	char mode[64];
	struct config_context ctx;

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;

	config_get_cpy(&ctx, "network", "mode", mode, sizeof(mode));

	config_end(&ctx);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" O("network") S("mode") "}}", mode);
	res->code = 200;
	res->data = res->buf;

	return 0;
}
REG_CGI(get_mode);

static int set_mode(struct http_req *req, struct http_res *res)
{
	int ret;
	struct config_context ctx;
	char *mode = get_post(req, "mode");

	if (!mode || check_mode(mode)) {
		res->code = 400;
		return 0;
	}
	
	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;

	ret = config_set(&ctx, "network", "mode", mode);

	config_end(&ctx);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("error") "}", 0);
	res->code = 200;
	res->data = res->buf;

	if (ret == 1) {
		res->graceful_shutdown = 1;
		http_response(req, res);
		system("/etc/init.d/network reload");
	}
	
	return 0;
}
REG_CGI(set_mode);

static int get_wan(struct http_req *req, struct http_res *res)
{
	const char *proto, *ip, *netmask, *gateway, *dns1, *dns2, \
		*user, *password, *nat, *mtu, *mtu_pppoe;
	struct config_context ctx;
	char *buf = res->buf;
	int len = 0, bufsiz = sizeof(res->buf);

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;
	
	proto = config_get(&ctx, "wan", "proto");
	ip = config_get(&ctx, "wan", "ip");
	netmask = config_get(&ctx, "wan", "netmask");
	gateway = config_get(&ctx, "wan", "gateway");
	user = config_get(&ctx, "wan", "user");
	password = config_get(&ctx, "wan", "password");
	dns1 = config_get(&ctx, "wan", "dns1");
	dns2 = config_get(&ctx, "wan", "dns1");
	nat = config_get(&ctx, "wan", "nat");
	mtu = config_get(&ctx, "wan", "mtu");
	mtu_pppoe = config_get(&ctx, "wan", "mtu_pppoe");

	/* use these char point before config_end()
		they point to shared memeory */
	P("{" O("wan"));
	P(S("proto"), proto ? : ""); 
	P(", " S("ip"), ip ? : ""); 
	P(", " S("netmask"), netmask ? : ""); 
	P(", " S("gateway"), gateway ? : ""); 
	P(", " S("user"), user ? : ""); 
	P(", " S("password"), password ? : ""); 
	P(", " S("dns1"), dns1 ? : ""); 
	P(", " S("dns2"), dns2 ? : ""); 
	P(", " S("nat"), nat ? : ""); 
	P(", " S("mtu"), mtu ? : ""); 
	P(", " S("mtu_pppoe"), mtu_pppoe ? : ""); 
	P("}}");

	config_end(&ctx);

	res->data_len = len;
	res->code = 200;
	res->data = buf;
	
	return 0;
}
REG_CGI(get_wan);

static int set_wan(struct http_req *req, struct http_res *res)
{
	int ret = 0;
	struct config_context ctx;
	char *proto, *ip, *netmask, *gateway;
	char *dns1, *dns2, *user, *password, *nat, *mtu, *mtu_pppoe;

	proto = get_post(req, "proto");
	ip = get_post(req, "ip");
	netmask = get_post(req, "netmask");
	gateway = get_post(req, "gateway");
	dns1 = get_post(req, "dns1");
	dns2 = get_post(req, "dns2");
	user = get_post(req, "user");
	password = get_post(req, "password");
	nat = get_post(req, "nat");
	mtu = get_post(req, "mtu");
	mtu_pppoe = get_post(req, "mtu_pppoe");
	
	if (!proto || check_proto(proto)) {
		res->code = 400;
		return 0;
	}

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;

	ret |= config_set(&ctx, "wan", "proto", proto);
	ret |= config_set(&ctx, "wan", "ip", ip);
	ret |= config_set(&ctx, "wan", "netmask", netmask);
	ret |= config_set(&ctx, "wan", "gateway", gateway);
	ret |= config_set(&ctx, "wan", "dns1", dns1);
	ret |= config_set(&ctx, "wan", "dns2", dns2);
	ret |= config_set(&ctx, "wan", "user", user);
	ret |= config_set(&ctx, "wan", "password", password);
	ret |= config_set(&ctx, "wan", "nat", nat);
	ret |= config_set(&ctx, "wan", "mtu", mtu);
	ret |= config_set(&ctx, "wan", "mtu_pppoe", mtu_pppoe);

	config_end(&ctx);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("error") "}", 0);
	res->code = 200;
	res->data = res->buf;

	if (ret & 1) {
		res->graceful_shutdown = 1;
		http_response(req, res);
		system("/etc/init.d/network reload");
	}
	
	return 0;
}
REG_CGI(set_wan);

static int get_lan(struct http_req *req, struct http_res *res)
{
	char ip[32], netmask[32];
	struct config_context ctx;

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;
	
	config_get_cpy(&ctx, "lan", "ip", ip, sizeof(ip));
	config_get_cpy(&ctx, "lan", "netmask", netmask, sizeof(netmask));

	config_end(&ctx);

	res->data = res->buf;
	res->data_len = PUSH(res->buf, 0, sizeof(res->buf),
			"{" O("lan") S("ip") ", " S("netmask") "}}", ip, netmask);
	res->code = 200;
	
	return 0;
}
REG_CGI(get_lan);

static int set_lan(struct http_req *req, struct http_res *res)
{
	int ret = 0;
	struct config_context ctx;
	char *ip = get_post(req, "ip");
	char *netmask = get_post(req, "netmask");

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;

	ret |= config_set(&ctx, "lan", "ip", ip);
	ret |= config_set(&ctx, "lan", "netmask", netmask);

	config_end(&ctx);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("error") "}", 0);
	res->code = 200;
	res->data = res->buf;

	if (ret == 1) {
		res->graceful_shutdown = 1;
		http_response(req, res);
		system("/etc/init.d/network reload");
	}
	
	return 0;
}
REG_CGI(set_lan);

static int get_dhcp(struct http_req *req, struct http_res *res)
{
	char dhcp_server[32], dhcp_range[64];
	struct config_context ctx;

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;
	
	config_get_cpy(&ctx, "lan", "dhcp-server", dhcp_server, sizeof(dhcp_server));
	config_get_cpy(&ctx, "lan", "dhcp-range", dhcp_range, sizeof(dhcp_range));

	config_end(&ctx);

	res->data = res->buf;
	res->data_len = PUSH(res->buf, 0, sizeof(res->buf),
			"{" O("lan") S("dhcp_server") ", " S("dhcp_range") "}}", dhcp_server, dhcp_range);
	res->code = 200;
	
	return 0;
}
REG_CGI(get_dhcp);

static int set_dhcp(struct http_req *req, struct http_res *res)
{
	int ret = 0;
	struct config_context ctx;
	char *dhcp_server = get_post(req, "dhcp_server");
	char *dhcp_range = get_post(req, "dhcp_range");

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;

	ret |= config_set(&ctx, "lan", "dhcp-server", dhcp_server);
	ret |= config_set(&ctx, "lan", "dhcp-range", dhcp_range);

	config_end(&ctx);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("error") "}", 0);
	res->code = 200;
	res->data = res->buf;

	if (ret == 1) {
		res->graceful_shutdown = 1;
		http_response(req, res);
		system("/etc/init.d/network reload");
	}
	
	return 0;
}
REG_CGI(set_dhcp);

/* ------------------ */

static const char *get_vap_sec(const char *radio)
{
	if (radio == NULL)
		return NULL;

	if (!strcmp(radio, "2g"))
		return "wifi_2g";

	else if (!strcmp(radio, "5g"))
		return "wifi_5g";

	return NULL;
}

static int get_wifi(struct http_req *req, struct http_res *res)
{
	struct config_context ctx;
	const char *radio, *en, *ssid, *enc, *psk, *bw, *ch, *txpower;
	const char *sec;
	char *buf = res->buf;
	int len = 0, bufsiz = sizeof(res->buf);
	
	radio = get_post(req, "radio");
	sec = get_vap_sec(radio);
	if (!sec)
		return 1;

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;
	
	en = config_get(&ctx, sec, "enable");
	ssid = config_get(&ctx, sec, "ssid");
	enc = config_get(&ctx, sec, "encrypt");
	psk = config_get(&ctx, sec, "psk");
	bw = config_get(&ctx, sec, "bandwidth");
	ch = config_get(&ctx, sec, "channel");
	txpower = config_get(&ctx, sec, "txpower");

	P("{ \"%s\": {", radio);
	P(S("enable"), en ? : ""); 
	P(", " S("ssid"), ssid ? : ""); 
	P(", " S("encrypt"), enc ? : ""); 
	P(", " S("psk"), psk ? : ""); 
	P(", " S("bandwidth"), bw ? : ""); 
	P(", " S("channel"), ch ? : ""); 
	P(", " S("txpower"), txpower ? : ""); 
	P("}}");

	config_end(&ctx);

	res->data_len = len;
	res->code = 200;
	res->data = buf;

	return 0;
}
REG_CGI(get_wifi);

static int set_wifi(struct http_req *req, struct http_res *res)
{
	struct config_context ctx;
	const char *radio, *en, *ssid, *enc, *psk, *bw, *ch, *txpower;
	const char *sec;
	int ret = 0;

	radio = get_post(req, "radio");
	en = get_post(req, "enable");
	ssid = get_post(req, "ssid");
	enc = get_post(req, "encrypt");
	psk = get_post(req, "psk");
	bw = get_post(req, "bandwidth");
	ch = get_post(req, "channel");
	txpower = get_post(req, "txpower");

	sec = get_vap_sec(radio);
	if (!sec)
		return 1;

	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;
	
	ret |= config_set(&ctx, sec, "enable", en);
	ret |= config_set(&ctx, sec, "ssid", ssid);
	ret |= config_set(&ctx, sec, "encrypt", enc);
	ret |= config_set(&ctx, sec, "psk", psk);
	ret |= config_set(&ctx, sec, "bandwidth", bw);
	ret |= config_set(&ctx, sec, "channel", ch);
	ret |= config_set(&ctx, sec, "txpower", txpower);

	config_end(&ctx);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("error") "}", 0);
	res->code = 200;
	res->data = res->buf;

	if (ret & 1) {
		res->graceful_shutdown = 1;
		http_response(req, res);
		system("/etc/init.d/network reload");
	}
	
	return 0;
}
REG_CGI(set_wifi);

