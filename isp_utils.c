/*********************************************************************
 *
 *   LPC1114 ISP - Utility functions
 *
 *********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h> /* for open, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>

#include <termios.h>
#include <ctype.h>
#include <errno.h>


/* FIXME : This is a place-holder forr uuencode and uudecode functions !!! */
int uu_encode(unsigned char* dest, unsigned char* src, int orig_size)
{
	int new_size = 0;

	while (orig_size--) {
		if (*src) {
			*dest++ = *src++;
			new_size++;
		} else {
		}
	}
	return new_size;
}
int uu_decode(unsigned char* dest, unsigned char* src, int orig_size)
{
	int new_size = 0;
	while (orig_size--) {
		if (*src) {
			*dest++ = *src++;
		} else {
		}
		new_size++;
	}
	return new_size;
}

