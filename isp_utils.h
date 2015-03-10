/*********************************************************************
 *
 *   LPC ISP - Utility functions
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


#ifndef ISP_UTILS_H
#define ISP_UTILS_H


/* ---- CRP Protection values ---------------------------------------------------*/
#define CRP_OFFSET  0x000002FC
#define CRP_NO_ISP  0x4E697370
#define CRP_CRP1    0x12345678
#define CRP_CRP2    0x87654321
#define CRP_CRP3    0x43218765


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

void isp_serial_empty_buffer();
/* Try to read at least "min_read" characters from the serial line.
 * Returns -1 on error, 0 on end of file, or read count otherwise.
 */
int isp_serial_read(char* buf, unsigned int buf_size, unsigned int min_read);


/* ---- UU_Encoding utility functions ----------------------------------------------*/
int isp_uu_encode(char* dest, char* src, unsigned int orig_size);

int isp_uu_decode(char* dest, char* src, unsigned int orig_size);


/* ---- File utility functions ----------------------------------------------*/
int isp_buff_to_file(char* data, unsigned int len, char* filename);

int isp_file_to_buff(char* data, unsigned int len, char* filename);

#endif /* ISP_UTILS_H */
