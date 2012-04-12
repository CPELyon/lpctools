/*********************************************************************
 *
 *   LPC1114 ISP Commands
 *
 *********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* List of commands to be supported :
  synchronize : OK
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


#include "isp_utils.h"

extern int trace_on;

#define SERIAL_BUFSIZE  128

#define SYNCHRO_START "?"
#define SYNCHRO  "Synchronized"


int isp_ret_code(char* buf)
{
	if (trace_on) {
		/* FIXME : Find how return code are sent (binary or ASCII) */
		printf("Received code: '0x%02x'\n", *buf);
	}
	return 0;
}


/* Connect or reconnect to the target.
 * Return positive or NULL value when connection is OK, or negative value otherwise.
 */
int isp_connect()
{
	char buf[SERIAL_BUFSIZE];

	/* Send synchronize request */
	if (isp_serial_write(SYNCHRO_START, strlen(SYNCHRO_START)) != strlen(SYNCHRO_START)) {
		printf("Unable to send synchronize request.\n");
		return -5;
	}

	/* Wait for answer */
	if (isp_serial_read(buf, SERIAL_BUFSIZE, strlen(SYNCHRO)) < 0) {
		printf("Error reading synchronize answer.\n");
		return -4;
	}

	/* Check answer, and acknoledge if OK */
	if (strncmp(SYNCHRO, buf, strlen(SYNCHRO)) == 0) {
		isp_serial_write(SYNCHRO, strlen(SYNCHRO));
	} else {
		printf("Unable to synchronize.\n");
		return -1;
	}

	/* Empty read buffer (maybe echo is on ?) */
	usleep( 5000 );
	isp_serial_read(buf, SERIAL_BUFSIZE, 0);

	/* FIXME : Do we always turn off echo ? */
	/* and turn off echo */
	isp_serial_write("A 0\r\n", 5);

	return 1;
}

/* Handle one command
 * Return positive or NULL value when command handling is OK, or negative value otherwise.
 */
int isp_handle_command(char* cmd, int arg_count, char** args)
{
	return 1;
}


