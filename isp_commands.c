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

#include "isp_utils.h"

extern int trace_on;


/* Max should be 1270 for read memory, a little bit more for writes */
#define SERIAL_BUFSIZE  1300
#define REP_BUFSIZE 100

#define SYNCHRO_START "?"
#define SYNCHRO  "Synchronized\r\n"
#define SYNCHRO_OK "OK\r\n"
#define DATA_BLOCK_OK "OK\r\n"
#define DATA_BLOCK_RESEND "RESEND\r\n"
#define SYNCHRO_ECHO_OFF "A 0\r\n"

#define ISP_ABORT ""  /* Not handled */

#define UNLOCK "U 23130\r\n"
#define READ_UID "N\r\n"
#define READ_PART_ID "J\r\n"
#define READ_BOOT_VERSION "K\r\n"

#define LINE_DATA_LENGTH 45
#define LINES_PER_BLOCK 20
#define MAX_DATA_BLOCK_SIZE (LINES_PER_BLOCK * LINE_DATA_LENGTH) /* 900 */
#define MAX_BYTES_PER_LINE (1 + ((LINE_DATA_LENGTH / 3) * 4) + 2) /* "line length" (1) + 60 + \r\n */

#if (SERIAL_BUFSIZE < (MAX_BYTES_PER_LINE * LINES_PER_BLOCK + 10 + 2)) /* uuencoded data + checksum + \r\n */
#error "SERIAL_BUFSIZE too small"
#endif

/* Order in this table is important */
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

static int isp_ret_code(char* buf, char** endptr, int quiet)
{
	unsigned int ret = 0;
	ret = strtoul(buf, endptr, 10);
	/* FIXME : Find how return code are sent (binary or ASCII) */
	if (quiet != 1) {
		if (ret > (sizeof(error_codes)/sizeof(char*))) {
			printf("Received unknown error code '%u' !\n", ret);
		} else if ((ret != 0) || trace_on) {
			printf("Received error code '%u': %s\n", ret, error_codes[ret]);
		}
	}
	return ret;
}


/* Connect or reconnect to the target.
 * crystal_freq is in KHz
 * Return positive or NULL value when connection is OK, or negative value otherwise.
 */
int isp_connect(unsigned int crystal_freq, int quiet)
{
	char buf[REP_BUFSIZE];
	char freq[10];

	snprintf(freq, 8, "%d\r\n", crystal_freq);

	/* Send synchronize request */
	if (isp_serial_write(SYNCHRO_START, strlen(SYNCHRO_START)) != strlen(SYNCHRO_START)) {
		printf("Unable to send synchronize request.\n");
		return -5;
	}
	/* Wait for answer */
	if (isp_serial_read(buf, REP_BUFSIZE, strlen(SYNCHRO)) < 0) {
		printf("Error reading synchronize answer.\n");
		return -4;
	}
	/* Check answer, and acknowledge if OK */
	if (strncmp(SYNCHRO, buf, strlen(SYNCHRO)) == 0) {
		isp_serial_write(SYNCHRO, strlen(SYNCHRO));
	} else {
		if (quiet != 1) {
			printf("Unable to synchronize, no synchro received.\n");
		}
		return -3;
	}
	/* Empty read buffer (echo is on) */
	isp_serial_empty_buffer();
	/* Read reply (OK) */
	isp_serial_read(buf, REP_BUFSIZE, strlen(SYNCHRO_OK));
	if (strncmp(SYNCHRO_OK, buf, strlen(SYNCHRO_OK)) != 0) {
		printf("Unable to synchronize, synchro not acknowledged.\n");
		return -2;
	}

	/* Documentation says we should send crystal frequency .. sending anything is OK */
	isp_serial_write(freq, strlen(freq));
	/* Empty read buffer (echo is on) */
	isp_serial_empty_buffer();
	/* Read reply (OK) */
	isp_serial_read(buf, REP_BUFSIZE, strlen(SYNCHRO_OK));
	if (strncmp(SYNCHRO_OK, buf, strlen(SYNCHRO_OK)) != 0) {
		printf("Unable to synchronize, crystal frequency not acknowledged.\n");
		return -2;
	}

	/* Turn off echo */
	isp_serial_write(SYNCHRO_ECHO_OFF, strlen(SYNCHRO_ECHO_OFF));
	/* Empty read buffer (echo still on) */
	isp_serial_empty_buffer();
	/* Read eror code for command */
	isp_serial_read(buf, REP_BUFSIZE, 3);

	/* Leave it even in quiet mode, so the user knows something is going on */
	printf("Device session openned.\n");

	return 1;
}

int isp_send_cmd_no_args(char* cmd_name, char* cmd, int quiet)
{
	char buf[5];
	int ret = 0, len = 0;

	/* Send request */
	if (isp_serial_write(cmd, strlen(cmd)) != (int)strlen(cmd)) {
		printf("Unable to send %s request.\n", cmd_name);
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, 3, 3);
	if (len <= 0) {
		printf("Error reading %s acknowledge.\n", cmd_name);
		return -4;
	}
	ret = isp_ret_code(buf, NULL, quiet);
	return ret;
}

int isp_cmd_unlock(int quiet)
{
	int ret = 0;

	ret = isp_send_cmd_no_args("unlock", UNLOCK, quiet);
	if (ret != 0) {
		printf("Unlock error.\n");
		return -1;
	}
	if (quiet == 0) {
		printf("Device memory protection unlocked.\n");
	}

	return 0;
}

int isp_cmd_read_uid(void)
{
	char buf[REP_BUFSIZE];
	char* tmp = NULL;
	int i = 0, ret = 0, len = 0;
	unsigned long int uid[4];

	ret = isp_send_cmd_no_args("read-uid", READ_UID, 0);
	if (ret != 0) {
		printf("Read UID error.\n");
		return ret;
	}
	len = isp_serial_read(buf, REP_BUFSIZE, 50);
	if (len <= 0) {
		printf("Error reading uid.\n");
		return -2;
	}
	tmp = buf;
	for (i=0; i<4; i++) {
		static char* endptr = NULL;
		uid[i] = strtoul(tmp, &endptr, 10);
		tmp = endptr;
	}
	printf("UID: 0x%08lx - 0x%08lx - 0x%08lx - 0x%08lx\n", uid[0], uid[1], uid[2], uid[3]);

	return 0;
}

int isp_cmd_part_id(int quiet)
{
	char buf[REP_BUFSIZE];
	int ret = 0, len = 0;
	unsigned long int part_id = 0;

	ret = isp_send_cmd_no_args("read-part-id", READ_PART_ID, quiet);
	if (ret != 0) {
		if (quiet != 1) {
			printf("Read part ID error.\n");
		}
		return ret;
	}
	len = isp_serial_read(buf, REP_BUFSIZE, 15);
	if (len <= 0) {
		printf("Error reading part ID.\n");
		return -2;
	}
	/* FIXME : some part IDs are on two 32bits values */
	part_id = strtoul(buf, NULL, 10);
	if (quiet == 0) {
		printf("Part ID is 0x%08lx\n", part_id);
	}

	return part_id;
}

int isp_cmd_boot_version(void)
{
	char buf[REP_BUFSIZE];
	int ret = 0, len = 0;
	char* tmp = NULL;
	unsigned int ver[2];

	ret = isp_send_cmd_no_args("read-boot-version", READ_BOOT_VERSION, 0);
	if (ret != 0) {
		printf("Read boot version error.\n");
		return ret;
	}
	len = isp_serial_read(buf, REP_BUFSIZE, 20);
	if (len <= 0) {
		printf("Error reading boot version.\n");
		return -2;
	}
	ver[0] = strtoul(buf, &tmp, 10);
	ver[1] = strtoul(tmp, NULL, 10);
	printf("Boot code version is %u.%u\n", ver[1], ver[0]);

	return 0;
}

int isp_send_cmd_two_args(char* cmd_name, char cmd, unsigned int arg1, unsigned int arg2)
{
	char buf[REP_BUFSIZE];
	int ret = 0, len = 0;

	/* Create read-memory request */
	len = snprintf(buf, REP_BUFSIZE, "%c %u %u\r\n", cmd, arg1, arg2);
	if (len > REP_BUFSIZE) {
		len = REP_BUFSIZE; /* This should not happen if REP_BUFSIZE is 32 or more ... */
	}

	/* Send request */
	if (isp_serial_write(buf, len) != len) {
		printf("Unable to send %s request.\n", cmd_name);
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, 3, 3);
	if (len <= 0) {
		printf("Error reading %s acknowledge.\n", cmd_name);
		return -4;
	}
	ret = isp_ret_code(buf, NULL, 0);
	return ret;
}

static int get_remaining_blocksize(unsigned int count, unsigned int actual_count, char* dir, unsigned int i)
{
	unsigned int blocksize = 0;
	unsigned int remain = count - actual_count;

	if (remain >= MAX_DATA_BLOCK_SIZE) {
		blocksize = (MAX_BYTES_PER_LINE * LINES_PER_BLOCK);
		if (trace_on) {
			printf("%s block %d (%d line(s)).\n", dir, i, LINES_PER_BLOCK);
		}
	} else {
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
			printf("%s block %d of size %u (%d line(s)).\n", dir, i, blocksize, nb_last_lines);
		}
	}
	return blocksize;
}

/* Compute the number of blocks of the transmitted data. */
static int get_nb_blocks(unsigned int count, char* dir)
{
	unsigned int lines = 0;
	unsigned int blocks = 0;

	/* See section 21.4.3 of LPC11xx user's manual (UM10398) */
	lines = (count / LINE_DATA_LENGTH);
	if (count % LINE_DATA_LENGTH) {
		lines += 1;
	}
	blocks = (lines / LINES_PER_BLOCK);
	if (lines % LINES_PER_BLOCK) {
		blocks += 1;
	}
	if (trace_on) {
		printf("%s %d block(s) (%d line(s)).\n", dir, blocks, lines);
	}

	return blocks;
}

static unsigned int calc_checksum(unsigned char* data, unsigned int size)
{
	unsigned int i = 0;
	unsigned int checksum = 0;

	for (i=0; i<size; i++) {
		checksum += data[i];
	}
	return checksum;
}

/*
 * perform read-memory operation
 * read 'count' bytes from 'addr' to 'data' buffer
 */
int isp_read_memory(char* data, uint32_t addr, unsigned int count, unsigned int uuencoded)
{
	/* Serial communication */
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	/* Reply handling */
	unsigned int blocks = 0;
	unsigned int total_bytes_received = 0; /* actual count of bytes received */
	unsigned int i = 0;

	/* Send command */
	ret = isp_send_cmd_two_args("read-memory", 'R', addr, count);
	if (ret != 0) {
		printf("Error when trying to read %u bytes of memory at address 0x%08x.\n", count, addr);
		return ret;
	}

	if (uuencoded == 0) {
		len = isp_serial_read(data, count, count);
		if (len <= 0) {
			printf("Error reading memory.\n");
			return -6;
		}
		if (len == (int)count) {
			return len;
		}
		/* Wait some time before reading possible remaining data */
		usleep( 1000 );
		len += isp_serial_read((data + len), (count - len), (count - len));
		if (len < 0) { /* Length may be null, as we may already have received everything */
			printf("Error reading memory.\n");
			return -5;
		}
		return len;
	}

	/* Now, find the number of blocks of the reply. */
	blocks = get_nb_blocks(count, "Reading");

	/* Receive and decode the data */
	for (i=0; i<blocks; i++) {
		unsigned int blocksize = 0, decoded_size = 0;
		unsigned int received_checksum = 0, computed_checksum = 0;
		static int resend_request_for_block = 0;

		/* First compute the next block size */
		blocksize = get_remaining_blocksize(count, total_bytes_received, "Reading", i);
		/* Read the uuencoded data */
		len = isp_serial_read(buf, SERIAL_BUFSIZE, blocksize);
		if (len <= 0) {
			printf("Error reading memory.\n");
			ret = -6;
			break;
		}
		usleep( 1000 );
		/* Now read the checksum, maybe not yet received */
		len += isp_serial_read((buf + len), (SERIAL_BUFSIZE - len), ((len > (int)blocksize) ? 0 : 3));
		if (len < 0) { /* Length may be null, as we may already have received everything */
			printf("Error reading memory (checksum part).\n");
			ret = -5;
			break;
		}
		/* Decode. This must be done before sending acknowledge because we must
		 * compute the checksum */
		decoded_size = isp_uu_decode((data + total_bytes_received), buf, blocksize);
		received_checksum = strtoul((buf + blocksize), NULL, 10);
		computed_checksum = calc_checksum((unsigned char*)(data + total_bytes_received), decoded_size);
		if (trace_on) {
			printf("Decoded Data :\n");
			isp_dump((unsigned char*)(data + total_bytes_received), decoded_size);
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

	return total_bytes_received;
}


/*
 * perform write-to-ram operation
 * send 'count' bytes from 'data' to 'addr' in RAM
 */
int isp_send_buf_to_ram(char* data, unsigned long int addr, unsigned int count, unsigned int perform_uuencode)
{
	/* Serial communication */
	int ret = 0, len = 0;
	/* Reply handling */
	unsigned int blocks = 0;
	unsigned int total_bytes_sent = 0;
	unsigned int i = 0;

	/* Send write-to-ram request */
	ret = isp_send_cmd_two_args("write-to-ram", 'W', addr, count);
	if (ret != 0) {
		printf("Error when trying to start write procedure to address 0x%08lx.\n", addr);
		return -8;
	}

	/* First check if we must UU-encode data */
	if (perform_uuencode == 0) {
		if (isp_serial_write(data, count) != (int)count) {
			printf("Error sending raw binary data.\n");
			return -7;
		}
		/* No checks required, we are done */
		return 0;
	}
	/* Now, find the number of blocks of data to send. */
	blocks = get_nb_blocks(count, "Sending");

	/* Encode and send the data */
	for (i=0; i<blocks; i++) {
		char buf[SERIAL_BUFSIZE]; /* Store encoded data, to be sent to the microcontroller */
		char repbuf[REP_BUFSIZE];
		unsigned int datasize = 0, encoded_size = 0;
		unsigned int computed_checksum = 0;
		static int resend_requested_for_block = 0;

		/* First compute the next block size */
		datasize = (count - total_bytes_sent);
		if (datasize >= MAX_DATA_BLOCK_SIZE) {
			datasize = MAX_DATA_BLOCK_SIZE;
		}

		/* uuencode data */
		encoded_size = isp_uu_encode(buf, data + total_bytes_sent, datasize);
		/* Add checksum */
		computed_checksum = calc_checksum((unsigned char*)(data + total_bytes_sent), datasize);
		encoded_size += snprintf((buf + encoded_size), 12, "%u\r\n", computed_checksum);
		if (trace_on) {
			printf("Encoded Data :\n");
			isp_dump((unsigned char*)buf, encoded_size);
		}
		if (isp_serial_write(buf, encoded_size) != (int)encoded_size) {
			printf("Error sending uuencoded data.\n");
			ret = -6;
			break;
		}

		usleep( 20000 );
		len = isp_serial_read(repbuf, REP_BUFSIZE, 4);
		if (len <= 0) {
			printf("Error reading write acknowledge.\n");
			return -5;
		}
		if (strncmp(DATA_BLOCK_OK, repbuf, strlen(DATA_BLOCK_OK)) == 0) {
			total_bytes_sent += datasize;
			resend_requested_for_block = 0; /* reset resend request counter */
			if (trace_on) {
				printf("Block %d sent.\n", i);
			}
		} else {
			resend_requested_for_block++;
			if (trace_on) {
				printf("Checksum error for block %u, error number: %d.\n", i, resend_requested_for_block);
			}
			if (resend_requested_for_block >= 3) {
				printf("Block %d still wrong after 3 attempts, aborting.\n", i);
				ret = -2;
				break;
			}
			/* Back to previous block */
			i--;
		}
	}

	return ret;
}


int isp_send_cmd_address(char cmd, uint32_t addr1, uint32_t addr2, uint32_t length, char* name)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	char* tmp = NULL;

	/* Create request */
	len = snprintf(buf, SERIAL_BUFSIZE, "%c %u %u %u\r\n", cmd, addr1, addr2, length);
	if (len > SERIAL_BUFSIZE) {
		len = SERIAL_BUFSIZE;
	}

	/* Send request */
	if (isp_serial_write(buf, len) != len) {
		printf("Unable to send %s request.\n", name);
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, 3, 3);
	if (len <= 0) {
		printf("Error reading %s result.\n", name);
		return -4;
	}
	ret = isp_ret_code(buf, &tmp, 0);
	return ret;
}


int isp_send_cmd_go(uint32_t addr, char mode)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;

	if (addr < 0x200) {
		printf("Error: address must be 0x00000200 or greater for go command.\n");
		return -6;
	}
	if ((mode != 'T') && (mode != 'A')) {
		printf("Error: mode must be 'thumb' (T) or 'arm' (A) for go command.\n");
		return -5;
	}

	/* Create go request */
	len = snprintf(buf, SERIAL_BUFSIZE, "G %u %c\r\n", addr, mode);
	if (len > SERIAL_BUFSIZE) {
		len = SERIAL_BUFSIZE;
	}

	/* Send go request */
	if (isp_serial_write(buf, len) != len) {
		printf("Unable to send go request.\n");
		return -4;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, SERIAL_BUFSIZE, 3);
	if (len <= 0) {
		printf("Error reading go result.\n");
		return -3;
	}
	ret = isp_ret_code(buf, NULL, 0);
	if (ret != 0) {
		printf("Error when trying to execute program at 0x%08x in '%c' mode.\n", addr, mode);
		return -1;
	}

	return 0;
}

int isp_send_cmd_sectors(char* name, char cmd, int first_sector, int last_sector, int quiet)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;

	if (last_sector < first_sector) {
		printf("Last sector must be after (or equal to) first sector for %s command.\n", name);
		return -6;
	}

	/* Create request */
	len = snprintf(buf, SERIAL_BUFSIZE, "%c %u %u\r\n", cmd, first_sector, last_sector);
	if (len > SERIAL_BUFSIZE) {
		len = SERIAL_BUFSIZE;
	}
	/* Send request */
	if (isp_serial_write(buf, len) != len) {
		printf("Unable to send %s request.\n", name);
		return -5;
	}
	/* Wait for answer */
	usleep( 5000 );
	len = isp_serial_read(buf, 3, 3); /* Read at exactly 3 bytes, so caller can retreive info */
	if (len <= 0) {
		printf("Error reading %s result.\n", name);
		return -4;
	}
	ret = isp_ret_code(buf, NULL, quiet);

	return ret;
}


