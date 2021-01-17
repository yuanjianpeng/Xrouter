#ifndef _CONFIG_H
#define _CONFIG_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CONF_OK		0
#define CONF_ERR	-1
#define CONF_NOP	1

struct opt_head
{
	unsigned short len;		/* 0: option end */
#define OPT_FLAG_SEC	1
#define OPT_FLAG_DEF	2
	unsigned char flag;
	char key[0];
};

struct shm_config_head
{
#define SHM_FLAG_ERASED		1
	unsigned int flag;
};

struct config_context
{
	/* protected only one process write it */
	int lockfd;

	/* config stored in shared memory, shared by all process */
	int shmfd;
#define SHM_CONF_DATA(c) ((char *)c + sizeof(struct shm_config_head))
#define SHM_CONF_DSIZE(n) (n - sizeof(struct shm_config_head))
	int shmsiz;
	char *conf;

	int changed;
	int err;
};

#define CFG_START_WITH_LOCK			1
#define CFG_START_WITH_CREATE_SHM	2
#define CFG_START_WITH_RM_SHM		4

/*
 * hold a file lock then load shared memory
 * return 0 ok, -1 failed 
 */
int config_start(struct config_context *ctx, int flag);

/* 
 * save config in shared memory to mtd partition
 * do nothing if erase flag is set
 * return 0 ok, -1 failed
 */
int config_save(struct config_context *ctx);

/* save config if changed, detach shared memory, relase the lock
 */
void config_end(struct config_context *ctx);

/*
 * erase mtd config partition, set erase flag to shared memory head to prevent
 * later save
 * return 0 ok, -1 failed
 */
int config_erase(struct config_context *ctx);

/* merge default config and mtd config to shared memory
 * return 0 ok, -1 failed
 */
int config_load(struct config_context *ctx);

/* read a config
 * return a pointer to the value string in shared memory
 * modify the returned value is not allowed, that may crash the config
 * return NULL if no config or error
 */
const char *config_get(struct config_context *ctx, const char *sec, const char *key);

/* set or delete (if val is null or empty string) a config
 * return -2: invalid argument
	0: not changed
	1: changed
	2: buffer overflow
 */
int config_set(struct config_context *ctx, const char *sec, const char *key, const char *val);

/* show all configs (if sec is null) or section configs */
void config_dump(struct config_context *ctx, const char *sec);

/* return u32 of config entry, 
	if convert config value string to u32 failed, return -1
 */
static inline
uint32_t config_get_u32(struct config_context *ctx, const char *sec, const char *key)
{
	const char *d;
	char *endptr;
	uint32_t val;

	if ((d = config_get(ctx, sec, key)) == NULL)
		return -1;

	val = strtoul(d, &endptr, 0);
	if (errno != 0 || endptr == d)
		return -1;

	return val;
}

static inline
char *config_get_dup(struct config_context *ctx, const char *sec, const char *key)
{
	const char *d = config_get(ctx, sec, key);
	if (d)
		return strdup(d);
	return NULL;
}

static inline
void config_get_cpy(struct config_context *ctx, const char *sec, const char *key, char *v, int n)
{
	const char *d = config_get(ctx, sec, key);
	if (d) {
		strncpy(v, d, n-1);
		v[n-1] = '\0';
	}
	else
		v[0] = '\0';
}


#endif

