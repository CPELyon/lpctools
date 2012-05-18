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

int start_prog(void)
{
	int ret = 0;

	return ret;
}


int flash_target(struct part_desc* part, char* filename, int check)
{
	int ret = 0;

	return ret;
}

