/*********************************************************************
 *
 *   LPC Parts identification
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

#include <stdint.h> /* uint64_t and uint32_t */
#include <stdlib.h> /* NULL */
#include <stddef.h> /* For offsetof */
#include <stdio.h>  /* perror(), fopen(), fclose() */
#include <errno.h>

#include "parts.h"

#define CONF_READ_BUF_SIZE 250

/* When looking for parts description in a file ee do allocate (malloc) two memory
 *   chunks which we will never free.
 * The user should free part_desc->name and part_desc when they are no more useful
 */
struct part_desc* find_part_in_file(uint64_t dev_id, char* parts_file_name)
{
	FILE* parts_file = NULL;
	unsigned int i = 0;
	unsigned int line = 0; /* Store current line number when reading file */
	struct part_desc* part = malloc(sizeof(struct part_desc));

	parts_file = fopen(parts_file_name, "r");
	if (parts_file == NULL) {
		perror("Unable to open file to read LPC descriptions !");
		return NULL;
	}

	while (1) {
		char buf[CONF_READ_BUF_SIZE];
		char* endp = NULL;
		uint32_t* part_values = NULL;
		unsigned int nval = 0;

		if (fgets(buf, CONF_READ_BUF_SIZE, parts_file) == NULL) {
			printf("Part not found before end of parts description file.\n");
			goto out_err_1;
		}
		line++; /* Store current line number to help parts description file correction */
		if (buf[0] == '#' || buf[0] == '\n') {
			/* skip comments */
			continue;
		}
		part->part_id = strtoul(buf, &endp, 0);
		if (part->part_id != dev_id) {
			continue;
		}
		printf("Part ID 0x%08x found on line %d\n", (unsigned int)part->part_id, line);
		/* We found the right part ID, get all the data */
		part->name = malloc(PART_NAME_LENGTH);
		/* Part names must start with "LPC" */
		while ((*endp != '\0') && (*endp != 'L')) {
			endp++;
		}
		/* Copy part name */
		for (i = 0; i < PART_NAME_LENGTH; i++) {
			if (endp[i] == '\0') {
				/* Hey, we need the part description after the name */
				printf("Malformed parts description file at line %d, nothing after part name.\n", line);
				goto out_err_2;
			}
	 		if (endp[i] == ',') {
				break;
			}
			part->name[i] = endp[i];
		}
		if ((i == PART_NAME_LENGTH) && (endp[i] != ',')) {
			printf("Malformed parts description file at line %d, part name too long.\n", line);
				goto out_err_2;
		}
		endp += i;
		/* Get all the values */
		nval = ((sizeof(struct part_desc) - offsetof(struct part_desc, flash_base)) / sizeof(uint32_t));
		part_values = &(part->flash_base); /* Use a table to read the data, we do not care of what the data is */
		for (i = 0; i < nval; i++) {
			errno = 0;
			part_values[i] = strtoul((endp + 1), &endp, 0);
			if ((part_values[i] == 0) && (errno == EINVAL)) {
				printf("Malformed parts description file at line %d, error reading value %d\n", line, i);
				goto out_err_2;
			}
		}
		/* Done, retrun the part ! */
		return part;
	}


out_err_2:
	free(part->name);
out_err_1:
	free(part);
	fclose(parts_file);
	return NULL;
}

