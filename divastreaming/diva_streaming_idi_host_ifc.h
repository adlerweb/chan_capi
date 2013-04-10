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
#ifndef __DIVA_STREAMING_IDI_HOST_IFC__
#define __DIVA_STREAMING_IDI_HOST_IFC__

/*
	Write data from host to remote side
	*/
struct _diva_stream;
struct _diva_segment_alloc_ifc;
struct _diva_streaming_idi_host_ifc_w;
struct _diva_streaming_idi_host_ifc_w_access;
struct _diva_streaming_idi_host_ifc_r;
struct _diva_streaming_idi_host_ifc_r_access;


typedef struct _diva_streaming_idi_host_ifc_w_access {
	int (*release)(struct _diva_streaming_idi_host_ifc_w* ifc);
	diva_streaming_idi_result_t (*release_stream)(struct _diva_streaming_idi_host_ifc_w* ifc);
	int (*write_message)(struct _diva_streaming_idi_host_ifc_w* ifc,
											 dword info, const void* data, dword data_length);
	int (*ack)(struct _diva_streaming_idi_host_ifc_w* ifc, dword length);
	int (*ack_rx)(struct _diva_streaming_idi_host_ifc_w* ifc, dword length, int flush_ack);
	int (*write)(struct _diva_streaming_idi_host_ifc_w* ifc, const void* data, dword data_length);
	int (*write_indirect)(struct _diva_streaming_idi_host_ifc_w* ifc, dword lo, dword hi, dword length);
	int (*update_remote)(struct _diva_streaming_idi_host_ifc_w* ifc);
	dword (*get_in_use)(const struct _diva_streaming_idi_host_ifc_w* ifc);
	dword (*get_free)(const struct _diva_streaming_idi_host_ifc_w* ifc);
	dword (*get_length)(const struct _diva_streaming_idi_host_ifc_w* ifc);
	byte (*description)(struct _diva_streaming_idi_host_ifc_w* ifc, byte* dst, byte max_length);
	int (*init)(struct _diva_streaming_idi_host_ifc_w* ifc, dword version, dword counter, dword info);
	diva_streaming_idi_result_t (*sync)(struct _diva_streaming_idi_host_ifc_w* ifc, dword ident);
	diva_streaming_idi_result_t (*trace_ident)(struct _diva_streaming_idi_host_ifc_w* ifc);
} diva_streaming_idi_host_ifc_w_access_t;

int diva_streaming_idi_host_ifc_create (struct _diva_streaming_idi_host_ifc_w** ifc,
																				dword number_segments,
																				struct _diva_segment_alloc* segment_alloc,
																				const char* trace_ident);
struct _diva_streaming_idi_host_ifc_w_access* diva_streaming_idi_host_ifc_get_access (
																											struct _diva_streaming_idi_host_ifc_w* ifc);

typedef struct _diva_streaming_idi_host_ifc_r_access {
	int (*release)(struct _diva_streaming_idi_host_ifc_r* ifc);
	int (*wakeup)(struct _diva_streaming_idi_host_ifc_r* ifc);
	byte (*description)(struct _diva_streaming_idi_host_ifc_r* ifc, byte* dst, byte max_length);
} diva_streaming_idi_host_ifc_r_access_t;

int diva_streaming_idi_host_rx_ifc_create (struct _diva_streaming_idi_host_ifc_r** ifc,
																					 dword number_segments,
																					 struct _diva_segment_alloc* segment_alloc,
																					 struct _diva_streaming_idi_host_ifc_w* tx,
																					 struct _diva_streaming_idi_host_ifc_w_access* tx_ifc,
																					 diva_streaming_idi_rx_notify_user_proc_t notify_user_proc,
																					 void* user_context,
																					 struct _diva_stream* diva_streaming_manager_ifc,
																					 const char* trace_ident);
struct _diva_streaming_idi_host_ifc_r_access* diva_streaming_idi_host_rx_ifc_get_access (
                                                      struct _diva_streaming_idi_host_ifc_r* ifc);

#endif
