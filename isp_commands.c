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

#include <unistd.h> /* for open, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "isp_utils.h"

extern int trace_on;

struct isp_command {
	char* name;
	int nb_args;
	int (*handler)(int arg_count, char** args);
};


/* Max should be 1270 for read memory, a little bit more for writes */
#define SERIAL_BUFSIZE  1300

#define SYNCHRO_START "?"
#define SYNCHRO  "Synchronized\r\n"
#define SYNCHRO_OK "OK\r\n"
#define DATA_BLOCK_OK "OK\r\n"
#define DATA_BLOCK_RESEND "RESEND\r\n"
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

int isp_cmd_read_memory(int arg_count, char** args)
{
	/* Serial communication */
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	/* Arguments */
	unsigned long int addr = 0, count = 0;
	char* out_file_name = NULL;
	int out_fd = -1;
	/* Reply handling */
	unsigned int lines = 0, blocks = 0;
	unsigned int total_bytes_received = 0;
	unsigned int i = 0;

	/* Check read-memory arguments */
	if (arg_count != 3) {
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
	
	/* Create read-memory request */
	len = snprintf(buf, SERIAL_BUFSIZE, "R %lu %lu\r\n", addr, count);
	if (len > SERIAL_BUFSIZE) {
		len = SERIAL_BUFSIZE; /* This should not happen if SERIAL_BUFSIZE is 32 or more ... */
	}

	/* Send read-memory request */
	if (isp_serial_write(buf, len) != len) {
		printf("Unable to send read-memory request.\n");
		return -10;
	}
	/* And receive and decode the answer */
	usleep( 2000 );
	/* First, check return code. This will also remove the first 3 caracters from the
     * reply so we get only interresting caracters in the buffer. */
	len = isp_serial_read(buf, 3, 3);
	if (len <= 0) {
		printf("Error reading memory.\n");
		return -9;
	}
	ret = isp_ret_code(buf, NULL);
	if (ret != 0) {
		printf("Error when trying to read %lu bytes of memory at address 0x%08lx.\n", count, addr);
		return -8;
	}

	/* Now, find the number of lines and the length of the reply.
	 * See section 21.4.3 of LPC11xx user's manual (UM10398) */
#define LINE_DATA_LENGTH 45
#define LINES_PER_BLOCK 20
#define MAX_DATA_BLOCK_SIZE (LINES_PER_BLOCK * LINE_DATA_LENGTH) /* 900 */
#define MAX_BYTES_PER_LINE (((LINE_DATA_LENGTH / 3) * 4) + 1 + 2) /* 60 + "line length" + \r\n */
#define FILE_CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP)
	lines = (count / LINE_DATA_LENGTH);
	if (count % LINE_DATA_LENGTH) {
		lines += 1;
	}
	blocks = (lines / LINES_PER_BLOCK);
	if (lines % LINES_PER_BLOCK) {
		blocks += 1;
	}
	if (trace_on) {
		printf("Reading %d block(s) (%d line(s)).\n", blocks, lines);
	}
	/* Open file for writing if output is not stdout */
	if (strncmp(out_file_name, "-", strlen(out_file_name)) == 0) {
		out_fd = STDOUT_FILENO;
	} else {
		out_fd = open(out_file_name, (O_WRONLY | O_CREAT | O_TRUNC), FILE_CREATE_MODE);
		if (out_fd <= 0) {
			perror("Unable to open or create file for writing");
			printf("Tried to open \"%s\".\n", out_file_name);
			return -7;
		}
	}
	
	ret = 0;
	/* Receive and decode the data */
	for (i=0; i<blocks; i++) {
		char data[SERIAL_BUFSIZE];
		unsigned int blocksize = 0, decoded_size = 0;
		unsigned int received_checksum = 0, computed_checksum = 0;
		static int resend_request_for_block = 0;
		unsigned int j = 0;

		/* Read the uuencoded data */
		if ((count - total_bytes_received) >= MAX_DATA_BLOCK_SIZE) {
			blocksize = (MAX_BYTES_PER_LINE * LINES_PER_BLOCK);
			if (trace_on) {
				printf("Reading block %d (%d line(s)).\n", i, LINES_PER_BLOCK);
			}
		} else {
			unsigned int remain = count - total_bytes_received;
			unsigned int nb_last_lines = (remain / LINE_DATA_LENGTH);
			if (remain % LINE_DATA_LENGTH) {
				nb_last_lines += 1;
			}
			/* 4 bytes transmitted for each 3 data bytes */
			blocksize = ((remain / 3) * 4);
			if (remain % 3) {
				blocksize += 4;
			}
			/* Add one byte per line for the starting "length character" and two bytes per
			 * line for the ending "\r\n" */
			blocksize += (3 * nb_last_lines);
			if (trace_on) {
				printf("Reading block %d of size %u (%d line(s)).\n", i, blocksize, nb_last_lines);
			}
		}
		len = isp_serial_read(buf, SERIAL_BUFSIZE, blocksize);
		if (len <= 0) {
			printf("Error reading memory.\n");
			ret = -6;
			break;
		}
		usleep( 1000 );
		/* Now read the checksum, maybe not yet received */
		len += isp_serial_read((buf + len), (SERIAL_BUFSIZE - len), ((len > (int)blocksize) ? 0 : 3));
		if (len < 0) { /* Length may be null, as may already have received everything */
			printf("Error reading memory (checksum part).\n");
			ret = -5;
			break;
		}
		/* Decode. This must be done before sending acknowledge because we must
		 * compute the checksum */
		decoded_size = isp_uu_decode(data, buf, blocksize);
		received_checksum = strtoul((buf + blocksize), NULL, 10);
		for (j=0; j<decoded_size; j++) {
			computed_checksum += (unsigned char)data[j];
		}
		if (trace_on) {
			printf("Decoded Data :\n");
			isp_dump((unsigned char*)data, decoded_size);
		}
		if (computed_checksum == received_checksum) {
			resend_request_for_block = 0; /* reset resend request counter */
			if (trace_on) {
				printf("Reading of blocks %u OK, contained %u bytes\n", i, decoded_size);
			}
			/* Acknowledge data */
			if (isp_serial_write(DATA_BLOCK_OK, strlen(DATA_BLOCK_OK)) != strlen(DATA_BLOCK_OK)) {
				printf("Unable to send acknowledge.\n");
				ret = -4;
				break;
			}
			/* Add data length to sum of received data */
			total_bytes_received += decoded_size;
			/* Write data to file */
			ret = write(out_fd, data, decoded_size);
			if (ret != (int)decoded_size) {
				perror("File write error");
				printf("Unable to write %u bytes of data to file \"%s\", returned %d !",
						decoded_size, out_file_name, ret);
				ret = -3;
				break;
			}
		} else {
			resend_request_for_block++;
			if (trace_on) {
				printf("Checksum error for block %u (received %u, computed %u) error number: %d.\n",
						i, received_checksum, computed_checksum, resend_request_for_block);
			}
			if (resend_request_for_block > 3) {
				printf("Block %d still wrong after 3 attempts, aborting.\n", i);
				ret = -2;
				break;
			}
			/* Back to previous block */
			i--;
			/* Ask for resend */
			if (isp_serial_write(DATA_BLOCK_RESEND, strlen(DATA_BLOCK_RESEND)) != strlen(DATA_BLOCK_RESEND)) {
				printf("Unable to send resend request.\n");
				ret = -1;
				break;
			}
		}
	}
	/* Close file */
	if (out_fd != STDOUT_FILENO) {
		close(out_fd);
	}

	return ret;
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
	{"read-memory", 3, isp_cmd_read_memory},
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
	ret = isp_cmds_list[cmd_found].handler(arg_count, args);
	
	if (cmd_found == -1) {
		printf("Unknown command \"%s\", use -h or --help for a list.\n", cmd);
		return -2;
	}

	return ret;
}


