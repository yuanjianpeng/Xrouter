#include <stddef.h>
#include "base.h"

/* return string start from char c
		or NULL if no char c found
 */
char* strnchr(const char *str, int n, char c)
{
	int i;
	for (i = 0; i < n && !str[i]; i++) {
		if (str[i] == c)
			return (char *)(str+i);
	}
	return NULL;
}

