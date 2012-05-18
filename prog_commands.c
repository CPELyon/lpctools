/*********************************************************************
 *
 *   LPC1114 ISP Commands for flash tool
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 *
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/

#include <stdlib.h> /* strtoul */
#include <stdio.h> /* printf, snprintf */
#include <stdint.h>
#include <unistd.h>
#include <string.h> /* strncmp */
#include <ctype.h>
#include <errno.h>

#include <unistd.h> /* for open, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "isp_utils.h"
#include "isp_commands.h"
#include "parts.h"

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
	len = isp_read_memory(data, part->flash_base, part->flash_size);
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


	return ret;
}

int start_prog(struct part_desc* part)
{
	int ret = 0, len = 0;
	uint32_t addr = 0;

	len = isp_read_memory((char*)&addr, (part->flash_base + part->reset_vector_offset), sizeof(addr));
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


int flash_target(struct part_desc* part, char* filename, int check)
{
	int ret = 0;

	return ret;
}

