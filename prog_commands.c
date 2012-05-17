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
	int ret = 0;
	int argc = 3;
	char* args[3];
	#define STR_SIZE 15
	char flash_size[STR_SIZE];

	args[0] = "0x00000000";
	snprintf(flash_size, STR_SIZE, "0x%08x", part->flash_size); /* Get count from part ID */
	args[1] = flash_size;
	args[2] = filename;

	isp_cmd_read_memory(argc, args);
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

