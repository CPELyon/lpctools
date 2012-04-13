/*********************************************************************
 *
 *   LPC1114 ISP Commands
 *
 *********************************************************************/

/* List of commands to be supported :
  synchronize
  unlock 
  set-baud-rate
  echo
  write-to-ram
  read-memory
  prepare-for-write
  copy-ram-to-flash
  go
  erase
  blank-check
  read-part-id
  read-boot-version
  compare
  read-uid
*/

#ifndef ISP_COMMADS_H
#define ISP_COMMADS_H

int isp_ret_code(char* buf);

/* Connect or reconnect to the target.
 * Return positive or NULL value when connection is OK, or negative value otherwise.
 */
int isp_connect(unsigned int crystal_freq);

/* Handle one command
 * Return positive or NULL value when command handling is OK, or negative value otherwise.
 */
int isp_handle_command(char* cmd, int arg_count, char** args);

#endif /* ISP_COMMADS_H */

