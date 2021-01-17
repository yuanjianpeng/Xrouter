/*
 * config librarry
 * 2020/2/2 shenzhen 
 * Yuan Jianpeng <yuanjp89@163.com>
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "log/log.h"
#include "libbase/mtd.h"
#include "config.h"

#define CONFIG_DEFAULT_PATH		"/etc/default.config"
#define CONFIG_LOCK_PATH		"/run/config/lock"

#define CONFIG_SHM_SIZE			0x20000

#define CONFIG_PARTITION	"config"
#define CONFIG_OFFSET		0
#define CONFIG_SIZE			0x10000

struct mtd_config_head
{
#define MTD_CONFIG_MAGIC	"XCFG"
	uint8_t magic[4];
#define MTD_CONFIG_VER	0
	uint32_t version;
	uint32_t length;
	uint8_t digest[16];		// use md5 as digest algorithm
};

/*
	> lan
	# commemt
	proto static
	ip 192.168.0.1
	mask 255.255.255.0

return:
	negative: error
	0: parse a empty line or a comment line
	1: parse a section
	2: parse an option
 */

#define SKIP_SPACE(d,l,p) \
	while (p < l && (d[p] == ' ' || d[p] == '\t')) \
		p++

#define GOTO_END(d,l,p) \
	while (p < l && d[p] != '\n' && d[p] != '\r') \
		p++

#define GOTO_SPACE(d,l,p) \
	while (p < l && (d[p] != ' ' && d[p] != '\t' && d[p] != '\n' && d[p] != '\r')) \
		p++

static int _config_parse(char *d, int l, int *p, int *kv)
{
	int _p = *p;
	int ret = 0;

	SKIP_SPACE(d,l,_p);

	if (_p >= l)
		goto out;

	switch (d[_p])
	{
	/* comment line */
	case '#':
		GOTO_END(d,l,_p);
		break;
	/* empty line */
	case '\r':
	case '\n':
		break;
	/* section */
	case '>':
		_p++;
		SKIP_SPACE(d,l,_p);
		kv[0] = _p;
		GOTO_SPACE(d,l,_p);
		kv[1] = _p - kv[0];
		if (kv[1] == 0) {
			ret = -1;
			break;
		}
		GOTO_END(d,l,_p);
		ret = 1;
		break;
	/* option */
	default:
		kv[0] = _p;
		GOTO_SPACE(d,l,_p);
		kv[1] = _p - kv[0];
		SKIP_SPACE(d,l,_p);
		kv[2] = _p;
		GOTO_END(d,l,_p);
		kv[3] = _p - kv[2];
		if (kv[2] == kv[0] || kv[1] == 0 || kv[3] == 0)
			ret = -1;
		else
			ret = 2;
	}

out:
	if (_p < l && (d[_p] == '\n' || d[_p] == '\r'))
		_p++;
	*p = _p;
	return ret;
}

#define NEXT_OPT(opt) ((struct opt_head *)((char *)opt + opt->len))
#define OPT_VAL(opt) (opt->key + strlen(opt->key) + 1)

static int strmemcmp(const char *str, const char *mem, int memlen)
{
	if (strlen(str) != memlen)
		return 1;
	return strncmp(str, mem, memlen);
}

/*
	read return:
		0: not found
		1: found, result stored in val
	
	write return:
		0: not changed
		1: changed
		2: buffer overflow
 */
static int _config_op(char *conf, int confsiz, int rd,
		const char *sec, int seclen,
		const char *key, int keylen,
		void *val, int vallen, int def)
{
	char *end = conf + confsiz;
	char *data_end = NULL;
	struct opt_head *opt = (struct opt_head *)conf;
	struct opt_head *matched_sec = NULL, *matched_opt = NULL, *next_sec = NULL, *next_opt = NULL;
	int new_sec_len = 0, new_len;

	/* find the key */
	while (opt->len) {
		if (opt->flag & OPT_FLAG_SEC) {
			if (matched_sec) {
				next_sec = opt;
				break;
			}
			else if (!strmemcmp(opt->key, sec, seclen))
				matched_sec = opt;
		}
		else if (matched_sec && !strmemcmp(opt->key, key, keylen)) {
			matched_opt = opt;
			next_opt = NEXT_OPT(matched_opt);
			break;
		}
		opt = NEXT_OPT(opt);
	}

	/* read */
	if (rd) {
		if (!matched_opt)
			return 0;
		*(char **)val = OPT_VAL(matched_opt);
		return 1;
	}

	/* goto the end, so that we can get the last opt pos */
	while (opt->len)
		opt = NEXT_OPT(opt);

	data_end = (char *)opt + sizeof(opt->len);

	if (!vallen) {		/* delete the option */
		if (!matched_opt)
			return 0;
		/* delete the section if last option is deleted */
		if (NEXT_OPT(matched_sec) == matched_opt && (next_opt->len == 0 || next_opt->flag & OPT_FLAG_SEC))
			memmove(matched_sec, next_opt, data_end - (char *)next_opt);
		else
			memmove(matched_opt, next_opt, data_end - (char *)next_opt);
		return 1;
	}

	new_len = sizeof(struct opt_head) + keylen + vallen + 2;
	if (new_len % 2)	/* align to 2 cuz opt->len is short type */
		new_len += 1;
	
	/* change */
	if (matched_opt) {
		if (data_end + new_len - matched_opt->len > end) {
			/* buffer overflow */
			return 2;
		}
		/* not changed */
		if (!strmemcmp(OPT_VAL(matched_opt), val, vallen))
			return 0;
		if (new_len != matched_opt->len) {
			memmove((char *)matched_opt + new_len, next_opt, data_end - (char *)next_opt);
			matched_opt->len = new_len;
		}
		if (def)
			matched_opt->flag |= OPT_FLAG_DEF;
		else
			matched_opt->flag &= ~OPT_FLAG_DEF;
		memcpy(OPT_VAL(matched_opt), val, vallen);
		OPT_VAL(matched_opt)[vallen] = '\0';
		return 1;
	}

	/* add ----> */
	if (!matched_sec) { // add new section
		new_sec_len = sizeof(struct opt_head) + seclen + 1;
		if (new_sec_len % 2)
			new_sec_len += 1;
	} 

	if (data_end + new_len + new_sec_len > end)
		return 2;
	
	if (!next_sec)
		next_sec = opt;

	memmove((char *)next_sec + new_len + new_sec_len, next_sec, data_end - (char *)next_sec);

	if (!matched_sec) {
		next_sec->len = new_sec_len;
		next_sec->flag = OPT_FLAG_SEC;
		memcpy(next_sec->key, sec, seclen);
		(next_sec->key)[seclen] = '\0';
		next_sec = NEXT_OPT(next_sec);
	}

	next_sec->len = new_len;
	next_sec->flag = def ? OPT_FLAG_DEF : 0;
	memcpy(next_sec->key, key, keylen);
	(next_sec->key)[keylen] = '\0';
	memcpy(OPT_VAL(next_sec), val, vallen);
	OPT_VAL(next_sec)[vallen] = '\0';
	return 1;
}

/* return data len written to buf
	-1: if buff overflow
 */
static int _config_save(char *conf, int confsiz, char *buf, int bufsiz)
{
	int len = 0, ret;
	int opt_num = 0, sec_len = 0;
	struct opt_head *opt = (struct opt_head *)conf;
	for ( ; opt->len ; opt = NEXT_OPT(opt)) {
		if (opt->flag & OPT_FLAG_SEC) {
			if (opt_num == 0)
				len -= sec_len;
			ret = snprintf(buf + len, bufsiz - len, "> %s\n", opt->key);
			opt_num = 0;
			sec_len = ret;
		}
		else if (!(opt->flag & OPT_FLAG_DEF)) {
			ret = snprintf(buf + len, bufsiz - len, "%s %s\n", opt->key, OPT_VAL(opt));
			opt_num++;
		}
		else
			continue;

		if (ret <= 0 || ret >= bufsiz - len)
			return -1;
		len += ret;
	}
	if (opt_num == 0)
		len -= sec_len;
	return len;
}

static void _config_dump(char *conf, int confsiz, const char *sec)
{
	struct opt_head *opt = (struct opt_head *)conf;
	int sec_found = 0;
	while (opt->len) {
		if (opt->flag & OPT_FLAG_SEC) {
			if (!sec || !strcmp(sec, opt->key)) {
				sec_found = 1;
				printf("> %s\n", opt->key);
			} else if (sec && sec_found)
				break;
		}
		else if (sec_found) {
			printf(" %c%s %s\n", opt->flag & OPT_FLAG_DEF ? ' ' : '+', opt->key, OPT_VAL(opt));
		}
		opt = NEXT_OPT(opt);
	}
}

// use mmap method to read file config
static int map_file(const char *path, char **map, int *sz)
{
	int fd;
	struct stat sb;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		plog(CFG, ERR, "open %s failed: %s", path, strerror(errno));
		return -1;
	}

	if (fstat(fd, &sb) < 0) {
		plog(CFG, ERR, "fstat %s failed: %s", path, strerror(errno));
		goto err;
	}

	if (sb.st_size <= 0) {
		plog(CFG, ERR, "empty default config");
		goto err;
	}

	*sz = (int)sb.st_size;
	*map = mmap(NULL, (size_t)*sz, PROT_READ, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		plog(CFG, ERR, "mmap %s failed: %s", path, strerror(errno));
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

static int open_mtd(const char *part_name, int *size, int *esize)
{
	int part_id;
	int mtd_fd;

	part_id = mtd_find(part_name);
	if (part_id == -1) {
		plog(CFG, ERR, "fopen /proc/mtd failed: %s", strerror(errno));
		return -1;
	}
	else if (part_id == -2) {
		plog(CFG, ERR, "mtd partition %s not found", part_name);
		return -1;
	}

	mtd_fd = mtd_open(part_id, O_RDWR);
	if (mtd_fd == -1) {
		plog(CFG, ERR, "open /dev/mtd%d failed: %s", part_id, strerror(errno));
		return -1;
	}

	if (mtd_info(mtd_fd, size, esize, NULL) < 0) {
		plog(CFG, ERR, "get mtd info failed: %s", strerror(errno));
		close(mtd_fd);
		return -1;
	}

	return mtd_fd;
}

static int read_mtd(const char *part_name, int offset, int size, char **mem, int *mem_len)
{
	int ret = -1;
	int fd, mtd_size, esize;

	fd = open_mtd(part_name, &mtd_size, &esize);
	if (fd < 0) {
		plog(CFG, ERR, "open mtd failed");
		return -1;
	}

	if (offset < 0 || size <= 0 || offset % esize || offset + size > mtd_size) {
		plog(CFG, ERR, "read mtd failed: invalid offset %d or size %d, mtd size %d erasesize %d",
				offset, size, mtd_size, esize);
		goto err;
	}

	*mem_len = size;
	*mem = malloc(size);
	if (*mem == NULL) {
		plog(CFG, ERR, "malloc mtd config %d bytes failed: %s", size, strerror(errno));
		goto err;
	}

	ret = mtd_read(fd, offset, *mem, size);
	if (ret < 0) {
		plog(CFG, ERR, "mtd read failed: %s", strerror(errno));
		return -1;
	}

err:
	close(fd);
	if (ret < 0 && *mem)
		free(mem);
	return ret;
}

static int write_mtd(const char *part_name, int offset, int size, char *mem, int len)
{
	int ret = -1;
	int fd, mtd_size, esize;

	fd = open_mtd(part_name, &mtd_size, &esize);
	if (fd < 0)
		return -1;

	if (offset < 0 || size < 0 || offset % esize || offset + size > mtd_size) {
		plog(CFG, ERR, "invalid offset %d or size %d, mtd size %d erasesize %d",
				offset, size, mtd_size, esize);
		goto err;
	}

	ret = mtd_write(fd, offset, esize, mem, len);
	if (ret < 0)
		plog(CFG, ERR, "write mtd ret %d error: %s", ret, strerror(errno));

err:
	close(fd);
	return ret;
}

static int erase_mtd(const char *part_name, int offset, int size)
{
	int ret = -1;
	int fd, mtd_size, esize;

	fd = open_mtd(part_name, &mtd_size, &esize);
	if (fd < 0)
		return -1;
	
	if (offset < 0 || size < 0 || offset % esize || offset + size > mtd_size) {
		plog(CFG, ERR, "invalid offset %d or size %d, mtd size %d erasesize %d",
				offset, size, mtd_size, esize);
		goto err;
	}

	ret = mtd_erase(fd, offset, size);
	if (ret < 0)
		plog(CFG, ERR, "erase mtd ret %d error: %s", ret, strerror(errno));

err:
	close(fd);
	return ret;
}

static void config_shm_init(char *conf)
{
	struct shm_config_head *h = (struct shm_config_head *)conf;
	struct opt_head *opt = (struct opt_head *)(h + 1);
	h->flag = 0;
	opt->len = 0;
}

int config_start(struct config_context *ctx, int flag)
{
	key_t key;
	int shmflag = 0;

	memset(ctx, 0, sizeof(struct config_context));

	if (flag & CFG_START_WITH_LOCK) {
		ctx->lockfd = open(CONFIG_LOCK_PATH, O_CREAT|O_RDWR|O_CLOEXEC, S_IRUSR|S_IWUSR);
		if (ctx->lockfd == -1) {
			plog(CFG, ERR, "config start failed: open %s failed: %s", CONFIG_LOCK_PATH, strerror(errno));
			return -1;
		}

		if (flock(ctx->lockfd, LOCK_EX) < 0) {
			plog(CFG, ERR, "config start failed: flock %s failed: %s", CONFIG_LOCK_PATH, strerror(errno));
			close(ctx->lockfd);
			return -1;
		}
	}
	else
		ctx->lockfd = -1;

	key = ftok(CONFIG_DEFAULT_PATH, 0);

	if (flag & CFG_START_WITH_CREATE_SHM)
		shmflag = IPC_CREAT|IPC_EXCL;

	ctx->shmfd = shmget(key, CONFIG_SHM_SIZE, shmflag);
	if (ctx->shmfd == -1) {
		plog(CFG, ERR, "config start failed: shmget failed: %s", strerror(errno));
		if (ctx->lockfd != -1)
			close(ctx->lockfd);
		return -1;
	}

	if (flag & CFG_START_WITH_RM_SHM)  {
		if (shmctl(ctx->shmfd, IPC_RMID, NULL) < 0) {
			plog(CFG, ERR, "shmctl failed: %s", strerror(errno));
			return -1;
		}
		return 0;
	}
	else {
		ctx->conf = shmat(ctx->shmfd, NULL, 0);
		if (ctx->conf == (void *)-1) {
			plog(CFG, ERR, "config start failed: shmat failed: %s", strerror(errno));
			close(ctx->shmfd);
			if (ctx->lockfd != -1)
				close(ctx->lockfd);
			return -1;
		}

		/* initial shared memory */
		if (flag & CFG_START_WITH_CREATE_SHM)
			config_shm_init(ctx->conf);

		ctx->shmsiz = CONFIG_SHM_SIZE;
	}
	return 0;
}

/* return
	0: ok
	1: empty
	-1: crashed
 */
static int check_mtd_data(char *buf, int buflen, char **data, int *datalen)
{
	struct mtd_config_head *mh = (struct mtd_config_head *)buf;
	*datalen = 0;
	if (mh->magic[0] == 0xff)
		return 1;
	if (memcmp(mh->magic, MTD_CONFIG_MAGIC, 4))
		return -1;
	if (mh->version != MTD_CONFIG_VER)
		return -1;
	if (mh->length + sizeof(*mh) > buflen)
		return -1;
	*data = buf + sizeof(*mh);
	*datalen = mh->length;
	return 0;
}

static int set_mtd_head(char *buf, int datalen)
{
	struct mtd_config_head *mh = (struct mtd_config_head *)buf;

	memset(mh, 0, sizeof(struct mtd_config_head));
	memcpy(mh->magic, MTD_CONFIG_MAGIC, 4);
	mh->version = MTD_CONFIG_VER;
	mh->length = datalen;
	return 0;
}

int config_save(struct config_context *ctx)
{
	char *buf;
	int bufsiz = CONFIG_SIZE;
	int buflen = 0;
	struct shm_config_head *h = (struct shm_config_head *)ctx->conf;
	int ret = -1;

	if (h->flag & SHM_FLAG_ERASED)
		return 0;
	
	buf = malloc(bufsiz);
	if (buf == NULL) {
		plog(CFG, ERR, "config save failed: malloc failed: %s", strerror(errno));
		return -1;
	}

	buflen = _config_save(SHM_CONF_DATA(ctx->conf), SHM_CONF_DSIZE(ctx->shmsiz),
			buf + sizeof(struct mtd_config_head), bufsiz - sizeof(struct mtd_config_head));
	if (buflen < 0) {
		plog(CFG, ERR, "_config_save failed\n");
		goto err;
	}

	if (set_mtd_head(buf, buflen)) {
		plog(CFG, ERR, "config save failed: set mtd head failed");
		goto err;
	}

	ret = write_mtd(CONFIG_PARTITION, CONFIG_OFFSET, CONFIG_SIZE,
			buf, sizeof(struct mtd_config_head) + buflen);
	if (ret < 0) {
		plog(CFG, ERR, "config save failed: write mtd failed");
	}

	ret = 0;

err:
	if (buf)
		free(buf);
	return ret;
}

void config_end(struct config_context *ctx)
{
	if (ctx->changed) 
		config_save(ctx);

	if (ctx->conf && -1 == shmdt(ctx->conf))
		plog(CFG, ERR, "config_end failed: shmdt failed: %s", strerror(errno));
	if (ctx->shmfd != -1)
		close(ctx->shmfd);
	if (ctx->lockfd != -1)
		close(ctx->lockfd);
}

int config_erase(struct config_context *ctx)
{
	struct shm_config_head *h = (struct shm_config_head *)ctx->conf;

	if (erase_mtd(CONFIG_PARTITION, CONFIG_OFFSET, CONFIG_SIZE) < 0) {
		plog(CFG, ERR, "config erase failed: erase mtd failed");
		return -1;
	}

	h->flag |= SHM_FLAG_ERASED;
	return 0;
}

int config_load(struct config_context *ctx)
{
	int fd;
	char *data[2];
	int len[2];
	int i;
	int res = -1, ret;
	char *mtd_data = NULL;
	int mtd_len = 0;
	
	fd = map_file(CONFIG_DEFAULT_PATH, &data[0], &len[0]);
	if (fd < 0) {
		plog(CFG, ERR, "config_load failed: map %s failed", CONFIG_DEFAULT_PATH);
		return -1;
	}

	if (read_mtd(CONFIG_PARTITION, CONFIG_OFFSET, CONFIG_SIZE, &mtd_data, &mtd_len) < 0) {
		plog(CFG, ERR, "config_load failed: can't read mtd data");
		goto err;
	}

	ret = check_mtd_data(mtd_data, mtd_len, &data[1], &len[1]);
	if (ret == -1) {
		plog(CFG, ERR, "config_load failed: config data in mtd partition crashed");
	}

	for (i = 0; i < 2; i++) {
		int p = 0;
		int kv[4] = {0};	/* GCC complains: error: '*((void *)&kv+12)' may be used uninitialized in this function
								but kv is set in _config_parse. initit to zero to make gcc happy */
		char *sec = NULL;
		int seclen = 0;
		while (p < len[i]) {
			ret = _config_parse(data[i], len[i], &p, kv);
			if (ret < 0) {
				plog(CFG, ERR, "config_load failed: parse %s config failed", i ? "user" : "default");
				goto err;
			}
			else if (ret == 1) {
				sec = data[i] + kv[0];
				seclen = kv[1];
			}
			else if (ret == 2) {
				if (sec == NULL || kv[1] <= 0 || kv[3] <= 0) {
					plog(CFG, ERR, "config_load failed: invalid config data");
					goto err;
				}

				ret = _config_op(SHM_CONF_DATA(ctx->conf), SHM_CONF_DSIZE(ctx->shmsiz), 0,
					sec, seclen, data[i] + kv[0], kv[1], data[i] + kv[2], kv[3], i == 0 ? 1 : 0);
				if (ret == -1) {
					plog(CFG, ERR, "config load failed: buffer overflow");
					goto err;
				}
			}
		}
	}

	res = 0;
err:
	munmap(data[0], len[0]);
	close(fd);
	if (mtd_data)
		free(mtd_data);
	return res;
}

const char *config_get(struct config_context *ctx, const char *sec, const char *key)
{
	char *val = NULL;
	_config_op(SHM_CONF_DATA(ctx->conf), SHM_CONF_DSIZE(ctx->shmsiz), 1,
		sec, strlen(sec), key, strlen(key), &val, 0, 0);
	return val;
}

/* val: null: delete the entry */
int config_set(struct config_context *ctx, const char *sec, const char *key, const char *val)
{
	int ret;
	if (val == NULL)
		return 0;
	ret =  _config_op(SHM_CONF_DATA(ctx->conf), SHM_CONF_DSIZE(ctx->shmsiz), 0,
		sec, strlen(sec), key, strlen(key), (void *)val, val ? strlen(val) : 0, 0);
	if (ret == 1)
		ctx->changed = 1;
	else if (ret == 2)
		ctx->err = 1;
	return ret;
}

void config_dump(struct config_context *ctx, const char *sec)
{
	if (sec && sec[0] == '\0')
		return;
	_config_dump(SHM_CONF_DATA(ctx->conf), SHM_CONF_DSIZE(ctx->shmsiz), sec);
}

