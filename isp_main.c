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
#include "isp_commands.h"

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
		"  \t synchronize (sync done each time unless -n is used (for multiple successive calls))\n" \
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
		"  \t -n | --no-synchronize : Do not perform synchronization before sending command\n" \
		"  \t -b | --baudrate=N : Use this baudrate (does not issue the set-baud-rate command)\n" \
		"  \t -t | --trace : turn on trace output of serial communication\n" \
		"  \t -h | --help : display this help\n" \
		"  \t -v | --version : display version information\n", prog_name);
	fprintf(stderr, "-----------------------------------------------------------------------\n");
}

#define SERIAL_BAUD  B115200

int trace_on = 0;


int main(int argc, char** argv)
{
	int baudrate = SERIAL_BAUD;
	int dont_synchronize = 0;
	char* isp_serial_device = NULL;

	/* For "command" handling */
	char* command = NULL;
	char** cmd_args = NULL;
	int nb_cmd_args = 0;


	/* parameter parsing */
	while(1) {
		int option_index = 0;
		int c = 0;

		struct option long_options[] = {
			{"no-synchronize", no_argument, 0, 'n'},
			{"baudrate", required_argument, 0, 'b'},
			{"trace", no_argument, 0, 't'},
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "nb:thv", long_options, &option_index);

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
			case 'n':
				dont_synchronize = 1;
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

	/* First one should (must) be serial device */
	if (optind < argc) {
		isp_serial_device = argv[optind++];
		if (trace_on) {
			printf("Serial device : %s\n", isp_serial_device);
		}
	}
	if (isp_serial_device == NULL) {
		printf("No serial device given, exiting\n");
		help(argv[0]);
		return 0;
	}
	if (isp_serial_open(baudrate, isp_serial_device) != 0) {
		printf("Serial open failed, unable to initiate serial communication with target.\n");
		return -1;
	}

	/* Next one should be "command" (if present) */
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
		if (trace_on) {
			printf("Command arguments :\n");
		}
		while (optind < argc) {
			static unsigned int idx = 0;
			cmd_args[idx++] = argv[optind++];
			if (trace_on) {
				printf("%s\n", cmd_args[idx - 1]);
			}
		}
	}

	if (! dont_synchronize) {
		isp_connect();
	}

	/* FIXME : call command handler */
	if (command != NULL) {
		int err = 0;
		err = isp_handle_command(command, nb_cmd_args, cmd_args);
		if (err >= 0) {
			if (trace_on) {
				printf("Command \"%s\" handled OK.\n", command);
			}
		} else {
			printf("Error handling command \"%s\" : %d\n", command, err);
		}
	} else {
		printf("No command given. use -h or --help for help on available commands.\n");
	}


	if (cmd_args != NULL) {
		free(cmd_args);
	}
	isp_serial_close();
	return 0;
}

