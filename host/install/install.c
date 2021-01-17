/*
 * the install program for Xrouter
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * this install implementation always compare the timestamp
 * if the dest is newer than the src, then install do nothing.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>

#define FORCE	0x100
#define STRIP	0x200
#define TODIR	0x400
/* keep directory hierarchy when install
	a relative path file */
#define KEEP_HIERARCHY	0x800

static int dbg_level;
int mkdir_p(char *path, unsigned mode);
int mkdir_l(char *path, unsigned mode);

#define fatal(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); exit(1); } while (0)
#define debug(fmt, ...) do { if (dbg_level) printf(fmt, ##__VA_ARGS__); } while (0)

static void cp(int srcfd, char *src, int dstfd, char *dst, int len, unsigned mode)
{
	int fd_src, fd_dst, ret;

	debug("Copy %s\n", src);

	fd_src = openat(srcfd, src, O_RDONLY);
	if (fd_src == -1)
		fatal("open %s failed: %s\n", src, strerror(errno));

	fd_dst = openat(dstfd, dst, O_WRONLY|O_TRUNC|O_CREAT, mode);
	if (fd_dst == -1)
		fatal("open %s failed: %s\n", dst, strerror(errno));

	while (len > 0) {
		ret = sendfile(fd_dst, fd_src, NULL, len);	
		if (ret == 0)
			fatal("sendfile no enough data\n");
		else if (ret < 0)
			fatal("sendfile %s -> %s failed: %s\n", src, dst, strerror(errno));
		len -= ret;
	}

	close(fd_src);
	close(fd_dst);

}

static void ln(int srcfd, char *src, int dstfd, char *dst)
{
	char buf[1024];
	int ret;

	debug("Link %s\n", src);

	ret = readlinkat(srcfd, src, buf, sizeof(buf));
	if (ret < 0)
		fatal("read link '%s' failed: %s\n", src, strerror(errno));
	else if (ret >= sizeof(buf))
		fatal("read link '%s' overflow\n", src);
	buf[ret] = '\0';
	if (symlinkat(buf, dstfd, dst) < 0)
		fatal("symlink %s -> %s failed: %s\n", dst, buf, strerror(errno));
}

static void strip(char *src, char *dst)
{
	static char *stripprog = NULL;
	static char *stripopt = NULL;
	int pid, status, ret;

	if (!stripprog)
		stripprog = getenv("STRIPPROG");
	if (!stripopt) {
		stripopt = getenv("STRIPOPT");
		if (!stripopt)
			stripopt = "--strip-debug";
	}
	if (!stripprog)
		fatal("set STRIPPROG env to specify the strip program\n");

	debug("Strp %s\n", src);
	pid = fork();
	if (pid < 0)
		fatal("fork failed: %s\n", strerror(errno));
	else if (pid == 0) {
		char *argv[] = { stripprog, stripopt, "-o", dst, src, NULL };
		umask(022);
		if (execvp(argv[0], argv) < 0)
			fatal("execvp strip program %s failed: %s\n", argv[0], strerror(errno));
		exit(1);
	}
	ret = waitpid(pid, &status, 0);
	if (ret < 0)
		fatal("wait strip failed: %s\n", strerror(errno));
	if (ret != pid)
		fatal("wait return %d\n", ret);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		fatal("strip failed, status %d\n", WEXITSTATUS(status));
}

void make_dst(char *src, char *dir, char *dst, int dst_len)
{
	char *slash = strrchr(src, '/');
	if (!slash)
		slash = src;
	else {
		slash++;
		if (*slash == 0 || (*slash == '.' && slash[1] == 0)
			|| (*slash == '.' && slash[1] == '.' && slash[2] == 0))
			fatal("src %s is a directory path\n", src);
	}
	if (snprintf(dst, dst_len, "%s/%s", dir, slash) >= dst_len)
		fatal("make dest path overflow\n");
}

static void install(int srcfd, char *src, int dstfd, char *dst, unsigned mode, unsigned flags)
{
	struct stat stat_dst, stat_src;
	char _dst[1024];

	if (flags & KEEP_HIERARCHY && *src != '/') {
		if (snprintf(_dst, sizeof(_dst), "%s/%s", dst, src) >= sizeof(_dst))
			fatal("path too long\n");
		dst = _dst;
		mkdir_l(dst, 0755);
	}
	else if (flags & TODIR) {
		make_dst(src, dst, _dst, sizeof(_dst));
		dst = _dst;
	}

	if (fstatat(srcfd, src, &stat_src, AT_SYMLINK_NOFOLLOW) < 0)
		fatal("lstat %s failed: %s\n", src, strerror(errno));
	
	if (mode == 0)
		mode = stat_src.st_mode & 0777; 

	if (fstatat(dstfd, dst, &stat_dst, AT_SYMLINK_NOFOLLOW) == 0) {
		if (!(flags & FORCE)
				&& (stat_dst.st_mtim.tv_sec > stat_src.st_mtim.tv_sec
				 || (stat_dst.st_mtim.tv_sec == stat_src.st_mtim.tv_sec
					 && stat_dst.st_mtim.tv_nsec > stat_src.st_mtim.tv_nsec))) {
			debug("Skip %s\n", src);
			return;
		}
		unlinkat(dstfd, dst, 0);
	}

	if (S_ISREG(stat_src.st_mode)) {
		if (flags & STRIP)
			strip(src, dst);
		else
			cp(srcfd, src, dstfd, dst, stat_src.st_size, mode);
	}
	else if (S_ISLNK(stat_src.st_mode))
		ln(srcfd, src, dstfd, dst);
	else if (S_ISDIR(stat_src.st_mode))
		fprintf(stderr, "omitting directory '%s'\n", src);
	else
		fatal("src '%s': unsupported file type\n", src);
}

void batch_install(char *src, char *dst, int flags)
{
	int n = 0;
	struct {
		DIR *dir;
		int src, dst;
	} dirs[32];
	struct dirent *dirent;

	dirs[0].src = open(src, O_RDONLY|O_DIRECTORY);
	if (dirs[0].src < 0)
		fatal("open %s failed: %s\n", src, strerror(errno));

	dirs[0].dst = open(dst, O_RDONLY|O_DIRECTORY);
	if (dirs[0].dst < 0)
		fatal("open %s failed: %s\n", dst, strerror(errno));
	
	dirs[0].dir = fdopendir(dirs[0].src);
	if (dirs[0].dir == NULL)
		fatal("fdopendir failed: %s\n", strerror(errno));

	do {
		while ((dirent = readdir(dirs[n].dir))) {
			if (!strcmp(dirent->d_name, ".")
				|| !strcmp(dirent->d_name, ".."))
				continue;
			if (dirent->d_type == DT_UNKNOWN) {
				struct stat statbuf;
				if (fstatat(dirs[n].src, dirent->d_name, &statbuf, AT_SYMLINK_NOFOLLOW) < 0)
					fatal("fstatat %s failed: %s\n", dirent->d_name, strerror(errno));
				if (S_ISDIR(statbuf.st_mode))
					dirent->d_type = DT_DIR;
				else if (S_ISREG(statbuf.st_mode))
					dirent->d_type = DT_REG;
				else if (S_ISLNK(statbuf.st_mode))
					dirent->d_type = DT_LNK;
				else
					fatal("unsupported file type: %s\n", dirent->d_name);
			}
			if (dirent->d_type == DT_DIR) {
				struct stat statbuf;
				if (++n >= 32)
					fatal("path too depth\n");
				dirs[n].src = openat(dirs[n-1].src, dirent->d_name, O_RDONLY|O_DIRECTORY);
				if (dirs[n].src < 0)
					fatal("open %s failed: %s\n", dirent->d_name, strerror(errno));
				if (fstat(dirs[n].src, &statbuf) < 0)
					fatal("fstat %s failed: %s\n", dirent->d_name, strerror(errno));
				if (mkdirat(dirs[n-1].dst, dirent->d_name, statbuf.st_mode & 0777) == 0)
					debug("Mkdr %s/\n", dirent->d_name);
				dirs[n].dst = openat(dirs[n-1].dst, dirent->d_name, O_RDONLY|O_DIRECTORY);
				if (dirs[n].dst < 0)
					fatal("open %s failed: %s\n", dirent->d_name, strerror(errno));
				dirs[n].dir = fdopendir(dirs[n].src);
				if (dirs[n].dir == NULL)
					fatal("fdopendir failed: %s\n", strerror(errno));
				continue;
			}
			else if (dirent->d_type == DT_LNK || dirent->d_type == DT_REG)
				install(dirs[n].src, dirent->d_name, dirs[n].dst, dirent->d_name, 0, flags);
			else
				fatal("unsupported file type: %s\n", dirent->d_name);
		}
	} while (--n >= 0);
}

static int is_dir(char *file)
{
	struct stat statbuf;
	if (stat(file, &statbuf) < 0)
		return 0;
	return S_ISDIR(statbuf.st_mode);
}

static void usage()
{
	fprintf(stderr,
		"Xrouter install program.\n"
		"This INSTALL skip install if the timestamp of dest is newer\n"
		"\n"

		"install -d DIRECTORY ... \n"
		"    create all components of the given directory(ies)\n"
		"\n"

		"install [-D] [-T] SOURCE DEST\n"
		"    install SOURCE as DEST or child of existing DEST directory\n"
		"    -T, no target directory (force install as DEST)\n"
		"    -D, create leading component of DEST or all components if DEST end with /\n"
		"\n"

		"install [-D] SOURCE ... DEST\n"
		"    install multiple SOURCEs to DEST directory\n"
		"    -D, create all components of DEST\n"
		"\n"

		"install [-D] -t DEST SOURCE ... \n"
		"    install SOURCE to DEST directory\n"
		"    -D, create all components of DEST\n"
		"\n"

		"install [-D] -b SOURCE ... DEST\n"
		"    install all sub files in SOURCE to DEST (batch mode)\n"
		"    -D, create all components of DEST\n"
		"    this mode ignore -s, -m, -k option\n"
		"\n"

		"options:\n"
		"    -s, STRIP binary, need STRIPPROG env and optional STRIPOPT env\n"
		"    -f, force install\n"
		"    -m mode, file mode\n"
		"    -k, keep directory hierarchy\n"
		"    -v, enable debug output\n"
		"    -h, show this usage\n"
		"\n"
	);

	exit(1);
}

int main(int argc, char **argv)
{
	int opt;
	unsigned mode = 0755, flags = 0;
	char *target = NULL;
	int directory_mode = 0;
	int batch_mode = 0;
	int no_target_directory = 0;
	int create_dir = 0;
	int i;

	while ((opt = getopt(argc, argv, "sfkm:Dt:Tdbvh")) != -1 ) {
		switch(opt) {
		case 's': flags |= STRIP; break;
		case 'f': flags |= FORCE; break;
		case 'k': flags |= KEEP_HIERARCHY; break;
		case 'm': mode = strtoul(optarg, NULL, 8); break;
		case 'D': create_dir = 1; break;
		case 't': target = optarg; break;
		case 'T': no_target_directory = 1; break;
		case 'd': directory_mode = 1; break;
		case 'b': batch_mode = 1; break;
		case 'v': dbg_level++; break;
		case 'h': usage();
		default: fprintf(stderr, "invalid argument\n"); return 1;
		}
	}

	umask(0);

	/* -d directory ... */
	if (directory_mode) {
		for (i = optind; i < argc; i++)
			mkdir_p(argv[i], mode);
	}
	
	else if (batch_mode) {
		if (argc - optind < 2)
			fatal("batch mode: insufficient argument\n");
		if (create_dir)
			mkdir_p(argv[argc - 1], 0755);
		for (i = optind; i < argc - 1; i++)
			batch_install(argv[i], argv[argc - 1], flags & FORCE);
	}

	/* [-D] -t target_directory source ... */
	else if (target) {
		if (no_target_directory)
			fatal("can't combine -t and -T\n");
		if (argc - optind == 0)
			fatal("no source\n");
		if (create_dir)
			mkdir_p(target, 0755);
		for (i = optind; i < argc; i++)
			install(AT_FDCWD, argv[i], AT_FDCWD, target, mode, flags|TODIR);
	}

	/* [-T] src dst (dst may be file or directory) */
	else if (argc - optind == 2 ) {
		if (create_dir)
			mkdir_l(argv[optind+1], 0755);
		if (is_dir(argv[optind+1])) {
			if (no_target_directory)
				fatal("%s is a directory\n", argv[optind+1]);
			flags |= TODIR;
		}
		install(AT_FDCWD, argv[optind], AT_FDCWD, argv[optind+1], mode, flags);
	}
	
	else if (argc - optind > 2 ) {
		if (create_dir)
			mkdir_p(argv[argc - 1], 0755);
		flags |= TODIR;
		for (i = optind; i < argc - 1; i++)
			install(AT_FDCWD, argv[i], AT_FDCWD, argv[argc - 1], mode, flags);
	}

	else
		fatal("no source or dest\n");

	return 0;
}

