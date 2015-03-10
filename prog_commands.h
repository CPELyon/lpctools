/*********************************************************************
 *
 *   LPC ISP Commands for flash tool
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

#ifndef ISP_CMDS_FLASH_H
#define ISP_CMDS_FLASH_H

#include "parts.h"

int dump_to_file(struct part_desc* part, char* filename);

int erase_flash(struct part_desc* part);

int flash_target(struct part_desc* part, char* filename, int check_user_code);

int get_ids(void);

int start_prog(struct part_desc* part);


#endif /* ISP_CMDS_FLASH_H */

