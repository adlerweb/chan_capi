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
#ifndef __DIVA_STREAMING_IDI_HOST_RX_IFC_IMPL_H__
#define __DIVA_STREAMING_IDI_HOST_RX_IFC_IMPL_H__


struct _diva_segment_alloc_ifc;
struct _diva_streaming_idi_host_ifc_r_access;
struct _diva_streaming_idi_host_ifc_r;


#define DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS 8
typedef struct _diva_streaming_idi_host_ifc_r {
	struct _diva_streaming_idi_host_ifc_r_access access;

	byte* segments[DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS]; /**< buffer segments */
	dword segment_length[DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS]; /**< length of every segment */
	dword segment_lo[DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS];
	dword segment_hi[DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS];
	dword  nr_segments; /**< number of available segments */

	volatile int32* remote_counter; /**< updated by remote side, located at begin of first segment */
	volatile int32  local_counter;

	dword current_segment;
	dword current_pos;
	dword current_free;

	struct _diva_segment_alloc* segment_alloc;

	struct _diva_streaming_idi_host_ifc_w* tx;
	struct _diva_streaming_idi_host_ifc_w_access* tx_ifc;
	diva_streaming_idi_rx_notify_user_proc_t notify_user_proc;
	void* user_context;

	byte system_message[512];

	int free_ifc;

	struct {
		dword length; /**< overall length including all segments */
	} state;

	struct _diva_stream* diva_streaming_manager_ifc;
	dword released;
	dword released_info;

	char trace_ident[DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH+1];
} diva_streaming_idi_host_ifc_r_t;




#endif
