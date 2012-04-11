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


/* display data as in hexdump -C :
   00000000  7f 45 4c 46 02 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|
 */
void isp_dump(const unsigned char* buf, unsigned int buf_size)
{
	unsigned int count = 0;
	char pass = 0;
	while ((count < buf_size) || (pass == 0)) {
		if (count == buf_size) {
			while (count & 0x0F) {
				printf("   ");
				count++;
			}
			pass = 1;
			count -= 0x10;
		}
		if ((count % 0x10) == 0) {
			if (pass == 0) {
				printf(" %08x  ", count);
			} else {
				printf(" |");
			}
		}
		if (pass == 0) {
			printf("%02x ", buf[count]);
		} else {
			if (isgraph((int)buf[count])) {
				printf("%c", buf[count]);
			} else {
				printf(".");
			}
		}
		count++;
		if ((count % 0x10) == 0) {
			if (pass == 0) {
				count -= 0x10;
			} else {
				printf("|\n");
			}
			pass = !pass;
		}
	}
	printf("|\n");
}


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

