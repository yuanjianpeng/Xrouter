#ifndef WEBCGI_H
#define WEBCGI_H

#include <stdint.h>
#include "http.h"

#define CGI_FLAG_PERM		(1<<0)

typedef int (* cgicb_t)(struct http_req *req, struct http_res *res);

/* note: because we look up cgi entry based on array align (8 bytes in 64bits system)
	but the variable may be put in section with different align (big struct is aligned to 16 bytes)
	so, here we force the align to 16 bytes !!!
 */
typedef struct cgi
{
	const char *cgi_name;
	cgicb_t fn;
	uint32_t flags;
} __attribute__ ((aligned (16))) cgi_t;

#define REGISTER_CGI(name, cb, flag) \
	static cgi_t __cgicall##cb \
		__attribute__((__section__("cgicalls"))) __attribute__((used)) \
		= { \
			.cgi_name = name, \
			.fn = cb, \
			.flags = flag, \
		} 

#define REG_CGI(cb) REGISTER_CGI(CGI_PATH #cb, cb, CGI_FLAG_PERM)

#endif

