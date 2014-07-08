/*********************************************************************
 *
 *   LPC ISP Commands
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

#ifndef ISP_COMMANDS_H
#define ISP_COMMANDS_H


extern char* error_codes[];

#define CMD_SUCCESS 0
#define INVALID_COMMAND 1
#define SRC_ADDR_ERROR 2
#define DST_ADDR_ERROR 3
#define SRC_ADDR_NOT_MAPPED 4
#define DST_ADDR_NOT_MAPPED 5
#define COUNT_ERROR 6
#define INVALID_SECTOR 7
#define SECTOR_NOT_BLANK 8
#define SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION 9
#define COMPARE_ERROR 10
#define BUSY 11
#define PARAM_ERROR 12
#define ADDR_ERROR 13
#define ADDR_NOT_MAPPED 14
#define CMD_LOCKED 15
#define INVALID_CODE 16
#define INVALID_BAUD_RATE 17
#define INVALID_STOP_BIT 18
#define CODE_READ_PROTECTION_ENABLED 19


/* Connect or reconnect to the target.
 * Return positive or NULL value when connection is OK, or negative value otherwise.
 */
int isp_connect(unsigned int crystal_freq, int quiet);


/*
 * Helper functions
 */
int isp_send_cmd_no_args(char* cmd_name, char* cmd, int quiet);
int isp_send_cmd_two_args(char* cmd_name, char cmd, unsigned int arg1, unsigned int arg2);
int isp_send_cmd_address(char cmd, uint32_t addr1, uint32_t addr2, uint32_t length, char* name);
int isp_send_cmd_sectors(char* name, char cmd, int first_sector, int last_sector, int quiet);


int isp_cmd_unlock(int quiet);

int isp_cmd_read_uid(void);

int isp_cmd_part_id(int quiet);

int isp_cmd_boot_version(void);

/*
 * read-memory
 * aruments : address count file
 * read 'count' bytes from 'address', store then in 'file'
 */
int isp_cmd_read_memory(int arg_count, char** args);
/*
 * perform read-memory operation
 * read 'count' bytes from 'addr' to 'data' buffer
 */
int isp_read_memory(char* data, uint32_t addr, unsigned int count, unsigned int uuencoded);

/*
 * write-to-ram
 * aruments : address file
 * send 'file' to 'address' in ram
 */
int isp_cmd_write_to_ram(int arg_count, char** args);
/*
 * perform write-to-ram operation
 * send 'count' bytes from 'data' to 'addr' in RAM
 */
int isp_send_buf_to_ram(char* data, unsigned long int addr, unsigned int count, unsigned int perform_uuencode);


int isp_cmd_compare(int arg_count, char** args);

int isp_cmd_copy_ram_to_flash(int arg_count, char** args);

/*
 * go
 * aruments : address mode
 * execute program at 'address' (> 0x200) in 'mode' ('arm' or 'thumb')
 */
int isp_cmd_go(int arg_count, char** args);
/*
 * perform go operation
 * start user program at 'addr' in 'mode'
 * mode is 'T' for thumb or 'A' for arm.
 */
int isp_send_cmd_go(uint32_t addr, char mode);

int isp_cmd_blank_check(int arg_count, char** args);

int isp_cmd_prepare_for_write(int arg_count, char** args);

int isp_cmd_erase(int arg_count, char** args);


#endif /* ISP_COMMANDS_H */

