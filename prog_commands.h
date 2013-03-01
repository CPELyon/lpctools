/*********************************************************************
 *
 *   LPC ISP Commands for flash tool
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 *
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/

#ifndef ISP_CMDS_FLASH_H
#define ISP_CMDS_FLASH_H

#include "parts.h"

int dump_to_file(struct part_desc* part, char* filename);

int erase_flash(struct part_desc* part);

int flash_target(struct part_desc* part, char* filename, int check);

int get_ids(void);

int start_prog(struct part_desc* part);


#endif /* ISP_CMDS_FLASH_H */

