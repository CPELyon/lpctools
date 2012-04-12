/*********************************************************************
 *
 *   LPC1114 ISP
 *
 *********************************************************************/


#include <stdlib.h> /* malloc, free */
#include <stdio.h>
#include <stdint.h>

#include <unistd.h> /* open, getopt, close, usleep */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h> /* strncmp */
#include <errno.h>
#include <getopt.h>

#include <termios.h> /* for serial config */
#include <ctype.h>

#include "isp_utils.h"

#define PROG_NAME "LPC11xx ISP"
#define VERSION   "0.01"

/* short explanation on exit values:
 *
 * 0 is OK
 * 1 is arg error
 *
 */


void help(char *prog_name)
{
	fprintf(stderr, "-----------------------------------------------------------------------\n");
	fprintf(stderr, "Usage: %s [options] device command [command arguments]\n" \
		"  Default baudrate is B115200\n" \
		"  <device> is the (host) serial line used to programm the device\n" \
		"  <command> is one of:\n" \
		"  \t synchronize \n" \
		"  \t unlock \n" \
		"  \t set-baud-rate \n" \
		"  \t echo \n" \
		"  \t write-to-ram \n" \
		"  \t read-memory \n" \
		"  \t prepare-for-write \n" \
		"  \t copy-ram-to-flash \n" \
		"  \t go \n" \
		"  \t erase \n" \
		"  \t blank-check \n" \
		"  \t read-part-id \n" \
		"  \t read-boot-version \n" \
		"  \t compare \n" \
		"  \t read-uid \n" \
		"  Available options:\n" \
		"  \t -s | --synchronize : Perform synchronization before sending command\n" \
		"  \t -b | --baudrate=N : Use this baudrate (does not issue the set-baud-rate command)\n" \
		"  \t -t | --trace : turn on trace output of serial communication\n" \
		"  \t -h | --help : display this help\n" \
		"  \t -v | --version : display version information\n", prog_name);
	fprintf(stderr, "-----------------------------------------------------------------------\n");
}

#define SERIAL_BUFSIZE  128

#define SYNCHRO_START "?"
#define SYNCHRO  "Synchronized"

#define SERIAL_BAUD  B115200

int serial_fd = -1;
int trace_on = 0;


/* Open the serial device and set it up.
 * Returns 0 on success, negativ value on error.
 * Actal setup is done according to LPC11xx user's manual.
 * Only baudrate can be changed using command line option.
 */
int serial_open(int baudrate, char* serial_device)
{
	struct termios tio;

	if (serial_device == NULL) {
		printf("No serial device given on command line\n");
		return -2;
	}

	/* Open serial port */
	serial_fd = open(serial_device, O_RDWR | O_NONBLOCK);
	if (serial_fd < 0) {
		perror("Unable to open serial_device");
		printf("Tried to open \"%s\".\n", serial_device);
		return -1;
	}
	/* Setup serial port */
	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = IXON | IXOFF;  /* See section 21.4.4 of LPC11xx user's manual (UM10398) */
	tio.c_cflag = CS8 | CREAD | CLOCAL;  /* 8n1, see termios.h for more information */
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;
	cfsetospeed(&tio, baudrate);
	cfsetispeed(&tio, baudrate);
	tcsetattr(serial_fd, TCSANOW, &tio);
	
	return 0;
}

/* Simple write() wrapper, with trace if enabled */
int serial_write(const char* buf, unsigned int buf_size)
{
	int nb = 0;

	if (trace_on) {
		printf("Sending %d octet(s) :\n", buf_size);
		isp_dump((unsigned char*)buf, buf_size);
	}
	nb = write(serial_fd, buf, buf_size);
	if (nb <= 0) {
		perror("Serial write error");
		return -1;
	}
	return nb;
}

int isp_ret_code(char* buf)
{
	if (trace_on) {
		/* FIXME : Find how return code are sent (binary or ASCII) */
		printf("Received code: '0x%02x'\n", *buf);
	}
	return 0;
}

/* Try to read at least "min_read" characters from the serial line.
 * Returns -1 on error, 0 on end of file, or read count otherwise.
 */
int serial_read(char* buf, unsigned int buf_size, unsigned int min_read)
{
	int nb = 0;
	unsigned int count = 0;
	
	if (min_read > buf_size) {
		printf("serial_read: buffer too small for min read value.\n");
		return -3;
	}

	do {
		nb = read(serial_fd, &buf[count], (buf_size - count));
		if (nb < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				usleep( 5000 );
				continue;
			}
			perror("Serial read error");
			return -1;
		} else if (nb == 0) {
			printf("serial_read: end of file !!!!\n");
			return 0;
		}
		if (trace_on == 2) {
			isp_dump((unsigned char*)(&buf[count]), nb);
		}	
		count += nb;
	} while (count < min_read);

	if (trace_on) {
		printf("Received %d octet(s) :\n", count);
		isp_dump((unsigned char*)buf, count);
	}
	return count;
}

/* Connect or reconnect to the target */
/* Return 1 when connection is OK, 0 otherwise */
int connect()
{
	char buf[SERIAL_BUFSIZE];

	/* Send synchronize request */
	serial_write(SYNCHRO_START, 1);

	/* Wait for answer */
	serial_read(buf, SERIAL_BUFSIZE, strlen(SYNCHRO));

	/* Check answer, and acknoledge if OK */
	if (strncmp(SYNCHRO, buf, strlen(SYNCHRO)) == 0) {
		serial_write(SYNCHRO, strlen(SYNCHRO));
	} else {
		printf("Unable to synchronize.\n");
		return 0;
	}
	/* Empty read buffer (echo on ?) */
	serial_read(buf, SERIAL_BUFSIZE, 1);
	/* and turn off echo */
	serial_write("A 0\r\n", 1);

	return 1;
}


int main(int argc, char** argv)
{
	int baudrate = SERIAL_BAUD;
	int synchronize = 0;
	char* serial_device = NULL;

	/* For "command" handling */
	char* command = NULL;
	char** cmd_args = NULL;
	int nb_cmd_args = 0;


	/* parameter parsing */
	while(1) {
		int option_index = 0;
		int c = 0;

		struct option long_options[] = {
			{"synchronize", no_argument, 0, 's'},
			{"baudrate", required_argument, 0, 'b'},
			{"trace", no_argument, 0, 't'},
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "sb:thv", long_options, &option_index);

		/* no more options to parse */
		if (c == -1) break;

		switch (c) {
			/* b, baudrate */
			case 'b':
				baudrate = atoi(optarg);
				/* FIXME: validate baudrate */
				break;

			/* t, trace */
			case 't':
				trace_on = 1;
				break;

			/* s, synchronize */
			case 's':
				synchronize = 1;
				break;

			/* v, version */
			case 'v':
				printf("%s Version %s\n", PROG_NAME, VERSION);
				return 0;
				break;

			/* h, help */
			case 'h':
			default:
				help(argv[0]);
				return 0;
		}
	}

	/* Parse remaining command line arguments (not options). */

	/* First one should be serial device */
	if (optind < argc) {
		serial_device = argv[optind++];
		if (trace_on) {
			printf("Serial device : %s\n", serial_device);
		}
	}
	/* Next one should be "command" */
	if (optind < argc) {
		command = argv[optind++];
		if (trace_on) {
			printf("Command : %s\n", command);
		}
	}
	/* And then remaining ones (if any) are command arguments */
	if (optind < argc) {
		nb_cmd_args = argc - optind;
		cmd_args = malloc(nb_cmd_args * sizeof(char *));
		while (optind < argc) {
			printf ("%s\n", argv[optind++]);
		}
	}

	if (serial_device == NULL) {
		printf("No serial device given, exiting\n");
		help(argv[0]);
		return 0;
	}
	if (serial_open(baudrate, serial_device) != 0) {
		printf("Serial open failed, unable to initiate serial communication with target.\n");
		return -1;
	}

	/* FIXME : do not sync if command is sync */
	if (synchronize) {
		connect();
	}

	/* FIXME : call command handler */


	if (cmd_args != NULL) {
		free(cmd_args);
	}
	close(serial_fd);
	return 0;
}

