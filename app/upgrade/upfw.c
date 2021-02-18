/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fwhdr.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "gcrypt.h"

#define PROG		"upfw"
#define DEFPUBKEY	"/etc/xrouter.pub"

struct upfw
{
	const char *pub_key;
	const char *image;
	const char *extract_dir;
	int force;

	int fd;
	void *fw;
	int fsize;

	uint32_t parts[MAX_PARTS], files[MAX_FILES];
};

static void usage(const char *fmt, ...)
{
	if (help)
		fprintf(stderr, "%s\n", help);

	fprintf(stderr,
		"%s <check|extract|upgrade> [options] image\n"
		"\n"
		"options:\n"
		"    -f, upgrade no signature image\n"
		"    -k <pub key path>, default public key: %s\n"
		"    -e <extract dir>, extract dir\n"
		"\n",
		PROG, DEFPUBKEY);

	exit(1);
}

static int load_key(const char *path, uint8_t **key, int *keylen)
{
	int ret = 1;
	struct stat keystat;
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return 1;
	
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

static int fw_rsa_verify(void *digest, void *sign, const char *public_key_path)
{
	int ret = -1;
	gcry_error_t gret;
	uint8_t *key;
	int keylen;
	gcry_sexp_t pubkey = NULL;
	gcry_mpi_t digest_mpi = NULL;
	gcry_sexp_t digest_sexp = NULL;
	gcry_mpi_t sign_mpi = NULL;
	gcry_sexp_t sign_sexp = NULL;

	if (load_key(public_key_path, &key, &keylen)) {
		fprintf(stderr, "load public key failed\n");
		return ret;
	}

	if (gcry_sexp_new(&pubkey, key, keylen, 1)) {
		fprintf(stderr, "new s-exp failed\n");
		goto error;
	}

	if (gcry_mpi_scan(&digest_mpi, GCRYMPI_FMT_USG,
		digest, FW_DIGEST_SIZE, NULL)) {
		fprintf(stderr, "scan digest mpi failed\n");
		goto error;
	}

	if (gcry_sexp_build(&digest_sexp, NULL,
		"(data (flags raw) (value %M))", digest_mpi)) {
		fprintf(stderr, "build digest sexp failed\n");
		goto error;
	}

	if (gcry_mpi_scan(&sign_mpi, GCRYMPI_FMT_USG,
		sign, FW_SIGN_SIZE, NULL)) {
		fprintf(stderr, "scan sign mpi failed\n");
		goto error;
	}

	if (gcry_sexp_build(&sign_sexp, NULL,
		"(sig-val (rsa (s %M)))", sign_mpi)) {
		fprintf(stderr, "build sign sexp failed\n");
		goto error;
	}

	gret = gcry_pk_verify(sign_sexp, digest_sexp, pubkey);
	if (gret == 0)
		ret = 0;
	else if (gcry_err_code(gret) == GPG_ERR_BAD_SIGNATURE)
			ret = 1;
	else {
		fprintf(stderr, "pk verify internal error\n");
	}

error:
	free(key);
	if (pubkey)
		gcry_sexp_release(pubkey);
	if (digest_mpi)
		gcry_mpi_release(digest_mpi);
	if (digest_sexp)
		gcry_sexp_release(digest_sexp);
	if (sign_mpi)
		gcry_mpi_release(sign_mpi);
	if (sign_sexp)
		gcry_sexp_release(sign_sexp);
	return ret;
}

static int open_fw(struct ufpw *up)
{
	struct stat statbuf;

	up->fd = open(up->image, O_RDONLY);
	if (up->fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", up->image, strerror(errno));
		return -1;
	}

	if (fstat(up->fd, &statbuf) < 0) {
		fprintf(stderr, "fstat failed: %s\n", strerror(errno));
		return -1;
	}
	up->fsize = statbuf.st_size;

	up->fw = mmap(NULL, up->fsize, PROT_READ, MAP_SHARED, up->fd, 0);
	if (up->fw == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int is_name(char *name, int size)
{
	int i, f = 0;
	for (i = size; i > 0; i--) {
		char c = name[i-1];
		if (f) {
			if (!c)
				return 0;
		}
		else if (c)
			f = i;
	}
	return f > 0 && f < size;
}

#define IS_TRUNC(off, cnt, size)	((off) + (cnt) > (size))

static int check_fw(struct upfw *up)
{
	struct fwhdr *hdr = up->fw;
	uint32_t offset = sizeof(struct fwhdr);
	uint32_t parts, files;

	if (up->fsize < sizeof(struct fwhdr)) {
		fprintf(stderr, "image too small\n");
		return -1;
	}

	if (memcmp(hdr->magic, FW_MGC, 4)) {
		fprintf(stderr, "invalid magic\n");
		return -1;
	}

	if (ntohl(hdr->fwver) != FW_VER) {
		fprintf(stderr, "unsupported firmware version\n");
		return -1;
	}

	parts = ntohl(hdr->parts);
	if (parts == 0 || parts >= MAX_PARTS) {
		fprintf(stderr, "invalid parts\n");
		return -1;
	}
	for (i = 0; i < parts; i++) {
		struct fwpart *part = (struct fwpart *)(up->fw + offset);
		if (IS_TRUNC(offset,  sizeof(struct fwpart), up->fsize))
			goto err_truncated;
		if (!is_name(part->part_name, sizeof(part->part_name))) {
			fprintf(stderr, "invalid part name\n");
			return -1;
		}
		up->parts[i] = offset;
		offset += sizeof(struct fwpart) + (uint32_t)ntohl(part->image_size);
		if (IS_TRUNC(offset, 0, up->fsize))
			goto err_truncated;
		offset = FW_ALIGN(offset);
	}

	files = ntohl(hdr->files);
	if (files >= MAX_FILES) {
		fprintf(stderr, "invalid files\n");
		return -1;
	}
	for (i = 0; i < files; i++) {
		struct fwfile *file = (struct fwfile *)(up->fw + offset);
		if (IS_TRUNC(offset,  sizeof(struct fwfile), up->fsize))
			goto err_truncated;
		if (!is_name(file->file_name, sizeof(file->file_name))) {
			fprintf(stderr, "invalid file name\n");
			return -1;
		}
		up->files[i] = offset;
		offset += sizeof(struct fwfile) + (uint32_t)ntohl(file->file_len);
		if (IS_TRUNC(offset, 0, up->fsize))
			goto err_truncated;
		offset = FW_ALIGN(offset);
	}

	return 0;

err_truncated:
	fprintf(stderr, "image may be truncated\n");
	return -1;
}

static int check_sha256(struct upfw *up)
{
	struct fwhdr *hdr = up->fw;
	uint32_t flags = ntohl(hdr->flags);
	gcry_md_hd_t handle;
	struct fwhdr hdr2;
	char digest[FW_DIGEST_SZ];

	if (!(flags & FW_FLAG_SHA256)) {
		fprintf(stderr, "no sha256");
		return -1;
	}

	if (gcry_md_open(&handle, GCRY_MD_SHA256, 0)) {
		fprintf(stderr, "gcry md open failed\n");
		return -1;
	}

	memcpy(hdr2, hdr, sizeof(struct fwhdr));
	memset(hdr2->digest, 0, sizeof(hdr2->digest));
	memset(hdr2->sign, 0, sizeof(hdr2->sign));

	gcry_md_write(handle, hdr2, sizeof(struct fwhdr));

	gcry_md_write(handle, (char *)up->fw + sizeof(struct fwhdr),
			up->fsize - sizeof(struct fwhdr));

	memcpy(digest, gcry_md_read(handle, GCRY_MD_SHA256), FW_DIGEST_SZ);
	gcry_md_close(hd);

	if (memcmp(digest, hdr->digest, FW_DIGEST_SZ)) {
		fprintf(stderr, "digest mismatch, image may corrupted\n");
		return -1;
	}

	return 0;
}

static int check_rsa_sign(struct upfw *up)
{
	struct fwhdr *hdr = up->fw;
	uint32_t flags = ntohl(hdr->flags);

	if (!(flags & FW_FLAG_RSA_SIGN)) {
		if (!up->force) {
			fprintf(stderr, "no signature\n");
			return -1;
		}
		return 0;
	}

	return fw_rsa_verify(hdr->digeset, hdr->sign, up->pub_key ? : DEFPUBKEY);
}

static int parse_fw(struct upfw *up)
{
	return open_fw(up) || check_fw(up) || check_sha256(up) || check_rsa_sign(up);
}

static int check(struct upfw *up)
{
	return parse_fw(up) ? 1 : 0;
}

static int extract(struct upfw *up)
{
}

static int upgrade(struct upfw *up)
{
}

static int pre_match(const char *sub, const char *str)
{
	int l_len = strlen(sub), r_len = strlen(str);
	return l_len > 0 && l_len <= r_len && !memcmp(sub, str, l_len);
}

static int parse_args(struct upfw *up, int argc, char **argv)
{
	if (argc < 2)
		return -1;

	optind = 2;
	while ((opt = getopt(argc, argv, "fk:e:")) != -1) {
		switch (opt) {
		case 'f': up->force = 1; break;
		case 'k': up->pub_key = optarg; break;
		case 'e': up->extract_dir = optarg; break;
		default: return -1;
		}
	}

	if (optind == argc || optind < argc - 1) 
		return -1;
	up->image = argv[argc - 1];

	return 0;
}

int main(int argc, char **argv)
{
	struct upfw up = {0};
	const char *cmd;

	if (parse_args(&up, argc, argv))
		usage(NULL);

	cmd = argv[1];

	if (pre_match(cmd, "check"))
		return check(&up);

	else if (pre_match(cmd, "extract"))
		return extract(&up);

	else if (pre_match(cmd, "upgrade"))
		return upgrade(&up);

	usage("unknown cmd: %s\n", cmd);
	return 1;
}

