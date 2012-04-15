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


#ifndef ISP_UTILS_H
#define ISP_UTILS_H

void isp_dump(const unsigned char* buf, unsigned int buf_size);


/* ---- Serial utility functions ---------------------------------------------------*/

/* Open the serial device and set it up.
 * Returns 0 on success, negativ value on error.
 * Actal setup is done according to LPC11xx user's manual.
 * Only baudrate can be changed using command line option.
 */
int isp_serial_open(int baudrate, char* serial_device);
void isp_serial_close(void);

/* Simple write() wrapper, with trace if enabled */
int isp_serial_write(const char* buf, unsigned int buf_size);

/* Try to read at least "min_read" characters from the serial line.
 * Returns -1 on error, 0 on end of file, or read count otherwise.
 */
int isp_serial_read(char* buf, unsigned int buf_size, unsigned int min_read);


/* ---- UU_Encoding utility functions ----------------------------------------------*/

/* FIXME : This is a place-holder forr uuencode and uudecode functions !!! */
int isp_uu_encode(char* dest, char* src, unsigned int orig_size);

int isp_uu_decode(char* dest, char* src, unsigned int orig_size);

#endif /* ISP_UTILS_H */
