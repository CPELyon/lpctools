/*********************************************************************
 *
 *   LPC ISP Commands for flash tool
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
#include <stdio.h> /* printf, snprintf */
#include <stdint.h>
#include <unistd.h>
#include <string.h> /* strncmp, memset */
#include <ctype.h>
#include <errno.h>

#include <unistd.h> /* for open, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "isp_utils.h"
#include "isp_commands.h"
#include "parts.h"

#define REP_BUFSIZE 40

extern int trace_on;


int get_ids(void)
{
	int ret = 0;

	isp_cmd_part_id(0);
	isp_cmd_read_uid();
	isp_cmd_boot_version();

	return ret;
}


int dump_to_file(struct part_desc* part, char* filename)
{
	int ret = 0, len = 0;
	char* data;

	/* Allocate buffer */
	data = malloc(part->flash_size);
	if (data == NULL) {
		printf("Unable to allocate read buffer, asked %u.\n", part->flash_size);
		return -10;
	}

	/* Read data */
	len = isp_read_memory(data, part->flash_base, part->flash_size, part->uuencode);
	if (len != (int)(part->flash_size)) {
		printf("Read returned %d bytes instead of %u.\n", len, part->flash_size);
	}

	/* Write data to file */
	ret = isp_buff_to_file(data, part->flash_size, filename);

	/* Free memory */
	free(data);

	return ret;
}


int erase_flash(struct part_desc* part)
{
	int ret = 0;
	int i = 0;

	/* Unlock device */
	ret = isp_cmd_unlock(1);
	if (ret != 0) {
		printf("Unable to unlock device, aborting.\n");
		return -1;
	}

	for (i=0; i<(int)(part->flash_nb_sectors); i++) {
		ret = isp_send_cmd_sectors("blank-check", 'I', i, i, 1);
		if (ret == CMD_SUCCESS) {
			/* sector already blank, preserve the flash, skip to next one :) */
			continue;
		}
		if (ret < 0) {
			/* Error ? */
			printf("Initial blank check error (%d) at sector %d!\n", ret, i);
			return ret;
		} else {
			/* Controller replyed with first non blank offset and data, remove it from buffer */
			char buf[REP_BUFSIZE];
			usleep( 5000 ); /* Some devices are slow to scan flash, give them some time */
			isp_serial_read(buf, REP_BUFSIZE, 3);
		}
		/* Sector not blank, perform erase */
		ret = isp_send_cmd_sectors("prepare-for-write", 'P', i, i, 1);
		if (ret != 0) {
			printf("Error (%d) when trying to prepare sector %d for erase operation!\n", ret, i);
			return ret;
		}
		ret = isp_send_cmd_sectors("erase", 'E', i, i, 1);
		if (ret != 0) {
			printf("Error (%d) when trying to erase sector %d!\n", ret, i);
			return ret;
		}
	}
	printf("Flash now all blank.\n");

	return 0;
}

int start_prog(struct part_desc* part)
{
	int ret = 0, len = 0;
	uint32_t addr = 0;

	len = isp_read_memory((char*)&addr, (part->flash_base + part->reset_vector_offset), sizeof(addr), part->uuencode);
	if (len != sizeof(addr)) {
		printf("Unable to read reset address from flash.\n");
		return len;
	}
	/* FIXME : the following value (0x200) may be LPC111x specific */
	if (addr < 0x200) {
		printf("Actual reset address is 0x%08x, which is under the lowest allowed address of 0x200.\n", addr);
	}
	/* Unlock device */
	ret = isp_cmd_unlock(1);
	if (ret != 0) {
		printf("Unable to unlock device, aborting.\n");
		return -1;
	}
	/* Read address in thumb or arm mode ? */
	if (addr & 0x01) {
		addr &= ~1UL;
		ret = isp_send_cmd_go(addr, 'T');
	} else {
		ret = isp_send_cmd_go(addr, 'A');
	}

	/* FIXME : start terminal */

	return ret;
}


static unsigned int calc_write_size(unsigned int sector_size, unsigned int ram_buff_size)
{
	unsigned int write_size = 0;

	write_size = ((sector_size < ram_buff_size) ? sector_size : ram_buff_size);
	/* According to section 21.5.7 of LPC11xx user's manual (UM10398), number of bytes
	 * written should be 256 | 512 | 1024 | 4096 */
	if (write_size >= 4096) {
		write_size = 4096;
	} else if (write_size >= 1024) {
		write_size = 1024;
	} else if (write_size >= 512) {
		write_size = 512;
	} else if (write_size >= 256) {
		write_size = 256;
	} else if (write_size >= 64) {
		write_size = 64;
	} else {
		write_size = 0;
	}
	return write_size;
}

int flash_target(struct part_desc* part, char* filename, int calc_user_code)
{
	int ret = 0;
	char* data = NULL;
	int size = 0;
	int i = 0, blocks = 0;
	unsigned int write_size = 0;
	unsigned int sector_size = (part->flash_size / part->flash_nb_sectors);
	uint32_t ram_addr = (part->ram_base + part->ram_buff_offset);
	uint32_t uuencode = part->uuencode;
	uint32_t* v = NULL; /* Used for checksum computing */
	uint32_t cksum = 0;
	uint32_t crp = 0;

	/**  Sanity checks  *********************************/
	/* RAM buffer address within RAM */
	if (ram_addr > (part->ram_base + part->ram_size)) {
		printf("Invalid configuration, asked to use buffer out of RAM, aborting.\n");
		return -1;
	}
	/* Calc write block size */
	write_size = calc_write_size(sector_size, part->ram_buff_size);
	if (write_size == 0) {
		printf("Config error, I cannot flash using blocks of nul size !\nAborted.\n");
		return -2;
	}

	/* Just make sure flash is erased */
	ret = erase_flash(part);
	if (ret != 0) {
		printf("Unable to erase device, aborting.\n");
		return -3;
	}

	/* Allocate a buffer as big as the flash */
	data = malloc(part->flash_size);
	if (data == NULL) {
		printf("Unable to get a buffer to load the image!");
		return -4;
	}
	/* And fill the buffer with the image */
	size = isp_file_to_buff(data, part->flash_size, filename);
	if (size <= 0){
		free(data);
		return -5;
	}
	/* Fill unused buffer with 0's so we can flash blocks of data of "write_size" */
	memset(&data[size], 0, (part->flash_size - size));
	/* And check checksum of first 7 vectors if asked, according to section 21.3.3 of
	 * LPC11xx user's manual (UM10398) */
	v = (uint32_t *)data;
	cksum = 0 - v[0] - v[1] - v[2] - v[3] - v[4] - v[5] - v[6];
	if (calc_user_code == 1) {
		v[7] = cksum;
	} else if (cksum != v[7]) {
		printf("Checksum is 0x%08x, should be 0x%08x\n", v[7], cksum);
		free(data);
		return -5;
	}
	printf("Checksum check OK\n");

	crp = v[(CRP_OFFSET / 4)];
	if ((crp == CRP_NO_ISP) || (crp == CRP_CRP1) || (crp == CRP_CRP2) || (crp == CRP_CRP3)) {
		printf("CRP : 0x%08x\n", crp);
		printf("The binary has CRP protection ativated, which violates GPLv3.\n");
		printf("Check the licence for the software you are using, and if this is allowed,\n");
		printf(" then modify this software to allow flashing of code with CRP protection\n");
		printf(" activated. (Or use another software).\n");
		return -6;
	}

	blocks = (size / write_size) + ((size % write_size) ? 1 : 0);
	/* Gonna write out of flash ? */
	if ((blocks * write_size) > part->flash_size) {
		printf("Config error, I cannot flash beyond end of flash !\n");
		printf("Flash size : %d, trying to flash %d blocks of %d bytes : %d\n",
				part->flash_size, blocks, write_size, (blocks * write_size));
		free(data);
		return -7;
	}
	printf("Flash size : %d, trying to flash %d blocks of %d bytes : %d\n",
			part->flash_size, blocks, write_size, (blocks * write_size));

	/* Now flash the device */
	printf("Writing started, %d blocks of %d bytes ...\n", blocks, write_size);
	for (i=0; i<blocks; i++) {
		unsigned int current_sector = (i * write_size) / sector_size;
		uint32_t flash_addr = part->flash_base + (i * write_size);
		/* Prepare sector for writting (must be done before each write) */
		ret = isp_send_cmd_sectors("prepare-for-write", 'P', current_sector, current_sector, 1);
		if (ret != 0) {
			printf("Error (%d) when trying to prepare sector %d for erase operation!\n", ret, i);
			free(data);
			return ret;
		}
		/* Send data to RAM */
		ret = isp_send_buf_to_ram(&data[i * write_size], ram_addr, write_size, uuencode);
		if (ret != 0) {
			printf("Unable to perform write-to-ram operation for block %d (block size: %d)\n",
					i, write_size);
			free(data);
			return ret;
		}
		/* Copy from RAM to FLASH */
		ret = isp_send_cmd_address('C', flash_addr, ram_addr, write_size, "write_to_ram");
		if (ret != 0) {
			printf("Unable to copy data to flash for block %d (block size: %d)\n", i, write_size);
			free(data);
			return ret;
		}
	}

	free(data);
	return ret;
}

