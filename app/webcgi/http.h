#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/* these two fds are opened by http server
	they are predefined
 */
#define POST_FD		STDIN_FILENO
#define OUT_FD		STDOUT_FILENO

/* max post fileds */
#define MAX_POST_F	16

struct post_data
{
	char *name;
	char *value;
	char *filename;
	int offset;
	int data_len;
};

enum method {
	GET = 1, POST
};

enum enctype {
	MULTIPART = 1, URLENCODED,
};

/* The request variable is keeped on stack
	don't add big array to this struct */
struct http_req
{
	char *cgi_name;
	const char *prog;

	int request_method;

	char *content_length;
	char *content_type;
	enum enctype enctype;

	int post_data_fd;
	int post_data_n;
	struct post_data post_data[MAX_POST_F];

	int response_fd;
};

struct http_res
{
	int code;
	char buf[1024];
	char *data;
	int data_len;
	int responsed;	/* response has been sent at cgi cb */
	int graceful_shutdown;
};

struct post_data * get_post_data(struct http_req *req, const char *name);

static inline char *get_post(struct http_req *req, const char *name)
{
	struct post_data *f = get_post_data(req, name);
	return f ? f->value : NULL;
}

int http_parse_post_data(struct http_req *req);
int http_response(struct http_req *req, struct http_res *res);

int append_printf(char *buf, int len, int bufsize, const char *fmt, ...);

#define S(name)	"\"" name "\": \"%s\""
#define D(name)	"\"" name "\": %d"
#define U(name)	"\"" name "\": %u"
#define LLU(name)	"\"" name "\": %llu"
#define O(name)	"\"" name "\": {"
#define A(name)	"\"" name "\": ["

#define PUSH(buf, len, bufsiz, fmt, ...) append_printf(buf, len, bufsiz, fmt, ##__VA_ARGS__)
#define P(fmt, ...) len = append_printf(buf, len, bufsiz, fmt, ##__VA_ARGS__)

#endif

