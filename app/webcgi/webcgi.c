 /* 
  * Xrouter web cgi
  * Yuan Jianpeng 2019/7/1
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "webcgi.h"
#include "libbase/filesystem.h"
#include "libbase/base.h"

extern cgi_t __start_cgicalls, __stop_cgicalls;

cgi_t * lookup_cgi(const char *name) {
	cgi_t *cgi_entry;
	for (cgi_entry = &__start_cgicalls;
		cgi_entry < &__stop_cgicalls; cgi_entry++) {
		if (!strcmp(name, cgi_entry->cgi_name))
			return cgi_entry;
	}
	return NULL;
}

static char *get_cgi_name()
{
	char *dot;
	char *cgi_name = getenv("REQUEST_URI");
	if (cgi_name == NULL)
		return NULL;
	cgi_name = strdup(cgi_name);
	if (cgi_name == NULL)
		return NULL;
	dot = strchr(cgi_name, '.');
	if (dot)
		*dot = '\0';
	return cgi_name;
}

static int cgi_init_req(struct http_req *req)
{
	char *req_method = getenv("REQUEST_METHOD");

	req->post_data_fd = POST_FD;
	req->response_fd = OUT_FD;
	req->content_type = getenv("CONTENT_TYPE");
	req->content_length = getenv("CONTENT_LENGTH");
	
	if (req_method == NULL)
		return 1;
	else if (!strcmp(req_method, "GET"))
		req->request_method = GET;
	else if (!strcmp(req_method, "POST"))
		req->request_method = POST;
	else
		return 1;

	if (req->request_method == POST && http_parse_post_data(req))
		return 1;
	
	req->cgi_name = get_cgi_name();
	if (req->cgi_name == NULL)
		return 1;

	return 0;
}

static int list_cgi()
{
	cgi_t *cgi_entry;
	for (cgi_entry = &__start_cgicalls;
		cgi_entry < &__stop_cgicalls; cgi_entry++)
		printf("%s\n", cgi_entry->cgi_name);
	return 0;
}

static int gen_cgi(char *dir)
{
	char path[250];
	cgi_t *cgi_entry;
	for (cgi_entry = &__start_cgicalls;
		cgi_entry < &__stop_cgicalls; cgi_entry++) {
		int n = snprintf(path, sizeof(path), "%s%s.cgi", dir, cgi_entry->cgi_name);
		if (n >= sizeof(path)) {
			fprintf(stderr, "webcgi: gen %s path overflow\n", cgi_entry->cgi_name);
			return 1;
		}
		mkdir_l(path, 0755);
		if (symlink("/usr/bin/webcgi", path) < 0) {
			fprintf(stderr, "webcgi: symlink %s failed: %s\n", path, strerror(errno));
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct http_req req = {0};
	struct http_res res = {0};
	cgi_t *cgi;

	if (argc == 2 && !strcmp(argv[1], "list"))
		return list_cgi();
	else if (argc == 3 && !strcmp(argv[1], "gen"))
		return gen_cgi(argv[2]);

	/* Child should not inherit the response fd */
	set_cloexec_flag(OUT_FD);
	set_cloexec_flag(POST_FD);

	if (cgi_init_req(&req))
		return 1;	/* fatal error, no response to client */

	cgi = lookup_cgi(req.cgi_name);
	if (cgi == NULL)
		res.code = 404;
	else if (cgi->fn(&req, &res))
		res.code = 500;

	if (!res.responsed)
		return http_response(&req, &res);
	
	return 0;
}

