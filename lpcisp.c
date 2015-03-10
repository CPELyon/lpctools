/*********************************************************************
 *
 *   ISP (In-System Programming) tool for NXP LPC Microcontrollers
 *
 * This tool gives access to each of the useful isp commands on LPC
 * devices. It does not provide wrappers for flashing a device.
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

#define PROG_NAME "LPC ISP"
#define VERSION   "1.07"

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
		"  <device> is the (host) serial line used to program the device\n" \
		"  <command> is one of:\n" \
		"  \t unlock, write-to-ram, read-memory, prepare-for-write, copy-ram-to-flash, go, erase,\n" \
		"  \t blank-check, read-part-id, read-boot-version, compare and read-uid.\n" \
		"  command specific arguments are as follow:\n" \
		"  \t unlock \n" \
		"  \t write-to-ram address file uuencode : send 'file' to 'address' in ram with or without uuencoding\n" \
		"  \t read-memory address count file uuencoded : read 'count' bytes from 'address', store then in 'file' (maybe decode)\n" \
		"  \t prepare-for-write first last : prepare sectors from 'first' to 'last' for write operation\n" \
		"  \t copy-ram-to-flash flash_addr ram_addr count : copy count bytes (256, 512, 1024 or 4096)\n" \
		"  \t     from 'ram_addr' to 'flash_addr'\n" \
		"  \t go address mode : execute program at 'address' (> 0x200) in 'mode' ('arm' or 'thumb')\n" \
		"  \t erase first last : erase flash starting from 'first' sector up to (including) 'last' sector \n" \
		"  \t blank-check first last : check flash starting from 'first' sector to 'last' sector is blank\n" \
		"  \t read-part-id \n" \
		"  \t read-boot-version \n" \
		"  \t compare address1 address2 count : compare count bytes between address1 and address2\n" \
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
		"   - User must issue an 'unlock' command before any of 'copy-ram-to-flash', 'erase',\n" \
		"     and 'go' commands.\n" \
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

int isp_handle_command(char* cmd, int arg_count, char** args);

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
		isp_connect(crystal_freq, 0);
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

struct isp_command {
	int cmd_num;
	char* name;
	int nb_args;
	int (*handler)(int arg_count, char** args);
};


static struct isp_command isp_cmds_list[] = {
	{0, "unlock", 0, NULL},
	{1, "write-to-ram", 3, isp_cmd_write_to_ram},
	{2, "read-memory", 3, isp_cmd_read_memory},
	{3, "prepare-for-write", 2, isp_cmd_prepare_for_write},
	{4, "copy-ram-to-flash", 3, isp_cmd_copy_ram_to_flash},
	{5, "go", 2, isp_cmd_go},
	{6, "erase", 2, isp_cmd_erase},
	{7, "blank-check", 2, isp_cmd_blank_check},
	{8, "read-part-id", 0, NULL},
	{9, "read-boot-version", 0, NULL},
	{10, "compare", 3, isp_cmd_compare},
	{11, "read-uid", 0, NULL},
	{-1, NULL, 0, NULL}
};

void isp_warn_args(int cmd_num, int arg_count, char** args)
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
	if (cmd_found == -1) {
		printf("Unknown command \"%s\", use -h or --help for a list.\n", cmd);
		return -2;
	}
	if (arg_count != isp_cmds_list[cmd_found].nb_args) {
		isp_warn_args(cmd_found, arg_count, args);
	}

	switch (isp_cmds_list[cmd_found].cmd_num) {
		case 0: /* unlock */
			ret = isp_cmd_unlock(0);
			break;
		case 8: /* read-part-id */
			ret = isp_cmd_part_id(0);
			break;
		case 9: /* read-boot-version */
			ret = isp_cmd_boot_version();
			break;
		case 11: /* read-uid */
			ret = isp_cmd_read_uid();
			break;
		default:
			ret = isp_cmds_list[cmd_found].handler(arg_count, args);
	}

	return ret;
}

