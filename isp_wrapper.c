/*********************************************************************
 *
 *   LPC ISP Commands
 *
 *
 *  Copyright (C) 2012 Nathael Pajani <nathael.pajani@nathael.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *********************************************************************/


#include <stdlib.h> /* strtoul */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h> /* strncmp */
#include <ctype.h>
#include <errno.h>

#include <unistd.h> /* for open, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "isp_commands.h"
#include "isp_utils.h"

extern int trace_on;


#define RAM_MAX_SIZE (8 * 1024) /* 8 KB */

#define REP_BUFSIZE 40



/*
 * read-memory
 * aruments : address count file uuencoded
 * read 'count' bytes from 'address', store then in 'file', and maybe decode uuencoded data.
 */
int isp_cmd_read_memory(int arg_count, char** args)
{
	/* Arguments */
	unsigned long int addr = 0, count = 0;
	char* out_file_name = NULL;
	/* Reply handling */
	char* data;
	int ret = 0, len = 0;
	unsigned int uuencoded = 1;

	/* Check read-memory arguments */
	if (arg_count < 3) {
		printf("read-memory command needs address and count. Both must be multiple of 4.\n");
		return -12;
	}
	addr = strtoul(args[0], NULL, 0);
	count = strtoul(args[1], NULL, 0);
	if (trace_on) {
		printf("read-memory command called for 0x%08lx (%lu) bytes at address 0x%08lx.\n",
				count, count, addr);
	}
	if ((addr & 0x03) || (count & 0x03)) {
		printf("Address and count must be multiple of 4 for read-memory command.\n");
		return -11;
	}
	out_file_name = args[2];
	if (arg_count > 3) {
		uuencoded = strtoul(args[3], NULL, 0);
	}

	/* Allocate buffer */
	data = malloc(count);
	if (data == NULL) {
		printf("Unable to allocate read buffer, asked %lu.\n", count);
		return -10;
	}

	/* Read data */
	len = isp_read_memory(data, addr, count, uuencoded);
	if (len != (int)count) {
		printf("Read returned %d bytes instead of %lu.\n", len, count);
	}

	/* Write data to file */
	ret = isp_buff_to_file(data, len, out_file_name);

	/* Free memory */
	free(data);

	return ret;
}

/*
 * write-to-ram
 * aruments : address file do_not_perform_uuencode
 * send 'file' to 'address' in ram with or without uuencoding data
 */
int isp_cmd_write_to_ram(int arg_count, char** args)
{
	/* Arguments */
	unsigned long int addr = 0;
	char* in_file_name = NULL;
	char file_buff[RAM_MAX_SIZE];
	unsigned int bytes_read = 0;
	int ret = 0;
	int uuencode = 1;

	/* Check write-to-ram arguments */
	if (arg_count < 2) {
		printf("write-to-ram command needs ram address. Must be multiple of 4.\n");
		return -15;
	}
	addr = strtoul(args[0], NULL, 0);
	if (trace_on) {
		printf("write-to-ram command called, destination address in ram is 0x%08lx.\n", addr);
	}
	if (addr & 0x03) {
		printf("Address must be multiple of 4 for write-to-ram command.\n");
		return -14;
	}
	in_file_name = args[1];
	if (arg_count > 2) {
		uuencode = strtoul(args[2], NULL, 0);
	}

	/* Read data */
	bytes_read = isp_file_to_buff(file_buff, RAM_MAX_SIZE, in_file_name);
	if (bytes_read <= 0){
		return -13;
	}

	if (trace_on) {
		printf("Read %d octet(s) from input file\n", bytes_read);
	}

	/* Pad data size to 4 */
	if (bytes_read & 0x03) {
		bytes_read = (bytes_read & ~0x03) + 0x04;
	}

	/* And send to ram */
	ret = isp_send_buf_to_ram(file_buff, addr, bytes_read, uuencode);

	return ret;
}

int isp_cmd_address_wrapper(int arg_count, char** args, char* name, char cmd)
{
	int ret = 0, len = 0;
	/* Arguments */
	unsigned long int addr1 = 0, addr2 = 0;
	unsigned long int length = 0;

	/* Check arguments */
	if (arg_count != 3) {
		printf("%s command needs two addresses and byte count.\n", name);
		return -7;
	}
	addr1 = strtoul(args[0], NULL, 0);
	addr2 = strtoul(args[1], NULL, 0);
	length = strtoul(args[2], NULL, 0);
	if (trace_on) {
		printf("%s command called for %lu bytes between addresses %lu and %lu.\n",
				name, length, addr1, addr2);
	}
	switch (cmd) {
		case 'M':
			if ((addr1 & 0x03) || (addr2 & 0x03) || (length & 0x03)) {
				printf("Error: addresses and count must be multiples of 4 for %s command.\n", name);
				return -7;
			}
			break;
		case 'C':
			if ((addr1 & 0x00FF) || (addr2 & 0x03)) {
				printf("Error: flash address must be on 256 bytes boundary, ram address on 4 bytes boundary.\n");
				return -8;
			}
			/* According to section 21.5.7 of LPC11xx user's manual (UM10398), number of bytes
			 * written should be 256 | 512 | 1024 | 4096 */
			len = ((length >> 8) & 0x01) + ((length >> 9) & 0x01);
			len += ((length >> 10) & 0x01) + ((length >> 12) & 0x01);
			if (len != 1) {
				printf("Error: bytes count must be one of 256 | 512 | 1024 | 4096\n");
				return -7;
			}
			break;
		default:
			printf("Error: unsupported command code: '%c'\n", cmd);
			return -6;
	}

	ret = isp_send_cmd_address(cmd, addr1, addr2, length, name);
	return ret;
}

int isp_cmd_compare(int arg_count, char** args)
{
	char buf[REP_BUFSIZE];
	int ret = 0, len = 0;
	unsigned long int offset = 0;

	ret = isp_cmd_address_wrapper(arg_count, args, "compare", 'M');
	switch (ret) {
		case CMD_SUCCESS:
			printf("Source and destination data are equal.\n");
			break;
		case COMPARE_ERROR:
			/* read remaining data */
			usleep( 2000 );
			len = isp_serial_read(buf, REP_BUFSIZE, 3);
			if (len <= 0) {
				printf("Error reading blank-check result.\n");
				return -3;
			}
			offset = strtoul(buf, NULL, 10);
			printf("First mismatch occured at offset 0x%08lx\n", offset);
			break;
		default :
			printf("Error for compare command.\n");
			return -1;
	}

	return 0;
}

int isp_cmd_copy_ram_to_flash(int arg_count, char** args)
{
	int ret = 0;

	ret = isp_cmd_address_wrapper(arg_count, args, "copy-ram-to-flash", 'C');

	if (ret != 0) {
		printf("Error when trying to copy data from ram to flash.\n");
		return ret;
	}
	printf("Data copied from ram to flash.\n");

	return 0;
}

int isp_cmd_go(int arg_count, char** args)
{
	int ret = 0;
	/* Arguments */
	unsigned long int addr = 0;
	char* mode = NULL;

	/* Check go arguments */
	if (arg_count != 2) {
		printf("go command needs address (> 0x200) and mode ('thumb' or 'arm').\n");
		return -7;
	}
	addr = strtoul(args[0], NULL, 0);
	mode = args[1];
	if (trace_on) {
		printf("go command called with address 0x%08lx and mode %s.\n", addr, mode);
	}
	if (addr < 0x200) {
		printf("Error: address must be 0x00000200 or greater for go command.\n");
		return -6;
	}
	if (strncmp(mode, "thumb", strlen(mode)) == 0) {
		mode[0] = 'T';
	} else if (strncmp(mode, "arm", strlen(mode)) == 0) {
		mode[0] = 'A';
	} else {
		printf("Error: mode must be 'thumb' or 'arm' for go command.\n");
		return -5;
	}

	ret = isp_send_cmd_go(addr, mode[0]);

	return ret;
}

int isp_cmd_sectors_skel(int arg_count, char** args, char* name, char cmd)
{
	int ret = 0;
	/* Arguments */
	unsigned long int first_sector = 0, last_sector = 0;

	/* Check arguments */
	if (arg_count != 2) {
		printf("%s command needs first and last sectors (1 sector = 4KB of flash).\n", name);
		return -7;
	}
	first_sector = strtoul(args[0], NULL, 0);
	last_sector = strtoul(args[1], NULL, 0);
	if (trace_on) {
		printf("%s command called for sectors %lu to %lu.\n", name, first_sector, last_sector);
	}

	ret = isp_send_cmd_sectors(name, cmd, first_sector, last_sector, 0);

	return ret;
}

int isp_cmd_blank_check(int arg_count, char** args)
{
	char* tmp = NULL;
	unsigned long int offset = 0, content = 0;
	char buf[REP_BUFSIZE];
	int ret = 0, len = 0;

	ret = isp_cmd_sectors_skel(arg_count, args, "blank-check", 'I');
	if (ret < 0) {
		return ret;
	}

	switch (ret) {
		case CMD_SUCCESS:
			printf("Specified sector(s) all blank(s).\n");
			break;
		case SECTOR_NOT_BLANK:
			/* read remaining data */
			usleep( 2000 );
			len = isp_serial_read(buf, REP_BUFSIZE, 3);
			if (len <= 0) {
				printf("Error reading blank-check result.\n");
				return -3;
			}
			offset = strtoul(buf, &tmp, 10);
			content = strtoul(tmp, NULL, 10);
			printf("First non blank word is at offset 0x%08lx and contains 0x%08lx\n", offset, content);
			break;
		case INVALID_SECTOR :
			printf("Invalid sector for blank-check command.\n");
			return -2;
		case PARAM_ERROR:
			printf("Param error for blank-check command.\n");
			return -1;
	}

	return 0;
}

int isp_cmd_prepare_for_write(int arg_count, char** args)
{
	int ret = isp_cmd_sectors_skel(arg_count, args, "prepare-for-write", 'P');

	if (ret != 0) {
		printf("Error when trying to prepare sectors for write operation.\n");
		return ret;
	}
	printf("Sectors prepared for write operation.\n");

	return 0;
}

int isp_cmd_erase(int arg_count, char** args)
{
	int ret = isp_cmd_sectors_skel(arg_count, args, "erase", 'E');

	if (ret != 0) {
		printf("Error when trying to erase sectors.\n");
		return ret;
	}
	printf("Sectors erased.\n");

	return 0;
}



