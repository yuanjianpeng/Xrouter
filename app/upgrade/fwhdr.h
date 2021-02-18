/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef FWHDR_H
#define FWHDR_H

#include <stdint.h>

/* 
 * upgrade firmware Format / Layout
 *
 *    +-----------------------+
 *    |    FIRMWARE HEADER    |
 *    +-----------------------+ <- every section is aligned to 4 bytes
 *    |    Partition Image 0  |
 *    +-----------------------+
 *    |    Partition ...      |
 *    +-----------------------+
 *    |    FILE 0             |
 *    +-----------------------+
 *    |    FILE ...           |
 *    +-----------------------+
 *
 * sections are aligned to 4 bytes, so there may be paddings between sections
 * paddings are not part of section and are not counted into the length of section.
 * There's no paddings after last section. paddings are filled with 0xff.
 *
 * All header members are stored as little-endian.
 *
 */

#define MAX_PARTS	32
#define MAX_FILES	32

#define FW_MGC				"\177XRF"
#define FW_VER				0x1		/* Increase the Version number, if the format changed */
#define FW_FLAG_SHA256		0x1
#define FW_FLAG_RSA_SIGN	0x2
#define FW_DIGEST_SZ		32
#define FW_SIGN_SZ			256

struct fwhdr
{
	uint8_t magic[4];		/* firmware magic */
	uint32_t fwver;			/* firmware format version */
	char swname[32];		/* Software name */
	uint32_t swver;			/* software version */
	uint32_t buildtime;		/* software build time */
	uint32_t hwver[64];		/* suppported hardware version */
	uint32_t parts;			/* number of partitions */
	uint32_t files;			/* number of files */
	uint32_t flags;			/* firmware flags, e.g. sha256 checksumed, rsa signed */
	uint8_t digest[FW_DIGEST_SZ];		/* sha256 digest */
	uint8_t sign[FW_SIGN_SZ];			/* RSA signature */
};

struct fwpart
{
	char part_name[32];
	uint32_t part_off;
	uint32_t part_size;
	uint32_t image_size;
	char image[0];
};

struct fwfile
{
	char file_name[64];
	uint32_t file_len;
	char file[0];
};

#endif

