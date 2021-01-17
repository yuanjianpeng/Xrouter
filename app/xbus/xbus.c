#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "libbase/net.h"
#include "xbus.h"

static int _xbus_bind(struct sockaddr_un *addr, const char *module, int cli)
{
	int sock;

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		return -1;

	memset(addr, 0, sizeof(*addr));
	addr->sun_family = AF_UNIX;
	if (cli)
		snprintf(addr->sun_path, sizeof(addr->sun_path)-1, XBUS_CLIENT_DIR "/%s.%d", module, getpid());
	else
		snprintf(addr->sun_path, sizeof(addr->sun_path)-1, XBUS_DIR "/%s", module);

	unlink(addr->sun_path);
	if (bind(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
		close(sock);
		return -2;
	}
	
	return sock;
}

/* server create a unix domain socket, and bind to module */
int xbus_bind(const char *module)
{
	struct sockaddr_un addr;
	return _xbus_bind(&addr, module, 0);
}

/* client send a request and recv response */
int xbus_request(const char *module, int timeout, char *buf, int bufsiz, int buflen)
{
	struct sockaddr_un addr;
	struct sockaddr_un cli_addr;
	int sock;
	int ret;

	sock = _xbus_bind(&cli_addr, module, 1);
	if (sock < 0)
		return sock;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	snprintf(addr.sun_path, sizeof(addr.sun_path), XBUS_DIR "/%s", module);

send_again:
	ret = sendto(sock, buf, buflen, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (ret <= 0) {
		/* if server is not bind: Connection refused 
		   if module socket file no exits: No such file or directory
		 */
		if (ret < 0 && errno == EINTR)
			goto send_again;
		ret = -3;
		goto out;
	}

	if (timeout && set_sock_rx_timeout(sock, timeout))
		return -4;

recv_again:
	ret = recv(sock, buf, bufsiz, 0);
	if (ret <= 0) {
		if (ret < 0 && errno == EINTR)
			goto recv_again;
		ret = -5;
	}

out:
	close(sock);
	unlink(cli_addr.sun_path);
	return ret;
}

/* server recv a request and reply a response */
int xbus_response(int sock, char *buf, int bufsiz, xbus_req_cb cb, void *priv)
{
	struct sockaddr_un addr;
	socklen_t socklen = sizeof(addr);
	int ret, buflen;

recv_again:
	ret = recvfrom(sock, buf, bufsiz-1, 0, (struct sockaddr *)&addr, &socklen);
	if (ret <= 0) {
		if (ret < 0 && errno == EINTR)
			goto recv_again;
		return -1;
	}

	buf[ret] = '\0';	/* make buf a null-terminated string */
	buflen = cb(buf, bufsiz, ret, priv);
	if (buflen <= 0)
		return 0;
	
send_again:
	ret = sendto(sock, buf, buflen, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (ret <= 0) {
		if (ret < 0 && errno == EINTR)
			goto send_again;
		return -2;
	}

	return 0;
}

int xbus_parse_req(char *buf, char **args, int max_arg)
{
	char *s = buf;
	char *e;
	int arg_n = 0;

	do {
		e = strchr(s, '\n');
		if (arg_n >= max_arg)
			return -1;
		args[arg_n++] = s;
		*e = '\0';
		s = e + 1;
	} while (e && *s);

	return arg_n;
}

