/*********************************************************************
 *
 *   LPC1114 ISP - Utility functions
 *
 *
 * Written by Nathael Pajani <nathael.pajani@nathael.net>
 * 
 * This programm is released under the terms of the GNU GPLv3 licence
 * as can be found on the GNU website : <http://www.gnu.org/licenses/>
 *
 *********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h> /* for open, close */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <string.h> /* memcpy */

#include <termios.h> /* serial */
#include <ctype.h>


extern int trace_on;

/* display data as in hexdump -C :
   00000000  7f 45 4c 46 02 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|
 */
void isp_dump(const unsigned char* buf, unsigned int buf_size)
{
	unsigned int count = 0;
	char pass = 0;
	if (buf_size == 0) {
		return;
	}
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
				if (count == buf_size) {
					break; /* prevent infinite loop */
				}
				printf("|\n");
			}
			pass = !pass;
		}
	}
	printf("|\n");
}


/* ---- Serial utility functions ---------------------------------------------------*/

static int serial_fd = -1;

/* Open the serial device and set it up.
 * Returns 0 on success, negativ value on error.
 * Actal setup is done according to LPC11xx user's manual.
 * Only baudrate can be changed using command line option.
 */
int isp_serial_open(int baudrate, char* serial_device)
{
	struct termios tio;

	if (serial_device == NULL) {
		printf("No serial device given on command line\n");
		return -2;
	}

	/* Open serial port */
	serial_fd = open(serial_device, O_RDWR | O_NONBLOCK);
	if (serial_fd < 0) {
		perror("Unable to open serial_device");
		printf("Tried to open \"%s\".\n", serial_device);
		return -1;
	}
	/* Setup serial port */
	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = IXON | IXOFF;  /* See section 21.4.4 of LPC11xx user's manual (UM10398) */
	tio.c_cflag = CS8 | CREAD | CLOCAL;  /* 8n1, see termios.h for more information */
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;
	cfsetospeed(&tio, baudrate);
	cfsetispeed(&tio, baudrate);
	tcsetattr(serial_fd, TCSANOW, &tio);
	
	return 0;
}

void isp_serial_close(void)
{
	close(serial_fd);
}

/* Simple write() wrapper, with trace if enabled */
int isp_serial_write(const char* buf, unsigned int buf_size)
{
	int nb = 0;

	if (trace_on) {
		printf("Sending %d octet(s) :\n", buf_size);
		isp_dump((unsigned char*)buf, buf_size);
	}
	nb = write(serial_fd, buf, buf_size);
	if (nb <= 0) {
		perror("Serial write error");
		return -1;
	}
	return nb;
}

/* Try to read at least "min_read" characters from the serial line.
 * Returns -1 on error, 0 on end of file, or read count otherwise.
 */
int isp_serial_read(char* buf, unsigned int buf_size, unsigned int min_read)
{
	int nb = 0;
	unsigned int count = 0;
	unsigned int loops = 0; /* Used to create a timeout */
	
	if (min_read > buf_size) {
		printf("serial_read: buffer too small for min read value.\n");
		return -3;
	}

	do {
		nb = read(serial_fd, &buf[count], (buf_size - count));
		if (nb < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				if (loops++ > 100) {
					break; /* timeout at 500ms */
				}
				usleep( 5000 );
				continue;
			}
			perror("Serial read error");
			return -1;
		} else if (nb == 0) {
			printf("serial_read: end of file !!!!\n");
			return 0;
		}
		if (trace_on == 2) {
			isp_dump((unsigned char*)(&buf[count]), nb);
		}	
		count += nb;
	} while (count < min_read);

	if (trace_on) {
		printf("Received %d octet(s) :\n", count);
		isp_dump((unsigned char*)buf, count);
	}
	return count;
}


/* ---- UU_Encoding utility functions ----------------------------------------------*/

/* This might have been taken from a lib, but I hate lib dependencies, and installing
 *  a full window manager (or MTA or MUA for instance) for two functions is not an 
 *  option */
#define UUENCODE_ADDED_VAL 32
#define LINE_DATA_LENGTH_MAX 45
#define LINE_LENGTH_NULL 96

int isp_uu_encode(char* dest, char* src, unsigned int orig_size)
{
	unsigned int new_size = 0;
	unsigned int pos = 0;

	while (pos < orig_size) {
		unsigned int line_length = orig_size - pos;
		unsigned int i = 0;
		
		/* Start with line length */
		if (line_length > LINE_DATA_LENGTH_MAX) {
			line_length = LINE_DATA_LENGTH_MAX;
		}
		dest[new_size] = line_length + UUENCODE_ADDED_VAL;
		new_size++;
		
		/* Encode line */
		while (i < line_length) {
			uint32_t int_triplet = 0;
			int j = 0;
			
			/* Get original triplet (three bytes) */
			for (j=0; j<3; j++) {
				int_triplet |= ((src[pos + i + j] & 0xFF) << (8 * (2 - j)));
				/* if not enougth data, leave it as nul */
				if ((i + j) > line_length) {
					break;
				}
			}
			for (j=0; j<4; j++) {
				/* Store triplet in four bytes */
				dest[new_size + j] = ((int_triplet >> (6 * (3 - j))) & 0x3F);
				/* Add offset */
				dest[new_size + j] += UUENCODE_ADDED_VAL;
			}
			i += 3;
			new_size += 4;
		}
		pos += line_length;

		/* Add \r\n */
		dest[new_size++] = '\r';
		dest[new_size++] = '\n';
	}
	return new_size;
}

int isp_uu_decode(char* dest, char* src, unsigned int orig_size)
{
	unsigned int new_size = 0;
	unsigned int pos = 0;

	while (pos < orig_size) {
		unsigned int line_length = 0;
		unsigned int i = 0;
		int j = 0;

		/* Read line length */
		line_length = src[pos] - UUENCODE_ADDED_VAL;
		if (src[pos] == LINE_LENGTH_NULL) {
			/* Empty line ? then we are done converting
			 * (should not happen in communication with ISP) */
			return new_size;
		}
		pos++;

		/* Decode line */
		while (i < line_length) {
			char quartet[4];
			uint32_t int_triplet = 0;
			/* copy data */
			memcpy(quartet, &src[pos], 4);
			pos += 4;
			/* Get the original bits */
			for (j=0; j<4; j++) {
				/* Remove the offset added by uuencoding */
				quartet[j] -= UUENCODE_ADDED_VAL;
				int_triplet |= ((quartet[j] & 0x3F) << ((3 - j) * 6));
			}
			/* And store them */
			for (j=2; j>=0; j--) {
				dest[new_size++] = ((int_triplet >> (j * 8)) & 0xFF );
				i++;
				if (i >= line_length) {
					break;
				}
			}
		}
		
		/* Find next line */
		while ((src[pos] < UUENCODE_ADDED_VAL) && (pos < orig_size))   {
			pos++;
		}
	}
	return new_size;
}

