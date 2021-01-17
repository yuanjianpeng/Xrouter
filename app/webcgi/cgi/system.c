#include "../webcgi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libbase/system.h"
#include "switch/libswitch.h"
#include "config/config.h"

#define CGI_PATH "/cgi-bin/system/"

int get_meminfo(struct http_req *req, struct http_res *res)
{
	unsigned long total = 0, free = 0;
	r_meminfo(&total, &free);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("total") "," U("free") "}", total, free);
	res->code = 200;
	res->data = res->buf;
	return 0;
}
REG_CGI(get_meminfo);

int get_cpustat(struct http_req *req, struct http_res *res)
{
	int i, n, len = 0, bufsiz = sizeof(res->buf);
	char *buf = res->buf;
	struct cpu_time time[8+1];
	n = r_cpustat(time, 8);
	
	P("{");
	P(D("n"), n);

	for (i = 0; i < n; i++) {
		if (i == 0)
			P(", " O("cpu"));
		else
			P(", " O("cpu%d"), i-1);
		P(LLU("user"), time[i].user);
		P(", " LLU("nice"), time[i].nice);
		P(", " LLU("system"), time[i].system);
		P(", " LLU("idle"), time[i].idle);
		P(", " LLU("iowait"), time[i].iowait);
		P(", " LLU("irq"), time[i].irq);
		P(", " LLU("softirq"), time[i].softirq);
		P("}");
	}
	P("}");

	res->code = 200;
	res->data = res->buf;
	res->data_len = len;
	return 0;
}
REG_CGI(get_cpustat);

int get_uptime(struct http_req *req, struct http_res *res)
{
	unsigned long sec = 0;
	r_uptime(&sec);

	res->data_len = PUSH(res->buf, 0, sizeof(res->buf), "{" U("uptime") "}", sec);
	res->code = 200;
	res->data = res->buf;
	return 0;
}
REG_CGI(get_uptime);

static int get_phy_status(struct http_req *req, struct http_res *res)
{
	uint32_t port_mask, phy_base, wan_port;
	int i, n = 0;
	char *buf = res->buf;
	int bufsiz = sizeof(res->buf);
	int len = 0;
	int link, speed, duplex;
	int sock;

	struct config_context ctx;
	if (config_start(&ctx, CFG_START_WITH_LOCK) < 0)
		return 1;
	port_mask = config_get_u32(&ctx, "switch", "port_mask");
	phy_base = config_get_u32(&ctx, "switch", "phy_base");
	wan_port = config_get_u32(&ctx, "switch", "wan_port");
	config_end(&ctx);

	if (port_mask == -1 || phy_base == -1 || wan_port == -1)
		return 1;

	if ((sock = switch_open()) < 0)
		return 1;

	P("{" A("status"));

	for (i = 0; i < 32; i++) {
		if (0 == (port_mask & (1 << i)))
			continue;
		if (switch_phy_status(sock, phy_base + i, &link, &speed, &duplex))
			continue;
		if (n)
			P(", ");
		P("{");
		P(S("type"), i == wan_port ? "wan" : "lan");
		P(", " D("port_id"), i);
		P(", " D("link"), link);
		if (link) {
			P(", " D("speed"), speed);
			P(", " D("duplex"), duplex);
		}
		P("}");
		n++;
	}

	switch_close(sock);

	P("]}");
	res->code = 200;
	res->data = res->buf;
	res->data_len = len;
	return 0;
}
REG_CGI(get_phy_status);

