
#include "xbus.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int test(char *buf, int bufsiz, int buflen, void * priv)
{
	time_t t;
	struct tm *tmp;

	printf("request: %s\n", buf);
	sleep(10);

	t = time(NULL);
	tmp = localtime(&t);
	return	strftime(buf, bufsiz, "%a, %d %b %Y %T %z\n", tmp);
}

int main(int artc, char *argv)
{
	int ret;
	char buf[1024];
	int sock = xbus_bind("test");

	if (sock < 0)
		return 1;

	while (1) {
		memset(buf, 0, sizeof(buf));
		ret = xbus_response(sock, buf, sizeof(buf), test, NULL);	
		if (ret < 0) {
			printf("xbus response failed: ret %d, err %s\n", ret, strerror(errno));
			break;
		}
	}
	return 0;
}
