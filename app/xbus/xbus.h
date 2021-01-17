#ifndef XBUS_H
#define XBUS_H

#define XBUS_TIMEOUT	5000
#define XBUS_DIR	"/run/xbus"
#define XBUS_CLIENT_DIR "/run/xbus/cli"
#define XBUS_BUF_SIZE	4094

#define XBUS_MOD_NETMAN	"netman"

typedef int (* xbus_req_cb)(char *buf, int bufsiz, int buflen, void *priv); 

/* return scoket fd or
	-1: socket() failed
	-2: bind() failed
 */
int xbus_bind(const char *module);

/* return response data len or
	-1: socket() failed
	-2: bind() failed
	-3: sendto() failed
	-4: set time out failed
	-5: recv() failed
 */
int xbus_request(const char *module, int timeout_ms, char *buf, int bufsiz, int buflen);

/* return 0 if success or
	-1: recvfrom() failed
	-2: sendto() failed
 */
int xbus_response(int sock, char *buf, int bufsiz, xbus_req_cb cb, void *priv);

/* return -1, if args can't hold all arg */
int xbus_parse_req(char *buf, char **args, int max_arg);

#endif

