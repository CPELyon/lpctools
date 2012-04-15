/*********************************************************************
 *
 *   LPC1114 ISP
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 * 
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/


#include <stdlib.h> /* malloc, free */
#include <stdio.h>
#include <stdint.h>

#include <unistd.h> /* open, getopt, close, usleep */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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
	fprintf(stderr, "Usage: %s [options] device [-s | --synchronize]\n" \
		"       %s [options] device command [command arguments]\n" \
		"  Default baudrate is B115200\n" \
		"  <device> is the (host) serial line used to programm the device\n" \
		"  <command> is one of:\n" \
		"  \t unlock \n" \
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
		"  Notes:\n" \
		"   - Access to the ISP mode is done by calling this utility once with the synchronize\n" \
		"     option and no command. This starts a session. No command can be used before starting\n" \
		"     a session, and no other synchronize request must be done once the session is started\n" \
		"     unless the target is reseted, which closes the session.\n" \
		"   - Echo is turned OFF when starting a session and the command is not available.\n" \
		"   - The set-baud-rate command is not available. The SAME baudrate MUST be used for the\n" \
		"     whole session. It must be specified on each successive call if the default baudrate\n" \
		"     is not used.\n" \
		"  Available options:\n" \
		"  \t -s | --synchronize : Perform synchronization (open session)\n" \
		"  \t -b | --baudrate=N : Use this baudrate (does not issue the set-baud-rate command)\n" \
		"  \t -t | --trace : turn on trace output of serial communication\n" \
		"  \t -h | --help : display this help\n" \
		"  \t -v | --version : display version information\n", prog_name, prog_name);
	fprintf(stderr, "-----------------------------------------------------------------------\n");
}

#define SERIAL_BAUD  B115200

int trace_on = 0;


int main(int argc, char** argv)
{
	int baudrate = SERIAL_BAUD;
	int crystal_freq = 10000;
	int synchronize = 0;
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

	if (synchronize) {
		if (optind < argc) {
			/* no command can be specified when opening a session */
			printf("No command can be specified with -s or --synchronize (Session opening)\n");
			printf("NOT SYNCHRONIZED !\n");
			return -1;
		}
		isp_connect(crystal_freq);
		isp_serial_close();
		return 0;
	}

	/* Next one should be "command" (if present) */
	if (optind < argc) {
		command = argv[optind++];
		if (trace_on) {
			printf("Command : %s\n", command);
		}
	} else {
		printf("No command given. use -h or --help for help on available commands.\n");
		isp_serial_close();
		return -1;
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

	if (command != NULL)  {
		int err = 0;
		err = isp_handle_command(command, nb_cmd_args, cmd_args);
		if (err >= 0) {
			if (trace_on) {
				printf("Command \"%s\" handled OK.\n", command);
			}
		} else {
			printf("Error handling command \"%s\" : %d\n", command, err);
		}
	}


	if (cmd_args != NULL) {
		free(cmd_args);
	}
	isp_serial_close();
	return 0;
}

