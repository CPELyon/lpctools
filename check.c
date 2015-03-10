/*********************************************************************
 *
 *   ISP (In-System Programming) companion tool for NXP LPC Microcontrollers
 *
 * This tool allows user-code computation and CRP protection check for NXP LPC
 * microcontrollers.
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

#include <string.h> /* strncmp, strlen, strdup */

#include "isp_utils.h"

#define PROG_NAME "LPC binary check"
#define VERSION   "1.07"


void help(char *prog_name)
{
	fprintf(stderr, "---------------- "PROG_NAME" --------------------------------\n");
	fprintf(stderr, "Usage: %s [options] [prog file name]\n" \
		"  Available options:\n" \
		"  \t -n | --no-user-code : do not compute a valid user code for exception vector 7\n" \
		"  \t -s | --skip-crp-verification : do not perform CRP code verification\n" \
		"  \t -h | --help : display this help\n" \
		"  \t -v | --version : display version information\n", prog_name);
	fprintf(stderr, "-----------------------------------------------------------------------\n");
}

int trace_on = 0;

static int calc_user_code = 1; /* User code is computed by default */
static int check_crp = 1; /* CRP is checked by default */

int main(int argc, char** argv)
{
	char* filename = NULL;
	char* data = NULL;
	struct stat stat_buffer;
	int size = 0;
	uint32_t* v = NULL; /* Used for checksum computing */
	uint32_t cksum = 0;
	uint32_t crp = 0;
	int ret = 0;

	/* parameter parsing */
	while(1) {
		int option_index = 0;
		int c = 0;

		struct option long_options[] = {
			{"no-user-code", no_argument, 0, 'n'},
			{"skip-crp-verification", no_argument, 0, 's'},
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "nshv", long_options, &option_index);

		/* no more options to parse */
		if (c == -1) break;

		switch (c) {
			/* n, user-code */
			case 'n':
				calc_user_code = 0;
				break;

			/* s, skip CRP verification */
			case 's':
				check_crp = 0;
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
				return -1;
		}
	}

	/* Parse remaining command line arguments (not options). It should be the binary file name */
	if ((optind >= argc) || ((argc - optind) != 1)) {
		printf("Need one (and only one) file name to check.\n");
		return -1;
	}
	filename = argv[optind];

	/* Get information on the file */
	if (stat(filename, &stat_buffer) == -1) {
		perror("stat failed");
		printf("Unable to get informations on file \"%s\"\n", filename);
		return -2;
	}
	/* Allocate a buffer big enougth to get the CRP in it */
	data = malloc(stat_buffer.st_size);
	if (data == NULL) {
		printf("Unable to get a buffer to load the image!\n");
		return -2;
	}
	size = isp_file_to_buff(data, stat_buffer.st_size, filename);
	if (size <= 0){
		free(data);
		return -2;
	}

	/* And check checksum of first 7 vectors if asked, according to section 21.3.3 of
	 * LPC11xx user's manual (UM10398) */
	v = (uint32_t *)data;
	cksum = 0 - v[0] - v[1] - v[2] - v[3] - v[4] - v[5] - v[6];
	if (calc_user_code == 1) {
		v[7] = cksum;
		isp_buff_to_file(data, size, filename);
		printf("Checksum check done. File updated.\n");
	} else if (cksum != v[7]) {
		printf("Checksum is 0x%08x, should be 0x%08x\n", v[7], cksum);
		ret = -3;
	}

	if (check_crp == 1) {
		crp = v[(CRP_OFFSET / 4)];
		printf("CRP : 0x%08x\n", crp);
		if ((crp == CRP_NO_ISP) || (crp == CRP_CRP1) || (crp == CRP_CRP2) || (crp == CRP_CRP3)) {
			printf("CRP : 0x%08x\n", crp);
			printf("The binary has CRP protection ativated, which violates GPLv3.\n");
			printf("Check the licence for the software you are about to flash.\n"); 
			ret = -4;
		}
	}

	free(data);
	return ret;
}

