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

#include <stdint.h> /* uint64_t */
#include <stdlib.h> /* NULL */
#include "parts.h"

struct part_desc parts[] = {
	/*        part info             |        flash            | reset  |            ram                  */
	/*                              |                     nb  | vector |                      buffer     */
	/*  part_id      part name      | base addr    size  sect | offset | base addr   size    off   size  */
	{ 0x2540102B, "LPC1114FHN33/302", 0x00000000, 0x8000, 8,    0x04,    0x10000000, 0x2000, 0x800, 0x400 },
	{0, NULL, 0, 0, 0, 0, 0, 0, 0, 0},
};


struct part_desc* find_part(uint64_t dev_id)
{
	int i = 0;

	while (parts[i].name != NULL) {
		if (parts[i].part_id == dev_id) {
			return &parts[i];
		}
		i++;
	}

	return NULL;
}

