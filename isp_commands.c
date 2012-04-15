/*********************************************************************
 *
 *   LPC1114 ISP Commands
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 * 
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/


#include <stdlib.h> /* strtoul */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h> /* strncmp */
#include <ctype.h>
#include <errno.h>

#include "isp_utils.h"

extern int trace_on;

struct isp_command {
	char* name;
	int nb_args;
	int (*handler)(int arg_count, char** args);
};


#define SERIAL_BUFSIZE  128

#define SYNCHRO_START "?"
#define SYNCHRO  "Synchronized\r\n"
#define SYNCHRO_OK "OK\r\n"
#define SYNCHRO_ECHO_OFF "A 0\r\n"

#define ISP_ABORT ""

#define UNLOCK "U 23130\r\n"
#define READ_UID "N\r\n"
#define READ_PART_ID "J\r\n"
#define READ_BOOT_VERSION "K\r\n"


char* error_codes[] = {
	"CMD_SUCCESS",
	"INVALID_COMMAND",
	"SRC_ADDR_ERROR",
	"DST_ADDR_ERROR",
	"SRC_ADDR_NOT_MAPPED",
	"DST_ADDR_NOT_MAPPED",
	"COUNT_ERROR",
	"INVALID_SECTOR",
	"SECTOR_NOT_BLANK",
	"SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION",
	"COMPARE_ERROR",
	"BUSY",
	"PARAM_ERROR",
	"ADDR_ERROR",
	"ADDR_NOT_MAPPED",
	"CMD_LOCKED",
	"INVALID_CODE",
	"INVALID_BAUD_RATE",
	"INVALID_STOP_BIT",
	"CODE_READ_PROTECTION_ENABLED",
};

int isp_ret_code(char* buf, char** endptr)
{
	unsigned int ret = 0;
	ret = strtoul(buf, endptr, 10);
	/* FIXME : Find how return code are sent (binary or ASCII) */
	if (ret > (sizeof(error_codes)/sizeof(char*))) {
		printf("Received unknown error code '%u' !\n", ret);
	} else if ((ret != 0) || trace_on) {
		printf("Received error code '%u': %s\n", ret, error_codes[ret]);
	}
	return ret;
}


/* Connect or reconnect to the target.
 * Return positive or NULL value when connection is OK, or negative value otherwise.
 */
int isp_connect(unsigned int crystal_freq)
{
	char buf[SERIAL_BUFSIZE];
	char freq[10];
	
	snprintf(freq, 8, "%d\r\n", crystal_freq);

	/* Send synchronize request */
	if (isp_serial_write(SYNCHRO_START, strlen(SYNCHRO_START)) != strlen(SYNCHRO_START)) {
		printf("Unable to send synchronize request.\n");
		return -5;
	}
	/* Wait for answer */
	if (isp_serial_read(buf, SERIAL_BUFSIZE, strlen(SYNCHRO)) < 0) {
		printf("Error reading synchronize answer.\n");
		return -4;
	}
	/* Check answer, and acknowledge if OK */
	if (strncmp(SYNCHRO, buf, strlen(SYNCHRO)) == 0) {
		isp_serial_write(SYNCHRO, strlen(SYNCHRO));
	} else {
		printf("Unable to synchronize, no synchro received.\n");
		return -3;
	}
	/* Empty read buffer (echo is on) */
	isp_serial_read(buf, strlen(SYNCHRO), strlen(SYNCHRO));
	/* Read reply (OK) */
	isp_serial_read(buf, SERIAL_BUFSIZE, strlen(SYNCHRO_OK));
	if (strncmp(SYNCHRO_OK, buf, strlen(SYNCHRO_OK)) != 0) {
		printf("Unable to synchronize, synchro not acknowledged.\n");
		return -2;
	}

	/* Documentation says we should send crystal frequency .. sending anything is OK */
	isp_serial_write(freq, strlen(freq));
	/* Empty read buffer (echo is on) */
	isp_serial_read(buf, strlen(freq), strlen(freq));
	/* Read reply (OK) */
	isp_serial_read(buf, SERIAL_BUFSIZE, strlen(SYNCHRO_OK));
	if (strncmp(SYNCHRO_OK, buf, strlen(SYNCHRO_OK)) != 0) {
		printf("Unable to synchronize, crystal frequency not acknowledged.\n");
		return -2;
	}

	/* Turn off echo */
	isp_serial_write(SYNCHRO_ECHO_OFF, strlen(SYNCHRO_ECHO_OFF));
	/* Empty read buffer (echo still on) */
	isp_serial_read(buf, strlen(SYNCHRO_ECHO_OFF), strlen(SYNCHRO_ECHO_OFF));
	/* Read eror code for command */
	isp_serial_read(buf, SERIAL_BUFSIZE, 3);

	printf("Device session openned.\n");

	return 1;
}

int isp_cmd_unlock(int arg_count, char** args)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	
	/* Send read-uid request */
	if (isp_serial_write(UNLOCK, strlen(UNLOCK)) != strlen(UNLOCK)) {
		printf("Unable to send unlock request.\n");
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, SERIAL_BUFSIZE, 1);
	if (len <= 0) {
		printf("Error reading unlock acknowledge.\n");
		return -4;
	}
	ret = isp_ret_code(buf, NULL);
	if (ret != 0) {
		printf("Unlock error.\n");
		return -1;
	}
	printf("Device memory protection unlocked.\n");

	return 0;
}

int isp_cmd_read_uid(int arg_count, char** args)
{
	char buf[SERIAL_BUFSIZE];
	char* tmp = NULL;
	int i = 0, ret = 0, len = 0;
	unsigned long int uid[4];
	
	/* Send read-uid request */
	if (isp_serial_write(READ_UID, strlen(READ_UID)) != strlen(READ_UID)) {
		printf("Unable to send read-uid request.\n");
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, SERIAL_BUFSIZE, 50);
	if (len <= 0) {
		printf("Error reading uid.\n");
		return -4;
	}
	ret = isp_ret_code(buf, &tmp);
	if (ret != 0) {
		printf("This cannot happen ... as long as you trust the user manual.\n");
		return -1;
	}
	for (i=0; i<4; i++) {
		static char* endptr = NULL;
		uid[i] = strtoul(tmp, &endptr, 10);
		tmp = endptr;
	}
	printf("UID: 0x%08lx - 0x%08lx - 0x%08lx - 0x%08lx\n", uid[0], uid[1], uid[2], uid[3]);

	return 0;
}

int isp_cmd_part_id(int arg_count, char** args)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	char* tmp = NULL;
	unsigned long int part_id = 0;

	/* Send read-part-id request */
	if (isp_serial_write(READ_PART_ID, strlen(READ_PART_ID)) != strlen(READ_PART_ID)) {
		printf("Unable to send read-part-id request.\n");
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, SERIAL_BUFSIZE, 10);
	if (len <= 0) {
		printf("Error reading part id.\n");
		return -4;
	}
	ret = isp_ret_code(buf, &tmp);
	if (ret != 0) {
		printf("This cannot happen ... as long as you trust the user manual.\n");
		return -1;
	}
	/* FIXME : some part IDs are on two 32bits values */
	part_id = strtoul(tmp, NULL, 10);
	printf("Part ID is 0x%08lx\n", part_id);

	return 0;
}

int isp_cmd_boot_version(int arg_count, char** args)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	char* tmp = NULL;
	unsigned int ver[2];

	/* Send read-boot-version request */
	if (isp_serial_write(READ_BOOT_VERSION, strlen(READ_BOOT_VERSION)) != strlen(READ_BOOT_VERSION)) {
		printf("Unable to send read-boot-version request.\n");
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, SERIAL_BUFSIZE, 5);
	if (len <= 0) {
		printf("Error reading boot version.\n");
		return -4;
	}
	ret = isp_ret_code(buf, &tmp);
	if (ret != 0) {
		printf("This cannot happen ... as long as you trust the user manual.\n");
		return -1;
	}
	ver[0] = strtoul(tmp, &tmp, 10);
	ver[1] = strtoul(tmp, NULL, 10);
	printf("Boot code version is %u.%u\n", ver[0], ver[1]);

	return 0;
}


/* FIXME : Temporary place-holder */
int isp_cmd_null(int arg_count, char** args)
{
	int i = 0;
	printf("command not yet handled, called with %d arguments.\n", arg_count);
	for (i=0; i<arg_count; i++) {
		printf("\targ[%d] : \"%s\"\n", i, args[i]);
	}
	return 0;
}

static struct isp_command isp_cmds_list[] = {
	{"unlock", 0, isp_cmd_unlock},
	{"write-to-ram", 0, isp_cmd_null}, /* isp_cmd_write-to-ram} */
	{"read-memory", 0, isp_cmd_null}, /* isp_cmd_read-memory} */
	{"prepare-for-write", 0, isp_cmd_null}, /* isp_cmd_prepare-for-write} */
	{"copy-ram-to-flash", 0, isp_cmd_null}, /* isp_cmd_copy-ram-to-flash} */
	{"go", 0, isp_cmd_null}, /* isp_cmd_go} */
	{"erase", 0, isp_cmd_null}, /* isp_cmd_erase} */
	{"blank-check", 0, isp_cmd_null}, /* isp_cmd_blank-check} */
	{"read-part-id", 0, isp_cmd_part_id},
	{"read-boot-version", 0, isp_cmd_boot_version},
	{"compare", 0, isp_cmd_null}, /* isp_cmd_compare} */
	{"read-uid", 0, isp_cmd_read_uid},
	{NULL, 0, NULL}
};

void isp_warn_trailing_args(int cmd_num, int arg_count, char** args)
{
	int i = 0;
	printf("command \"%s\" needs %d args, got %d.\n",
			isp_cmds_list[cmd_num].name,
			isp_cmds_list[cmd_num].nb_args, arg_count);
	for (i=0; i<arg_count; i++) {
		printf("\targ[%d] : \"%s\"\n", i, args[i]);
	}
}

/* Handle one command
 * Return positive or NULL value when command handling is OK, or negative value otherwise.
 */
int isp_handle_command(char* cmd, int arg_count, char** args)
{
	int cmd_found = -1;
	int ret = 0;
	int index = 0;

	if (cmd == NULL) {
		printf("isp_handle_command called with no command !\n");
		return -1;
	}

	while ((cmd_found == -1) && (isp_cmds_list[index].name != NULL)) {
		if (strncmp(isp_cmds_list[index].name, cmd, strlen(isp_cmds_list[index].name)) == 0) {
			cmd_found = index;
			break;
		}
		index++;
	}
	if (arg_count != isp_cmds_list[cmd_found].nb_args) {
		isp_warn_trailing_args(cmd_found, arg_count, args);
	}
	isp_cmds_list[cmd_found].handler(arg_count, args);
	
	if (cmd_found == -1) {
		printf("Unknown command \"%s\", use -h or --help for a list.\n", cmd);
		return -2;
	}

	return ret;
}


