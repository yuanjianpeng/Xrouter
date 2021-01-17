#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include "http.h"
#include "libbase/filesystem.h"
#include "libbase/base.h"

int append_printf(char *buf, int len, int bufsize, const char *fmt, ...)
{
	int ret;
	va_list ap;
	if (len < 0 || len >= bufsize)
		return -1;
	va_start(ap, fmt);
	ret = vsnprintf(buf + len, bufsize - len, fmt, ap); 	
	va_end(ap);
	if (ret <= 0 || ret >= bufsize - len)
		return -1;
	return len + ret;
}

struct post_data * get_post_data(struct http_req *req, const char *name)
{
	int i;
	for (i = 0; i < req->post_data_n; i++) {
		if (!strcmp(req->post_data[i].name, name))
			return &req->post_data[i];
	}
	return NULL;
}

#define XDIGIT_VALUE(c) (c>='a'?c-'a'+10:(c>='A'?c-'A'+10:c-'0'))

static char * urldecode_string(const char *str, int n)
{
	int i = 0, j = 0;
	char *s = malloc(n+1);
	if (s == NULL)
		return s;
	while (i < n) {
		if (str[i] == '%' && i + 2 < n
			&& isxdigit(str[i+1])
			&& isxdigit(str[i+2])) {
			s[j++] = ((XDIGIT_VALUE(str[i+1]) << 4) | XDIGIT_VALUE(str[i+2]));
			i += 3;
			continue;
		}
		/* rfc1866 8.2.1 */
		else if (str[i] == '+') {
			s[j++] = ' ';
			i++;
		}
		else
			s[j++] = str[i++];
	}
	s[j] = '\0';
	return s;
}

static int parse_urlencoded(const char *post, int size, struct post_data *f, int n)
{
	const char *name = post, *value = NULL;
	int num = 0;
	char *s;
	const char *end = post + size;

	while (post < end && num < n) {
		s = strnchr(post, end-post, '=');
		if (s == NULL)
			return -1;
		value = s+1;
		s = strchr(value, '&');
		if (s == NULL) {
			f[num].value = urldecode_string(value, end-value);
		}
		else {
			f[num].value = urldecode_string(value, s-value);
		}
		f[num].name = urldecode_string(name, value - name -1);
		if (f[num].name == NULL || f[num].value == NULL)
			return -1;
		num++;
		if (s == NULL)
			break;
		name = post = s + 1;
	}
	return num;
}

/*
	A http multi-part form post data example:

------WebKitFormBoundaryGGr38spJ3fbcMmBq
Content-Disposition: form-data; name="name"

yuan
------WebKitFormBoundaryGGr38spJ3fbcMmBq
Content-Disposition: form-data; name="file"; filename="printarg"
Content-Type: application/octet-stream

1111111
------WebKitFormBoundaryGGr38spJ3fbcMmBq--
*/

static char *get_prop(char *st, int len, char *name)
{
	char c;
	int key = -1, key_end = -1, value = -1, value_end = -1;
	int i = 0;

	while (i < len) {
		c = st[i];
		if (c == ' ' || c == '\t') {
		}
		else if ( c == '=') {
			if (key != -1 && value == -1 && key_end == -1)
				key_end = i;
		}
		else if ( c == '\\') {
			if ( i + 1 < len && st[i+1] == '"') {
				i+=2;
				continue;
			}
		}
		else if ( c == '"') {
			if (key == -1 || key_end == -1) {
				return NULL;
			}
			if (value == -1)
				value = i + 1;
			else
				value_end = i;
		}
		else if ( c == ';') {
		}
		else {
			if (key == -1)
				key = i;
		}

		if (value_end != -1) {
			if (key == -1 || key_end == -1 || value == -1)
				return NULL;
			if (key_end - key == strlen(name)
				&& memcmp(st+key, name, key_end-key) == 0) {
				return urldecode_string(&st[value], value_end - value);
			}
			key = key_end = value = value_end = -1;
		}

		i++;
	}
	return NULL;
}

static inline int parse_fields(void *req_start, char *st, char *en, struct post_data *f)
{
	char *tail;
	char *data = NULL;
	int len = 0;
	int binary = 0;

	memset(f, 0, sizeof(*f));

	while (st < en) {
		tail = memmem(st, en - st, "\r\n", 2);
		if (tail == NULL) {
			goto leave;
		}
		if (tail == st) {
			data  = tail + 2;
			break;
		}

#define HHCD	"Content-Disposition"
		if (memmem(st, tail-st, HHCD, sizeof(HHCD)-1) == st) {
			st = memchr(st, ';', tail-st);
			if (st == NULL)
				goto leave;
			st += 1;
			f->name = get_prop(st, tail-st, "name");
			f->filename = get_prop(st, tail-st, "filename");
		}

#define HHCT	"Content-Type"
		else if (memmem(st, tail-st, HHCT, sizeof(HHCT)-1) == st) {
			binary = 1;
		}

		st = tail + 2;
	}

	if (data == NULL || f->name == NULL)
		goto leave;

	len = en - data;
	if (len < 2 || data[len - 2] != '\r' || data[len - 1] != '\n')
		goto leave;

	len -= 2;
	f->offset = data - (char *)req_start;
	f->data_len = len;
	if (binary == 0) {
		f->value = malloc(f->data_len + 1);
		if (f->value == NULL)
			goto leave;
		memcpy(f->value, data, f->data_len);
		f->value[f->data_len] = '\0';
	}

	return 0;

leave:
	if (f->name)
		free(f->name);
	if (f->filename)
		free(f->filename);
	return 1;
}

static inline void * check_boundary(void *req, int req_len, void* boundary, int b_len)
{
	char *b;
	char e1, e2;

	b = memmem(req, req_len, boundary, b_len);
	if (b == NULL)
		return NULL;

	/* check \r\n after boundary */
	if (b + b_len + 2 > (char *)req + req_len)
		return NULL;

	e1 = b[b_len];
	e2 = b[b_len+1];

	if ((e1 == '\r' && e2 == '\n') ||
		(e1 == '-' && e2 == '-')) {
		return b;
	}

	return NULL;
}

int parse_multipart(void *req, int req_len, void* boundary, int b_len, struct post_data *fields, int n)
{
	int parsed = 0;
	int first = 1;
	char *en;
	void *req_start = req;

	while (req_len > 0 && parsed < n) {
		en = check_boundary(req, req_len, boundary, b_len);
		if (en == NULL)
			break;
		if (first) {
			first = 0;
		}
		else {
			if (parse_fields(req_start, req, en, fields + parsed)) {
				break;
			}
			parsed++;
		}

		req_len -= en - (char *)req + b_len + 2;
		req = en + b_len + 2;
	}

	return parsed;
}

static int parse_enctype(struct http_req *req)
{
	if (req->content_type == NULL)
		return 1;

	if (strstr(req->content_type, "application/x-www-form-urlencoded"))
		req->enctype = URLENCODED;
	else if (strstr(req->content_type, "multipart/form-data"))
		req->enctype = MULTIPART;
	else
		return 1;

	return 0;
}

static char *get_boundary(char *ct, int *length)
{
	char *b, *c;

	if (ct == NULL)
		return NULL;

	b = strstr(ct, "boundary=");
	if (b == NULL)
		return NULL;

	b += sizeof("boundary=")-1;
	c = strchr(b, ';');  /* there are after props after boundary */
	if (c == NULL)
		*length = strlen(b);
	else
		*length = c - b;

	// the real boundary has two dashes '--' before boundary
	c = malloc(3 + *length);
	if (c != NULL) {
		c[0] = '-';
		c[1] = '-';
		memcpy(c+2, b, *length);
		c[2+*length] = '\0';
	}
	(*length) += 2;

	return c;
}

int http_parse_post_data(struct http_req *req)
{
	char *post;
	off_t size;
	int num;
	char *boundary = NULL;
	int blen;
	int ret = 1;

	if (parse_enctype(req))
		return 1;

	size = lseek(req->post_data_fd, 0, SEEK_END);
	if (size == (off_t)-1)
		return 1;

	if (size == 0)
		return 0;

	post = mmap(NULL, size, PROT_READ, MAP_SHARED, req->post_data_fd, 0);	
	if (post == MAP_FAILED)
		return 1;

	if (req->enctype == MULTIPART) {
		boundary = get_boundary(req->content_type, &blen);
		if (boundary) {
			num = parse_multipart(post, size, boundary, blen,
					req->post_data, MAX_POST_F);
			if (num >= 0) {
				req->post_data_n = num;
				ret = 0;
			}
		}
	}
	else if (req->enctype == URLENCODED) {
		num = parse_urlencoded(post, size, req->post_data, MAX_POST_F);
		if (num >= 0) {
			req->post_data_n = num;
			ret = 0;
		}
	}

	munmap(post, size);
	if (boundary)
		free(boundary);
	return ret;
}

static const char * http_status(int code)
{
	switch (code) {
	case 200:
		return "OK";
	case 400:
		return "Bad Request";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 500:
		return "Internal Server Error";
	}
	return "";
}

int http_response(struct http_req *req, struct http_res *res)
{
	char *res_header_template =
		"HTTP/1.1 %d %s\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: %d\r\n"
		"\r\n";
	char *body_template = "{\"error\"=%d,\"desc\"=\"%s\"}";
	char header[512];
	int header_len;
	char body_buf[128];
	char *body;
	int body_len;

	if (res->data == NULL) {
		body_len = snprintf(body_buf, sizeof(body_buf),
			body_template, res->code, http_status(res->code));
		if (body_len >= sizeof(body_buf))
			return 1;
		body = body_buf;
	}
	else {
		body = res->data;
		body_len = res->data_len;
	}

	header_len = snprintf(header, sizeof(header), res_header_template,
			res->code, http_status(res->code), body_len);
	if (header_len >= sizeof(header))
		return 1;

	safe_write(req->response_fd, -1, header, header_len);
	safe_write(req->response_fd, -1, body, body_len);

	res->responsed = 1;

	if (res->graceful_shutdown)
		close(req->response_fd);

	return 0;
}

