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

#include <stdint.h>

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
};

struct part_desc* find_part(uint64_t dev_id);

#endif /* FIND_PART_H */

