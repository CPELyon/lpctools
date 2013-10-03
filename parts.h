/*********************************************************************
 *
 *   LPC Parts identification
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 *
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/

#ifndef FIND_PART_H
#define FIND_PART_H

#include <stdint.h> /* uint64_t and uint32_t */

#define PART_NAME_LENGTH  25

/* Part descriptor */
/* ALL data below name MUST be of type uint32_t */
struct part_desc {
	uint64_t part_id;
	char* name;
	/* Flash */
	uint32_t flash_base;
	uint32_t flash_size;
	uint32_t flash_nb_sectors;
	uint32_t reset_vector_offset;
	/* RAM */
	uint32_t ram_base;
	uint32_t ram_size;
	uint32_t ram_buff_offset; /* Used to transfer data for flashing */
	uint32_t ram_buff_size;
	uint32_t uuencode;
};

/* When looking for parts description in a file ee do allocate (malloc) two memory
 *   chunks which we will never free.
 * The user should free part_desc->name and part_desc when they are no more usefull
 */
struct part_desc* find_part_in_file(uint64_t dev_id, char* conf_file_name);

/* Find a part in the internal parts table */
/* FIXME : To be inplemented ? */
struct part_desc* find_part_internal_tab(uint64_t dev_id);

#endif /* FIND_PART_H */

