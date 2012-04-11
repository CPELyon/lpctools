/*********************************************************************
 *
 *   LPC1114 ISP
 *
 *********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h> /* for open, getopt, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
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
		"Default baudrate is B115200\n" \
		"<device> is the (host) serial line used to programm the device\n" \
		"<command> is one of:\n" \
		"  synchronize \n" \
		"  unlock \n" \
		"  set-baud-rate \n" \
		"  echo \n" \
		"  write-to-ram \n" \
		"  read-memory \n" \
		"  prepare-for-write \n" \
		"  copy-ram-to-flash \n" \
		"  go \n" \
		"  erase \n" \
		"  blank-check \n" \
		"  read-part-id \n" \
		"  read-boot-version \n" \
		"  compare \n" \
		"  read-uid \n" \
		"Available options:\n" \
		"  -s | --synchronize : Perform synchronization before sending command\n" \
		"  -b | --baudrate=N : Use this baudrate (does not issue the set-baud-rate command)\n" \
		"  -t | --trace : turn on trace output of serial communication\n" \
		"  -h | --help : display this help\n" \
		"  -v | --version : display version information\n", prog_name);
	fprintf(stderr, "-----------------------------------------------------------------------\n");
}

#define SERIAL_BUFSIZE  128

#define SYNCHRO_START "?"
#define SYNCHRO  "Synchronized"

#define SERIAL_BAUD  B115200

char * serial_device = NULL;
int serial_fd = -1;
int trace_on = 0;

int serial_open(int baudrate)
{
	struct termios tio;

	if (serial_device == NULL) {
		printf("No serial device given on command line\n");
		exit(-1);
	}

	/* Open serial port */
	serial_fd = open(serial_device, O_RDWR | O_NONBLOCK);
	if (serial_fd < 0) {
		perror("Unable to open serial_device");
		printf("Tried to open %s.\n", serial_device);
		return -1;
	}
	/* Setup serial port */
	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = IXON | IXOFF; /* See section 21.4.4 of LPC11xx user's manual (UM10398) */
	tio.c_cflag = CS8 | CREAD | CLOCAL;    /* 8n1, see termios.h for more information */
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;
	cfsetospeed(&tio, baudrate);
	cfsetispeed(&tio, baudrate);
	tcsetattr(serial_fd, TCSANOW, &tio);
	
	return 0;
}

int serial_write(const char* buf, unsigned int buf_size)
{
	int nb = 0;

	if (trace_on) {
		printf("Sending %d :\n", buf_size);
		isp_dump((unsigned char*)buf, buf_size);
	}
	nb = write(serial_fd, buf, buf_size);
	if (nb <= 0) {
		perror("Serial read error");
		return -1;
	}
	return nb;
}

int isp_ret_code(char* buf)
{
	if (trace_on) {
		/* FIXME : Find how return code are sent (binary or ASCII) */
		printf("Received code : '0x%02x'\n", *buf);
	}
	return 0;
}

int serial_read(char* buf, unsigned int buf_size)
{
	int nb = 0;

	nb = read(serial_fd, buf, buf_size);
	if (nb < 0) {
		perror("Serial read error");
		return -1;
	} else if (nb == 0) {
		printf("End of file !!!!\n");
		return -1;
	}
	if (trace_on) {
		printf("Received : %d octets\n", nb);
		isp_dump((unsigned char*)buf, nb);
	}
	return nb;
}

/* Connect or reconnect to the target */
/* Return 1 when connection is OK, 0 otherwise */
int connect()
{
	char buf[SERIAL_BUFSIZE];

	/* Send synchronize request */
	serial_write(SYNCHRO_START, 1);

	/* Wait for answer */
	serial_read(buf, SERIAL_BUFSIZE);

	/* Check answer, and acknoledge if OK */
	if (strncmp(SYNCHRO, buf, strlen(SYNCHRO)) == 0) {
		serial_write(SYNCHRO, strlen(SYNCHRO));
	} else {
		printf("Unable to synchronize.\n");
		return 0;
	}
	/* Empty read buffer (echo on ?) */
	serial_read(buf, SERIAL_BUFSIZE);
	/* and turn off echo */
	serial_write("A 0\r\n", 1);

	return 1;
}


int main(int argc, char** argv)
{
	int baudrate = SERIAL_BAUD;
	int synchronize = 0;

	/* parameter parsing */
	while(1) {
		int option_index = 0, c=0;

		struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"baudrate", required_argument, 0, 'b'},
			{"synchronize", no_argument, 0, 's'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long_only(argc, argv, "hb:sv", long_options, &option_index);

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

	/* FIXME : get serial device and command (and arguments) */
	/* serial_device = strdup(optarg); */
	if (serial_device == NULL) {
		return 0;
	}


	serial_open(baudrate);

	/* FIXME : do not sync if command is sync */
	if (synchronize) {
		connect();
	}

	/* FIXME : call command handler */


	close(serial_fd);
	return 0;
}

