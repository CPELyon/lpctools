/*********************************************************************
 *
 *   ISP (In-System Programming) tool for NXP LPC Microcontrollers
 *
 * This tool does not give access to each isp command, instead it
 * provides wrappers for flashing a device.
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

#include <string.h> /* strncmp, strlen */

#include "isp_utils.h"
#include "isp_commands.h"
#include "prog_commands.h"
#include "parts.h"

#define PROG_NAME "LPC ISP Prog tool"
#define VERSION   "1.00"


void help(char *prog_name)
{
	fprintf(stderr, "---------------- "PROG_NAME" --------------------------------\n");
	fprintf(stderr, "Usage: %s [options] serial_device command [command arguments]\n" \
		"  Default baudrate is B115200\n" \
		"  Default oscilator frequency used is 10000 KHz\n" \
		"  <serial_device> is the (host) serial line used to programm the device\n" \
		"  <command> is one of:\n" \
		"  \t dump, flash, id, blank, go\n" \
		"  command specific arguments are:\n" \
		"  \t dump file : dump flash content to 'file'\n" \
		"  \t flash file : put 'file' to flash, erasing requiered sectors\n" \
		"  \t blank : erase whole flash\n" \
		"  \t id : get all id information\n" \
		"  \t go : execute programm from reset handler in thumb mode and open terminal\n" \
		"  Available options:\n" \
		"  \t -b | --baudrate=N : Use this baudrate (Same baudrate must be used across whole session)\n" \
		"  \t -t | --trace : turn on trace output of serial communication\n" \
		"  \t -f | --freq=N : Oscilator frequency of target device\n" \
		"  \t -n | --no-user-code : do not compute a valid user code for exception vector 7\n" \
		"  \t -h | --help : display this help\n" \
		"  \t -v | --version : display version information\n", prog_name);
	fprintf(stderr, "-----------------------------------------------------------------------\n");
}

#define SERIAL_BAUD  B115200

int trace_on = 0;
int quiet = 0;
static int calc_user_code = 1; /* User code is computed by default */

static int prog_connect_and_id(int freq);
static int prog_handle_command(char* cmd, int dev_id, int arg_count, char** args);

int main(int argc, char** argv)
{
	int baudrate = SERIAL_BAUD;
	int crystal_freq = 10000;
	char* isp_serial_device = NULL;
	int dev_id = 0;

	/* For "command" handling */
	char* command = NULL;
	char** cmd_args = NULL;
	int nb_cmd_args = 0;


	/* parameter parsing */
	while(1) {
		int option_index = 0;
		int c = 0;

		struct option long_options[] = {
			{"synchronized", no_argument, 0, 's'},
			{"baudrate", required_argument, 0, 'b'},
			{"trace", no_argument, 0, 't'},
			{"freq", required_argument, 0, 'f'},
			{"no-user-code", no_argument, 0, 'n'},
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "sb:tf:nhv", long_options, &option_index);

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

			/* f, freq */
			case 'f':
				crystal_freq = atoi(optarg);
				break;

			/* u, user-code */
			case 'n':
				calc_user_code = 0;
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

	/* Frist : sync with device */
	dev_id = prog_connect_and_id(crystal_freq);
	if (dev_id < 0) {
		printf("Unable to connect to target, consider hard reset of target or link\n");
		return -1;
	}

	if (command != NULL)  {
		int err = 0;
		err = prog_handle_command(command, dev_id, nb_cmd_args, cmd_args);
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

struct prog_command {
	int cmd_num;
	char* name;
};

static struct prog_command prog_cmds_list[] = {
	{0, "dump"},
	{1, "flash"},
	{2, "id"},
	{3, "blank"},
	{4, "go"},
	{5, NULL}
};

/*
 * Try to connect to the target and identify the device.
 * First try sync, and if it fails, try to read id twice. if both fail, then we are not connected
 */
static int prog_connect_and_id(int freq)
{
	int sync_ret = 0;

	/* Try to connect */
	sync_ret = isp_connect(freq, 1);
	/* Synchro failed or already synchronised ? */
	if (sync_ret < 0) {
		/* If already synchronized, then sync command and the next command fail.
		   Sync failed, maybe we are already sync'ed, then send one command for nothing */
		isp_cmd_part_id(1);
	}

	return isp_cmd_part_id(1);
}

static int prog_handle_command(char* cmd, int dev_id, int arg_count, char** args)
{
	int cmd_found = -1;
	int ret = 0;
	int index = 0;
	struct part_desc* part = NULL;

	if (cmd == NULL) {
		printf("prog_handle_command called with no command !\n");
		return -1;
	}

	part = find_part(dev_id);
	if (part == NULL) {
		printf("Unknown part number : 0x%08x.\n", dev_id);
		return -2;
	}

	while ((cmd_found == -1) && (prog_cmds_list[index].name != NULL)) {
		if (strncmp(prog_cmds_list[index].name, cmd, strlen(prog_cmds_list[index].name)) == 0) {
			cmd_found = index;
			break;
		}
		index++;
	}
	if (cmd_found == -1) {
		printf("Unknown command \"%s\", use -h or --help for a list.\n", cmd);
		return -3;
	}

	switch (prog_cmds_list[cmd_found].cmd_num) {
		case 0: /* dump, need one arg : filename */
			if (arg_count != 1) {
				printf("command dump needs one arg (filename), got %d.\n", arg_count);
				return -4;
			}
			ret = dump_to_file(part, args[0]);
			break;

		case 1: /* flash, need one arg : filename */
			if (arg_count != 1) {
				printf("command flash needs one arg (filename), got %d.\n", arg_count);
				return -4;
			}
			ret = flash_target(part, args[0], calc_user_code);
			break;

		case 2: /* id : no args */
			ret = get_ids();
			break;

		case 3: /* blank : no args */
			ret = erase_flash(part);
			break;

		case 4: /* go : no args */
			ret = start_prog(part);
			break;
	}

	return ret;
}

