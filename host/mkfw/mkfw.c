#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "fwhdr.h"

struct fw
{
	struct fwhdr hdr;

	struct {
		struct fwpart part;
		char *image;
	} parts[32];

	struct {
		struct fwfile file;
		char *path;
	} files[128];

#define SEC_PART 1
#define SEC_FILE 2
	int part_no, file_no, section;
	int hwver_no;
};

static struct fw fw;

static void usage()
{
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

static int parse_key(char *name, char *value)
{
	if (value == NULL) {
		if (!strcmp(name, "partition")) {
			fw.part_no++;
			fw.section = SEC_PART;
			return 0;
		}
		else if (!strcmp(name, "file")) {
			fw.section = SEC_FILE;
			return 0;
		}

	}

	else if (fw.section == 0) {
		if (!strcmp(name, "software version")) {
			fw.hdr.swver = parse_softver(value);
			if (fw.hdr.swver == 0) {
				fprintf(stderr, "invalid softver\n");
				return -1;
			}
			return 0;
		}
		else if (!strcmp(name, "compatible hardware versions")) {
			char *ver = strtok(value, " ");
			while (ver) {
				if (fw.hwver_no >= sizeof(fw.hdr.hwver)/sizeof(fw.hdr.hwver[0])) {
					fprintf(stderr, "no many hardware versions\n");
					return -1;
				}
				fw.hdr.hwver[fw.hwver_no] = parse_softver(ver);
				if (fw.hdr.hwver[fw.hwver_no] == 0) {
					fprintf(stderr, "invalid hardware version\n");
					return -1;
				}
				fw.hwver_no++;
				ver = strtok(NULL, " ");
			}
			return 0;
		}
	}
	else if (fw.section == SEC_PART) {
		if (!strcmp(name, "name")) {
			if (strlen(value) == 0 || strlen(value) >= sizeof(fw.parts[0].part.part_name)) {
				fprintf(stderr, "part name too long or empty\n");
				return -1;
			}
			strcpy(fw.parts[fw.part_no - 1].part.part_name, name);
			return 0;
		}
		else if (!strcmp(name, "offset")) {
			fw.parts[fw.part_no - 1].part.part_off = get_num(value);
			return 0;
		}
		else if (!strcmp(name, "size")) {
			fw.parts[fw.part_no - 1].part.part_size = get_num(value);
			return 0;
		}
		else if (!strcmp(name, "image")) {
			fw.parts[fw.part_no - 1].image = strdup(value);
			return 0;
		}
	}
	else if (fw.section = SEC_FILE) {
	}

	fprintf(stderr, "unknown config %s\n", name);
	return -1;
}

static int parse_cfg(const char *cfg)
{
	char buf[1024];
	FILE *f = fopen(cfg, "r");
	if (f == NULL) {
		fprintf(stderr, "read cfg: %s failed: %s\n", cfg, strerror(errno));
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		char *tmp = buf;
		char c;
		char *name, *value;

		while ((c = *tmp++))
			if (c == '\r' || c == '\n') {
				tmp[-1]= '\0';
				break;
			}
		
		c = buf[0];
		if (c == 0 || c == ' ' || c == '\t' || c == '#')
			continue;

		if (c == '[') {
			tmp = strchr(buf, ']');
			if (!tmp)
				return -1;
			*tmp = '\0';
			name = buf + 1;
			value = NULL;
		}
		else {
			tmp = strchr(buf, '=');
			if (!tmp)
				return -1;
			*tmp = '\0';
			name = buf;
			value = tmp + 1;
			while (*--tmp == ' ');
			tmp[1] = '\0';
			while (*value == ' ')
				value++;
		}
		if (parse_key(name, value) < 0)
			return -1;
	}

	fclose(f);
	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	char *cfg = NULL;
	uint32_t softver = 0;
	char *image = NULL;

	while ((opt = getopt(argc, argv, "c:v:h:")) != -1) {
		switch (opt) {
		case 'c':
			cfg = optarg;
			break;
		case 'v':
			softver = parse_softver(optarg);
			if (softver == 0) {
				fprintf(stderr, "invalid soft version\n");
				return 1;
			}
			break;
		default:
			fprintf(stderr, "invalid option\n");
		case 'h':
			usage();
		}
	}

	if (cfg == NULL) {
		fprintf(stderr, "require config\n");
		usage();
	}
	if (parse_cfg(cfg) < 0) {
		fprintf(stderr, "config format error\n");
		return 1;
	}

	return 0;
}

