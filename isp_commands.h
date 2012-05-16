/*********************************************************************
 *
 *   LPC1114 ISP Commands
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 * 
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/

#ifndef ISP_COMMANDS_H
#define ISP_COMMANDS_H



/* Connect or reconnect to the target.
 * Return positive or NULL value when connection is OK, or negative value otherwise.
 */
int isp_connect(unsigned int crystal_freq);



int isp_send_buf_to_ram(char* data, unsigned long int addr, unsigned int count);


int isp_cmd_unlock(int quiet);

int isp_cmd_read_uid(void);

int isp_cmd_part_id(int quiet);

int isp_cmd_boot_version(void);

int isp_cmd_read_memory(int arg_count, char** args);

int isp_cmd_write_to_ram(int arg_count, char** args);

int isp_cmd_compare(int arg_count, char** args);

int isp_cmd_copy_ram_to_flash(int arg_count, char** args);

int isp_cmd_go(int arg_count, char** args);

int isp_cmd_blank_check(int arg_count, char** args);

int isp_cmd_prepare_for_write(int arg_count, char** args);

int isp_cmd_erase(int arg_count, char** args);


#endif /* ISP_COMMANDS_H */

