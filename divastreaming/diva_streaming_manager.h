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
#ifndef __DIVA_STREAMING_MANAGER_H__
#define __DIVA_STREAMING_MANAGER_H__

struct _diva_streaming_vector;
struct _diva_segment_alloc;

typedef struct _diva_stream {
	diva_streaming_idi_result_t (*release)(struct _diva_stream* ifc); /**< destroy stream */
	diva_streaming_idi_result_t (*release_stream)(struct _diva_stream* ifc); /**< destroy stream */
	diva_streaming_idi_result_t (*write)(struct _diva_stream* ifc, dword message, const void* data, dword length); /**< write data to stream */
	diva_streaming_idi_result_t (*wakeup)(struct _diva_stream* ifc);
	const byte* (*description)(struct _diva_stream* ifc, const byte* addie, byte addielength);
	diva_streaming_idi_result_t (*sync)(struct _diva_stream* ifc, dword ident);
	diva_streaming_idi_result_t (*flush_stream)(struct _diva_stream* ifc);
	dword (*get_tx_free)(const struct _diva_stream* ifc);
	dword (*get_tx_in_use)(const struct _diva_stream* ifc);
	void  (*notify_os_resource_removed)(struct _diva_stream* ifc);
} diva_stream_t;

/*
	Message field length one byte
	*/
#define DIVA_STREAM_MESSAGE_TX_DATA        0x00000000 /** Tx data */
#define DIVA_STREAM_MESSAGE_TX_DATA_ACK    0x00000001 /** Tx data with ack */
#define DIVA_STREAM_MESSAGE_TX_IDI_REQUEST 0x00000002 /** Tx IDI request, request is passed in bits 8...15 */

#define DIVA_STREAM_MESSAGE_RX_DATA    0x00000000 /** Received data */
#define DIVA_STREAM_MESSAGE_RX_TX_FREE 0x00000001 /** Tx space available */
#define DIVA_STREAM_MESSAGE_RX_TX_ACK  0x00000002 /** Received Tx Ack message */
#define DIVA_STREAM_MESSAGE_INIT       0x00000003 /** Stream init complete */
#define DIVA_STREAM_MESSAGE_SYNC_ACK   0x00000004 /** Received stream sync ack */
#define DIVA_STREAM_MESSAGE_RELEASE_ACK 0x00000005 /** Received stream release acknowledge */
#define DIVA_STREAM_MESSAGE_INIT_ERROR 0x00000007 /** Stream init error */
#define DIVA_STREAM_MESSAGE_RELEASED   0x00000008 /** Not message, used internally */

#define DIVA_STREAMING_MANAGER_HOST_USER_MODE_STREAM   0x40000000U
#define DIVA_STREAMING_MANAGER_TX_COUNTER_IN_TX_PAGE   0x20000000U /* Tx counter is located at end of TX page */

diva_streaming_idi_result_t diva_stream_create_with_user_segment_alloc (struct _diva_stream** ifc,
																																				void* os_context,
																																				dword length,
																																				diva_streaming_idi_rx_notify_user_proc_t rx,
																																				void* rx_context,
																																				const char* trace_ident,
																																				struct _diva_segment_alloc* user_segment_alloc);
diva_streaming_idi_result_t diva_stream_create (struct _diva_stream** ifc,
																								void* os_context,
																								dword length,
																								diva_streaming_idi_rx_notify_user_proc_t rx,
																								void* rx_context,
																								const char* trace_ident);

dword diva_streaming_read_vector_data (const diva_streaming_vector_t* v, int nr_v, dword *vector_offset, dword *vector_position, byte* dst, dword length);
dword diva_streaming_get_indication_data (dword handle, dword message, dword length, const diva_streaming_vector_t* v, int nr_v, byte* pInd, diva_streaming_vector_t* vind, int *pvind_nr);





#endif
