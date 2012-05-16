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
#define FILE_CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP)

#if (SERIAL_BUFSIZE < (MAX_BYTES_PER_LINE * LINES_PER_BLOCK + 10 + 2)) /* uuencoded data + checksum + \r\n */
#error "SERIAL_BUFSIZE too small"
#endif

#define RAM_MAX_SIZE (8 * 1024) /* 8 KB */

char* error_codes[] = {
#define CMD_SUCCESS 0
	"CMD_SUCCESS",
#define INVALID_COMMAND 1
	"INVALID_COMMAND",
#define SRC_ADDR_ERROR 2
	"SRC_ADDR_ERROR",
#define DST_ADDR_ERROR 3
	"DST_ADDR_ERROR",
#define SRC_ADDR_NOT_MAPPED 4
	"SRC_ADDR_NOT_MAPPED",
#define DST_ADDR_NOT_MAPPED 5
	"DST_ADDR_NOT_MAPPED",
#define COUNT_ERROR 6
	"COUNT_ERROR",
#define INVALID_SECTOR 7
	"INVALID_SECTOR",
#define SECTOR_NOT_BLANK 8
	"SECTOR_NOT_BLANK",
#define SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION 9
	"SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION",
#define COMPARE_ERROR 10
	"COMPARE_ERROR",
#define BUSY 11
	"BUSY",
#define PARAM_ERROR 12
	"PARAM_ERROR",
#define ADDR_ERROR 13
	"ADDR_ERROR",
#define ADDR_NOT_MAPPED 14
	"ADDR_NOT_MAPPED",
#define CMD_LOCKED 15
	"CMD_LOCKED",
#define INVALID_CODE 16
	"INVALID_CODE",
#define INVALID_BAUD_RATE 17
	"INVALID_BAUD_RATE",
#define INVALID_STOP_BIT 18
	"INVALID_STOP_BIT",
#define CODE_READ_PROTECTION_ENABLED 19
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
		printf("Unable to synchronize, no synchro received.\n");
		return -3;
	}
	/* Empty read buffer (echo is on) */
	isp_serial_read(buf, strlen(SYNCHRO), strlen(SYNCHRO));
	/* Read reply (OK) */
	isp_serial_read(buf, REP_BUFSIZE, strlen(SYNCHRO_OK));
	if (strncmp(SYNCHRO_OK, buf, strlen(SYNCHRO_OK)) != 0) {
		printf("Unable to synchronize, synchro not acknowledged.\n");
		return -2;
	}

	/* Documentation says we should send crystal frequency .. sending anything is OK */
	isp_serial_write(freq, strlen(freq));
	/* Empty read buffer (echo is on) */
	isp_serial_read(buf, strlen(freq), strlen(freq));
	/* Read reply (OK) */
	isp_serial_read(buf, REP_BUFSIZE, strlen(SYNCHRO_OK));
	if (strncmp(SYNCHRO_OK, buf, strlen(SYNCHRO_OK)) != 0) {
		printf("Unable to synchronize, crystal frequency not acknowledged.\n");
		return -2;
	}

	/* Turn off echo */
	isp_serial_write(SYNCHRO_ECHO_OFF, strlen(SYNCHRO_ECHO_OFF));
	/* Empty read buffer (echo still on) */
	isp_serial_read(buf, strlen(SYNCHRO_ECHO_OFF), strlen(SYNCHRO_ECHO_OFF));
	/* Read eror code for command */
	isp_serial_read(buf, REP_BUFSIZE, 3);

	printf("Device session openned.\n");

	return 1;
}

int isp_send_cmd_no_args(char* cmd_name, char* cmd)
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
	ret = isp_ret_code(buf, NULL);
	return ret;
}

int isp_cmd_unlock(int quiet)
{
	int ret = 0;

	ret = isp_send_cmd_no_args("unlock", UNLOCK);
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
	
	ret = isp_send_cmd_no_args("read-uid", READ_UID);
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

	ret = isp_send_cmd_no_args("read-part-id", READ_PART_ID);
	if (ret != 0) {
		printf("Read part ID error.\n");
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

	ret = isp_send_cmd_no_args("read-boot-version", READ_BOOT_VERSION);
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
	printf("Boot code version is %u.%u\n", ver[0], ver[1]);

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
	ret = isp_ret_code(buf, NULL);
	return ret;
}

int get_remaining_blocksize(unsigned int count, unsigned int actual_count, char* dir, unsigned int i)
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
int get_nb_blocks(unsigned int count, char* dir)
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

unsigned int calc_checksum(unsigned char* data, unsigned int size)
{
	unsigned int i = 0;
	unsigned int checksum = 0;

	for (i=0; i<size; i++) {
		checksum += data[i];
	}
	return checksum;
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
	unsigned int blocks = 0;
	unsigned int total_bytes_received = 0; /* actual count of bytes received */
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
	
	/* Send command */
	ret = isp_send_cmd_two_args("read-memory", 'R', addr, count);
	if (ret != 0) {
		printf("Error when trying to read %lu bytes of memory at address 0x%08lx.\n", count, addr);
		return ret;
	}

	/* Now, find the number of blocks of the reply. */
	blocks = get_nb_blocks(count, "Reading");

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
	
	/* Receive and decode the data */
	/* FIXME : Factorize ? */
	for (i=0; i<blocks; i++) {
		char data[SERIAL_BUFSIZE];
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
		decoded_size = isp_uu_decode(data, buf, blocksize);
		received_checksum = strtoul((buf + blocksize), NULL, 10);
		computed_checksum = calc_checksum((unsigned char*)data, decoded_size);
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

int isp_send_buf_to_ram(char* data, unsigned long int addr, unsigned int count)
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

int isp_cmd_write_to_ram(int arg_count, char** args)
{
	/* Arguments */
	unsigned long int addr = 0;
	char* in_file_name = NULL;
	int in_fd = -1;
	char file_buff[RAM_MAX_SIZE];
	unsigned int bytes_read = 0;
	int ret = 0;

	/* Check write-to-ram arguments */
	if (arg_count != 2) {
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

	/* Open file for reading if input is not stdin */
	if (strncmp(in_file_name, "-", strlen(in_file_name)) == 0) {
		/* FIXME : set as non blocking ? */
		in_fd = STDIN_FILENO;
	} else {
		in_fd = open(in_file_name, O_RDONLY | O_NONBLOCK);
		if (in_fd <= 0) {
			perror("Unable to open file for reading");
			printf("Tried to open \"%s\".\n", in_file_name);
			return -13;
		}
	}
	/* Read file content */
	do {
		int nb = read(in_fd, &file_buff[bytes_read], (RAM_MAX_SIZE - bytes_read));
		if (nb < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep( 50 );
				continue;
			}
			perror("Input file read error");
			if (in_fd != STDIN_FILENO) {
				close(in_fd);
			}
			return -12;
		} else if (nb == 0) {
			/* End of file */
			break;
		}
		bytes_read += nb;
	} while (bytes_read < RAM_MAX_SIZE);
	if (in_fd != STDIN_FILENO) {
		close(in_fd);
	}
	if (trace_on) {
		printf("Read %d octet(s) from input file\n", bytes_read);
	}

	/* Pad data size to 4 */
	if (bytes_read & 0x03) {
		bytes_read = (bytes_read & ~0x03) + 0x04;
	}

	/* And send to ram */
	ret = isp_send_buf_to_ram(file_buff, addr, bytes_read);

	return ret;
}

int isp_send_cmd_address(int arg_count, char** args, char* name, char cmd)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	char* tmp = NULL;
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

	/* Create request */
	len = snprintf(buf, SERIAL_BUFSIZE, "%c %lu %lu %lu\r\n", cmd, addr1, addr2, length);
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
	ret = isp_ret_code(buf, &tmp);
	return ret;
}

int isp_cmd_compare(int arg_count, char** args)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
	unsigned long int offset = 0;
	
	ret = isp_send_cmd_address(arg_count, args, "compare", 'M');
	switch (ret) {
		case CMD_SUCCESS:
			printf("Source and destination data are equal.\n");
			break;
		case COMPARE_ERROR:
			/* read remaining data */
			usleep( 2000 );
			len = isp_serial_read(buf, SERIAL_BUFSIZE, 3);
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

	ret = isp_send_cmd_address(arg_count, args, "copy-ram-to-flash", 'C');
	
	if (ret != 0) {
		printf("Error when trying to copy data from ram to flash.\n");
		return ret;
	}
	printf("Data copied from ram to flash.\n");

	return 0;
}

int isp_cmd_go(int arg_count, char** args)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
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

	/* Create go request */
	len = snprintf(buf, SERIAL_BUFSIZE, "G %lu %c\r\n", addr, mode[0]);
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
	ret = isp_ret_code(buf, NULL);
	if (ret != 0) {
		printf("Error when trying to execute programm at 0x%08lx in %s mode.\n", addr, mode);
		return -1;
	}

	return 0;
}

int isp_cmd_sectors_skel(int arg_count, char** args, char* name, char cmd)
{
	char buf[SERIAL_BUFSIZE];
	int ret = 0, len = 0;
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
	if (last_sector < first_sector) {
		printf("Last sector must be after (or equal to) first sector for %s command.\n", name);
		return -6;
	}

	/* Create request */
	len = snprintf(buf, SERIAL_BUFSIZE, "%c %lu %lu\r\n", cmd, first_sector, last_sector);
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
	ret = isp_ret_code(buf, NULL);

	return ret;
}

int isp_cmd_blank_check(int arg_count, char** args)
{
	char* tmp = NULL;
	unsigned long int offset = 0, content = 0;
	char buf[SERIAL_BUFSIZE];
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
			len = isp_serial_read(buf, SERIAL_BUFSIZE, 3);
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



