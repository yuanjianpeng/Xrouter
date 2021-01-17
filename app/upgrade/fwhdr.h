/*
 * Copyright (C) 2020 Yuan Jianpeng <yuan89@163.com>
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

#ifndef FWHDR_H
#define FWHDR_H

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
 * All header members are stored as little-endian
 */

#define FW_MGC	"\177XRF"

/* increase the Version number,
   every time you changed the format */
#define FW_VER	0x1

#define FW_FLAG_SHA256		0x1
#define FW_FLAG_RSA_SIGN	0x2

struct fwhdr
{
	uint8_t magic[4];
	uint32_t fw_ver;		/* firmware format version */
	uint32_t flags;
	uint32_t swver;
	uint32_t hwver[64];
	uint32_t parts;			/* number of partitions */
	uint32_t files;			/* number of files */
	uint8_t digest[32];
	uint8_t sign[2048>>3];
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

