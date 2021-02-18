#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <gcrypt.h>
#include <arpa/inet.h>	// htonl
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <libgen.h>
#include "fwhdr.h"

struct fw
{
	struct fwhdr hdr;

	int partno;
	struct {
		struct fwpart part;
		char *image;
	} parts[32];

	int fileno;
	struct {
		struct fwfile file;
		char *path;
	} files[128];

#define SEC_GLOBAL		0
#define SEC_PARTITION	1
#define SEC_FILE		2
	int section;

	void *sha256_handle;
	int sha256_done;
	int offset;
	int fd;
	int no_digest;
	char *priv_rsa_key;
	int flags;
};

static void usage(const char *warn)
{
	if (warn)
		fprintf(stderr, "%s\nusage:\n", warn);

	fprintf(stderr,
		"mkfw -c <config path> [options] <image path>\n"
		"mkfw -d [-e <dir>] <image path>, dump image\n"
		"mkfw -h\n"
		"\n"

		"options:\n"
		"    -v <ver>, software version\n"
		"    -f <path>, add file to image\n"
		"    -D, No digest\n"
		"    -k <priv key>, RSA signature key path\n"
		"    -e <dir>, extract partition images and files to dir\n"
		"    -h, show this usage\n"
		"\n"
	);
	exit(1);
}

static void fatal(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "fatal: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

static unsigned get_num(char *str)
{
	char *end;
	unsigned long ret;
	unsigned base = 1;
	ret = strtoul(str, &end, 0);
	if (*end == 'M' || *end == 'm')
		base = 1024 * 1024;
	if (*end == 'K' || *end == 'K')
		base = 1024;
	return ret * base;
}

static uint32_t parse_softver(char *ver)
{
	unsigned a, b, c;
	int ret;
	ret = sscanf(ver, "%u.%u.%u", &a, &b, &c);
	if (ret != 3)
		return 0;
	return (a << 16) + (b << 8) + c;
}

static uint32_t parse_hwver(char *ver)
{
	unsigned a, b;
	int ret;
	ret = sscanf(ver, "%u.%u", &a, &b);
	if (ret != 2)
		return 0;
	return (a << 8) + b;
}


static void add_file(struct fw *fw, const char *file)
{
	if (fw->fileno >= sizeof(fw->files)/sizeof(fw->files[0]))
		fatal("too many files\n");

	fw->files[fw->fileno].path = strdup(file);
	if (fw->files[fw->fileno++].path == NULL)
		fatal("strdup file failed: %s\n", strerror(errno));
}

static void parse_sec(struct fw *fw, char *sec)
{
	if (!strcmp(sec, "partition")) {
		fw->partno++;
		if (fw->partno > sizeof(fw->parts)/sizeof(fw->parts[0]))
			fatal("too many partitions\n");
		fw->section = SEC_PARTITION;
	}
	else if (!strcmp(sec, "file"))
		fw->section = SEC_FILE;
	else 
		fatal("unknown section %s\n", sec);
}

static void parse_key(struct fw *fw, char *name, char *value)
{
	if (fw->section == SEC_GLOBAL) {
		if (!strcmp(name, "software name")) {
			int len = strlen(value);
			if (len <= 0)
				fatal("software name require value\n");
			else if (len >= sizeof(fw->hdr.swname))
				fatal("software name too long\n");
			strcpy(fw->hdr.swname, value);
		}
		else if (!strcmp(name, "software version")) {
			fw->hdr.swver = parse_softver(value);
			if (fw->hdr.swver == 0)
				fatal("invalid softver\n");
		}
		else if (!strcmp(name, "compatible hardware versions")) {
			char *ver = strtok(value, " ");
			int hwver_no = 0;
			while (ver) {
				if (hwver_no >= sizeof(fw->hdr.hwver)/sizeof(fw->hdr.hwver[0]))
					fatal("no many hardware versions\n");
				fw->hdr.hwver[hwver_no] = parse_hwver(ver);
				if (fw->hdr.hwver[hwver_no] == 0)
					fatal("invalid hardware version\n");
				hwver_no++;
				ver = strtok(NULL, " ");
			}
		}
		else 
			fatal("unknown global config: %s\n", name);
	}

	if (fw->section == SEC_PARTITION) {
		if (!strcmp(name, "name")) {
			if (strlen(value) == 0 || strlen(value) >= sizeof(fw->parts[0].part.part_name))
				fatal("part name too long or empty\n");
			strcpy(fw->parts[fw->partno - 1].part.part_name, value);
		}
		else if (!strcmp(name, "offset")) {
			fw->parts[fw->partno - 1].part.part_off = get_num(value);
		}
		else if (!strcmp(name, "size")) {
			fw->parts[fw->partno - 1].part.part_size = get_num(value);
		}
		else if (!strcmp(name, "image")) {
			fw->parts[fw->partno - 1].image = strdup(value);
			if (fw->parts[fw->partno - 1].image == NULL)
				fatal("strdup image failed: %s\n", strerror(errno));
		}
		else
			fatal("unknown partition config: %s\n", name);
	}

	if (fw->section == SEC_FILE)
		add_file(fw, name);
}

static char *strip_white(char *str)
{
	char *st = NULL, *en = NULL, c;

	/* skip the heading white space */
	while ((c = *str)) {
		if (c == '\r' || c == '\n') {
			*str = 0;
			break;
		}
		else if (c != ' ' && c != '\t')
			break;
		str++;
	}
	st = str;

	while ((c = *str)) {
		if (c == '\r' || c == '\n') {
			*str = 0;
			break;
		}
		if (c != ' ' && c != '\t')
			en = str;
		str++;
	}
	if (en)
		*(en + 1) = '\0';
	return st;
}

static void parse_cfg(struct fw *fw, const char *cfg)
{
	char buf[1024];
	FILE *f;

	f = fopen(cfg, "r");
	if (f == NULL)
		fatal("read cfg: %s failed: %s\n", cfg, strerror(errno));

	while (fgets(buf, sizeof(buf), f)) {
		char *name, *value;
		char *tmp = strip_white(buf);

		/* empty line or comment line */
		if (*tmp == 0 || *tmp == '#')
			continue;

		if (*tmp == '[') {
			name = tmp + 1;
			/* find ] */
			tmp = strchr(name, ']');
			if (!tmp)
				fatal("invalid config syntax\n");
			*tmp = '\0';

			name = strip_white(name);
			parse_sec(fw, name);
		}
		else {
			name = tmp;
			tmp = strchr(tmp, '=');
			if (tmp) {
				*tmp = '\0';
				value = tmp + 1;
			}
			else
				value = "";
			name = strip_white(name);
			value = strip_white(value);
			parse_key(fw, name, value);
		}
	}

	fclose(f);
}

static void check_cfg(struct fw *fw)
{
	int i;

	if (fw->hdr.swname[0] == 0)
		fatal("no software name\n");

	if (fw->hdr.swver == 0)
		fatal("no software version\n");

	if (fw->hdr.hwver[0] == 0)
		fatal("no compatible hardware version\n");

	if (fw->partno == 0 )
		fatal("no partition\n");

	for (i = 0; i < fw->partno; i++)
	{
		if (fw->parts[i].part.part_name[0] == 0)
			fatal("no partition name\n");

		if (fw->parts[i].part.part_size == 0)
			fatal("partition %s: no size\n", fw->parts[i].part.part_name);

		if (fw->parts[i].image == NULL)
			fatal("partition %s: no image\n", fw->parts[i].part.part_name);
	}
}

static int sha256_init(void **hd)
{
	return gcry_md_open((gcry_md_hd_t *)hd, GCRY_MD_SHA256, 0) ? -1 : 0;
}

static void sha256_update(void *hd, const void *data, int len)
{
	gcry_md_write(hd, data, len);
}

static void sha256_final(void *hd, void *digest)
{
	memcpy(digest, gcry_md_read(hd, GCRY_MD_SHA256), 32);
	gcry_md_close(hd);
}

static int get_file_size(const char *path)
{
	int size;
	struct stat statbuf;
	if (stat(path, &statbuf) < 0)
		fatal("stat %s failed: %s\n", path, strerror(errno));
	size = statbuf.st_size;
	if (size == 0)
		fatal("size of %s is zero\n", path);
	if (size > 32*1024*1024)
		fatal("size of %s is too bigger\n", path);
	return size;
}

static void write_buf(struct fw *fw, char *data, int len, int align)
{
	if (align && (fw->offset & 0x3)) {
		int padding = (fw->offset | 0x3) + 1 - fw->offset;
		char padding_byte[4] = { 0xff, 0xff, 0xff, 0xff };
		write_buf(fw, padding_byte, padding, 0);
	}

	if (fw->flags & FW_FLAG_SHA256 && !fw->sha256_done)
		sha256_update(fw->sha256_handle, data, len);

	while (len) {
		int ret = write(fw->fd, data, len);
		if (ret < 0) {
			if (errno != EINTR)
				fatal("write failed: %s\n", strerror(errno));
			continue;
		}
		len -= ret;
		data += ret;
	}

	fw->offset += len;
}

static void write_file(struct fw *fw, const char *path, int size)
{
	static char buf[4094];
	int done = 0, fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		fatal("open %s for read failed: %s\n", path, strerror(errno));

	while (1) {
		int ret = read(fd, buf, sizeof(buf));
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fatal("read %s failed: %s\n", path, strerror(errno));
		}
		if (ret == 0) {
			if (done != size)
				fatal("partial data readed\n");
			return;
		}
		done += ret;
		if (done > size)
			fatal("too many data readed %d > %d\n", done, size);
		write_buf(fw, buf, ret, 0);
	}
}

static void wr_part(struct fw *fw, struct fwpart *part, char *image)
{
	int size = get_file_size(image);

	if (size > part->part_size)
		fatal("image %s is too large\n", image);

	part->part_off = htonl(part->part_off);
	part->part_size = htonl(part->part_size);
	part->image_size = htonl(size);

	write_buf(fw, (char *)part, sizeof(*part), 1);
	write_file(fw, image, size);
}

static void wr_file(struct fw *fw, struct fwfile *file, char *path)
{
	int size;
	char *name = basename(path);

	if (strlen(name) >= sizeof(file->file_name))
		fatal("file name too long\n");

	size = get_file_size(path);
	file->file_len = htonl(size);
	strcpy(file->file_name, name);

	write_buf(fw, (char *)file, sizeof(struct fwfile), 1);
	write_file(fw, path, size);
}

static void seek(struct fw *fw, int off)
{
	off_t ret = lseek(fw->fd, off, SEEK_SET);
	if (ret != off)
		fatal("seek failed: %s\n", strerror(errno));
	fw->offset = off;
}

static char *sz(uint32_t size, char *buf)
{
	uint32_t mega, kilo, bytes;
	mega = size / 1024 / 1024;
	size -= mega * 1024 * 1024;
	kilo = size / 1024;
	size -= kilo * 1024;
	bytes = size;
	buf[0] = '\0';
	if (mega)
		sprintf(buf + strlen(buf), "%uM", mega);
	if (kilo)
		sprintf(buf + strlen(buf), "%uK", kilo);
	if (bytes || (!mega && !kilo))
		sprintf(buf + strlen(buf), "%uB", kilo);
	return buf;
}

static void dump_image(struct fw *fw, const char *image)
{
	int i;
	struct fwhdr *hdr = &fw->hdr;
	uint32_t flags = ntohl(hdr->flags);
	uint32_t ver = ntohl(hdr->swver);
	char buf[128];
	char sz0[128];
	char sz1[128];

	time_t t = (time_t)ntohl(hdr->buildtime);
	strftime(buf, sizeof(buf), "%F %H:%M", localtime(&t));

	printf("===\n");
	printf("%-30s : %d\n", "Fw version", ntohl(hdr->fwver));
	printf("%-30s : %s\n", "Sw name", hdr->swname);
	printf("%-30s : %u.%u.%u\n", "Sw version", ver >> 16, (ver >> 8) & 0xff, ver & 0xff);
	printf("%-30s : %s\n", "Build time", buf);
	printf("%-30s : ", "Supported Hardware version");
	for (i = 0; i < sizeof(hdr->hwver)/sizeof(hdr->hwver[0]); i++) {
		ver = ntohl(hdr->hwver[i]);
		if (ver == 0)
			break;
		printf("%u.%u ", ver >> 8, ver & 0xff);
	}
	printf("\n");
	printf("%-30s : %s %s\n", "Flags",
		flags & FW_FLAG_SHA256 ? "SHA256" : "",
		flags & FW_FLAG_RSA_SIGN ? "RSA_SIGN" : "");
	printf("---\n");
	printf("%-4s %-16s %-8s   %-8s   %-10s %-10s %s\n",
		"ID", "Name", "Start", "End", "Size", "Used", "Image");
	for (i = 0; i < ntohl(hdr->parts); i++) {
		uint32_t off = ntohl(fw->parts[i].part.part_off);
		uint32_t size = ntohl(fw->parts[i].part.part_size);
		uint32_t image_size = ntohl(fw->parts[i].part.image_size);
		printf("%-4d %-16s %08X   %08X   %-10s %-10s %s\n",
			i, fw->parts[i].part.part_name,
			off, off + size, sz(size, sz0), sz(image_size, sz1),
			fw->parts[i].image);
	}
	printf("---\n");
	printf("%-4s %-8s %s\n", "ID", "Size", "Name");
	for (i = 0; i < ntohl(hdr->files); i++) {
		uint32_t size = ntohl(fw->files[i].file.file_len);
		printf("%-4d %-8d %s\n", i, size, fw->files[i].path);
	}
	printf("===\n");
	if (image)
		printf("Output image to: %s\n", image);
}

static int load_key(const char *path, uint8_t **key, int *keylen)
{
	int ret = -1;
	struct stat keystat;
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;
	
	*key = NULL;
	if (-1 == fstat(fd, &keystat))
		goto error;

	if (keystat.st_size == 0)
		goto error;

	*key = malloc(keystat.st_size);
	if (*key == NULL)
		goto error;

	*keylen = keystat.st_size;
	
	if (*keylen != read(fd, *key, *keylen))
		goto error;

	ret = 0;

error:
	close(fd);
	if (ret && *key)
		free(*key);
	return ret;
}

static void fw_rsa_sign(void *digest, uint8_t *out_sign, const char *priv_key_path)
{
	uint8_t *key;
	int keylen;
	gcry_sexp_t seckey = NULL;
	gcry_mpi_t digest_mpi = NULL;
	gcry_sexp_t digest_sexp = NULL;
	gcry_sexp_t sinkey = NULL;
	gcry_sexp_t sin_data_sexp = NULL;
	const char *sign_data;
	size_t sign_data_len;

	if (load_key(priv_key_path, &key, &keylen))
		fatal("load private key failed\n");

	if (gcry_sexp_new(&seckey, key, keylen, 1))
		fatal("new s-exp failed\n");

	if (gcry_mpi_scan(&digest_mpi, GCRYMPI_FMT_USG, digest, FW_DIGEST_SZ, NULL))
		fatal("scan digest mpi failed\n");

	if (gcry_sexp_build(&digest_sexp, NULL, "(data (flags raw) (value %M))", digest_mpi))
		fatal("build digest sexp failed\n");

	if (gcry_pk_sign(&sinkey, digest_sexp, seckey))
		fatal("signature failed\n");

	sin_data_sexp = gcry_sexp_find_token(sinkey, "s", 0);
	if (sin_data_sexp == NULL)
		fatal("find sin data sexp failed\n");

	sign_data = gcry_sexp_nth_data(sin_data_sexp, 1, &sign_data_len);
	if (sign_data == NULL)
		fatal("get sign data failed\n");

	/* signature success */
	memset(out_sign, 0, FW_SIGN_SZ);
	memcpy(out_sign + FW_SIGN_SZ - sign_data_len, sign_data, sign_data_len);

	free(key);
	if (seckey)
		gcry_sexp_release(seckey);
	if (digest_mpi)
		gcry_mpi_release(digest_mpi);
	if (digest_sexp)
		gcry_sexp_release(digest_sexp);
	if (sinkey)
		gcry_sexp_release(sinkey);
	if (sin_data_sexp)
		gcry_sexp_release(sin_data_sexp);
}

static void mk_image(struct fw *fw, const char *image)
{
	struct fwhdr *hdr = &fw->hdr;
	int i;

	if (!fw->no_digest && sha256_init(&fw->sha256_handle))
		fatal("init sha256 failed\n");
	
	if (!fw->no_digest) {
		fw->flags |= FW_FLAG_SHA256;
		if (fw->priv_rsa_key)
			fw->flags |= FW_FLAG_RSA_SIGN;
	}

	memcpy(hdr->magic, FW_MGC, 4);
	hdr->fwver = htonl(FW_VER);
	hdr->flags = htonl(fw->flags);
	hdr->swver = htonl(hdr->swver);
	hdr->buildtime = htonl((uint32_t)time(NULL));

	for (i = 0; i < sizeof(hdr->hwver)/sizeof(hdr->hwver[0]); i++) {
		if (hdr->hwver[i] == 0)
			break;
		hdr->hwver[i] = htonl(hdr->hwver[0]);
	}

	hdr->parts = htonl(fw->partno);
	hdr->files = htonl(fw->fileno);

	if (fw->flags & FW_FLAG_SHA256)
		sha256_update(fw->sha256_handle, (char *)hdr, sizeof(struct fwhdr));

	fw->fd = open(image, O_RDWR|O_TRUNC|O_CREAT, 0644);
	if (fw->fd < 0)
		fatal("open image %s failed: %s\n", image, strerror(errno));
	
	seek(fw, sizeof(*hdr));

	for (i = 0; i < fw->partno; i++)
		wr_part(fw, &fw->parts[i].part, fw->parts[i].image);
	
	for (i = 0; i < fw->fileno; i++)
		wr_file(fw, &fw->files[i].file, fw->files[i].path);

	if (fw->flags & FW_FLAG_SHA256) {
		sha256_final(fw->sha256_handle, hdr->digest);
		fw->sha256_done = 1;
	}

	if (fw->flags & FW_FLAG_RSA_SIGN)
		fw_rsa_sign(hdr->digest, hdr->sign, fw->priv_rsa_key);

	seek(fw, 0);
	write_buf(fw, (char *)hdr, sizeof(*hdr), 0);

	dump_image(fw, image);
}

static void safe_read(struct fw *fw, char *buf, int len, int align)
{
	if (align && (fw->offset & 0x3)) {
		int padding = (fw->offset | 0x3) + 1 - fw->offset;
		char padding_byte[4] = { 0xff, 0xff, 0xff, 0xff };
		char bytes[4];
		safe_read(fw, bytes, padding, 0);
		if (memcmp(bytes, padding_byte, padding))
			fatal("invalid padding\n");
	}

	fw->offset += len;
	while (len > 0) {
		int ret = read(fw->fd, buf, len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fatal("read failed: %s\n", strerror(errno));
		}
		else if (ret == 0)
			fatal("no enough data, may file truncated\n");
		buf += ret;
		len -= ret;
	}
}

static void extract_img(struct fw *fw, char *name, char *dir, int len)
{
	int dirfd, fd, ret;

	fw->offset += len;

	if (!dir) {
		ret = lseek(fw->fd, len, SEEK_CUR);
		if (ret == -1)
			fatal("lseek failed: %s\n", strerror(errno));
		return;
	}

	dirfd = open(dir, O_RDONLY|O_DIRECTORY);
	if (dirfd < 0)
		fatal("open dir %s failed: %s\n", dir, strerror(errno));

	fd = openat(dirfd, name, O_RDWR|O_CREAT|O_TRUNC, 0644);
	if (fd < 0)
		fatal("open %s/%s failed: %s\n", dir, name, strerror(errno));

	while (len) {
		ret = sendfile(fd, fw->fd, NULL, len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fatal("sendfile failed: %s\n", strerror(errno));
		}
		else if (ret == 0)
			fatal("no enough data, may file truncated\n");
		len -= ret;
	}

	close(fd);
	close(dirfd);
}

static int read_image(struct fw *fw, char *image, char *extract)
{
	struct fwhdr *hdr = &fw->hdr;
	int i, len;
	char name[16];

	fw->fd = open(image, O_RDONLY);
	if (fw->fd == -1)
		fatal("open image %s failed: %s\n", image, strerror(errno));

	safe_read(fw, (char *)hdr, sizeof(struct fwhdr), 0);
	
	fw->partno = ntohl(hdr->parts);
	fw->fileno = ntohl(hdr->files);

	for (i = 0; i < fw->partno; i++) {
		safe_read(fw, (char *)&fw->parts[i].part, sizeof(struct fwpart), 1);
		sprintf(name, "part%d", i);
		len = ntohl(fw->parts[i].part.image_size);
		if (len < 0)
			fatal("invalid part image len\n");
		extract_img(fw, name, extract, len);
		fw->parts[i].image = "";
	}

	for (i = 0; i < fw->fileno; i++) {
		safe_read(fw, (char *)&fw->files[i].file, sizeof(struct fwfile), 1);
		len = ntohl(fw->files[i].file.file_len);
		if (len < 0)
			fatal("invalid part image len\n");
		extract_img(fw, fw->files[i].file.file_name, extract, len);
		fw->files[i].path = fw->files[i].file.file_name;
	}
	dump_image(fw, NULL);
}

int main(int argc, char **argv)
{
	int opt;
	char *cfg = NULL;
	uint32_t softver = 0;
	char *image = NULL;
	struct fw fw = { 0 };
	int dump = 0;
	char *extract_dir = NULL;

	while ((opt = getopt(argc, argv, "c:v:hf:Dk:de:")) != -1) {
		switch (opt) {
		case 'c':
			cfg = optarg;
			break;
		case 'v':
			softver = parse_softver(optarg);
			if (softver == 0)
				fatal("invalid soft version\n");
			break;
		case 'f':
			add_file(&fw, optarg);
			break;
		case 'D':
			fw.no_digest = 1;
			break;
		case 'k':
			fw.priv_rsa_key = strdup(optarg);
			break;
		case 'd':
			dump = 1;
			break;
		case 'e':
			extract_dir = optarg;
			break;
		default:
		case 'h':
			usage(NULL);
		}
	}

	/* image path */
	if (optind == argc || optind < argc - 1) 
		usage("invalid arguments\n");

	image = argv[argc - 1];

	if (dump)
		return read_image(&fw, image, extract_dir);

	if (cfg == NULL)
		usage("require config\n");

	parse_cfg(&fw, cfg);
	check_cfg(&fw);

	mk_image(&fw, image);
	return 0;
}

