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



