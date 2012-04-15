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
	
	if (min_read > buf_size) {
		printf("serial_read: buffer too small for min read value.\n");
		return -3;
	}

	do {
		nb = read(serial_fd, &buf[count], (buf_size - count));
		if (nb < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
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

/* FIXME : This is a place-holder forr uuencode and uudecode functions !!! */
int isp_uu_encode(char* dest, char* src, unsigned int orig_size)
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

int isp_uu_decode(char* dest, char* src, unsigned int orig_size)
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

