/*
 *
  Copyright (c) Dialogic (R) 2009 - 2010
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Dialogic (R) File Revision :    1.9
 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef __DIVA_SPI_DESCRIPTOR_H__
#define __DIVA_SPI_DESCRIPTOR_H__


#define MAX_SPI_DESCRIPTOR_LENGTH     (4*1024)
#define MAX_XCONN_PER_DSP_DESCRIPTORS 8
#define SMALL_FLAT_RX_BUFFER_LENGTH   128

/*
	SPI descriptor is always transferred in two parts:
	byte* msg;

	xmit(&msg[4], length-4);
	xmit(msg, 4);

	The second transfer is used as indication about end of the
	descriptor transfer.
	*/

typedef struct _diva_spi_msg_hdr {
	byte length_lo; /* Total transfer length, includes the the length of the header, lo byte */
	byte length_hi; /* Total transfer length, includes the length of the header, h byte      */
	byte Id;        /* IDI Id. 0xff in case of the system message */
	byte Ind;       /* Ind */
} diva_spi_msg_hdr_t;

typedef struct _diva_spi_msg {
	diva_spi_msg_hdr_t hdr;
	byte data[MAX_SPI_DESCRIPTOR_LENGTH-sizeof(diva_spi_msg_hdr_t)];
} diva_spi_msg_t;

/*
	System messages:
	if Id == 0xff then system message
	if (length_hi & 0x80) then system message is chained after the message

	Messages:
	byte - message type
	byte - message length without this byte and the length byte
	body - sequence of bytes, depends on the type of message
	*/

/*
	Used for comminication with
	monitor task
	*/
typedef struct _diva_dsp_monitor_message_hdr {
	byte length_lo; /* Total transfer length, includes the the length of the header, lo byte */
	byte length_hi; /* Total transfer length, includes the length of the header, h byte      */
	byte message_lo;
	byte message_hi;
} diva_dsp_monitor_message_hdr_t;

typedef struct _diva_dsp_monitor_message {
	diva_dsp_monitor_message_hdr_t hdr;
	byte data[MAX_SPI_DESCRIPTOR_LENGTH-sizeof(diva_dsp_monitor_message_hdr_t)];
} diva_dsp_monitor_message_t;

/*
	Format of the request:
	length_lo
	length_hi   - set to "data_length + 5"
	Id
	Req         - Stored in the Ind field
	RegCh				- Req Ch stored in the first byte of data field
	data        - first byte of data
*/

/*
	Format of the return code without extended info:
	length_lo
	length_hi  - set to 6
	Id
	Rc         - Stored in the Ind field
	RcCh       - RcCh stored in the first byte of data field
	type       - Type. Set to zero for RC, stored in the second byte of data field

	Format of the return code with extended info:
	length_lo
	length_hi  - set to 14
	Id
	Rc         - Stored in the Ind field
	RcCh       - RcCh stored in the first byte of data field
	type       - Type. Set to zero for RC, stored in the second byte of data field
	dword      - extended info type (bytes 2,3,4,5 of data field)
	dword      - extended info (bytes 6,7,8,9 of data field)
*/

/*
	Format of the indication:
	length_lo
	length_hi  -  set to "data_lengt + 6"
	Id
	Ind        - Stored in the Ind field
	IndCh      - IndCh stored in the first byte of data field
	type       - Type. Set to one for IND, stored in the second byte of data field
	data       - first byte of data, starts at third data byte
*/

typedef struct _diva_xconn_msg_hdr {
  word counter; 
  word length; 
} diva_xconn_msg_hdr_t;

typedef struct _diva_xconn_msg {
	diva_xconn_msg_hdr_t hdr;
	byte data[MAX_SPI_DESCRIPTOR_LENGTH-sizeof(diva_xconn_msg_hdr_t)];
} diva_xconn_msg_t;

#endif
