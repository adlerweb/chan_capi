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
#ifndef __DIVA_STREAMING_IDI_HOST_IFC_IMPL_H__
#define __DIVA_STREAMING_IDI_HOST_IFC_IMPL_H__

struct _diva_segment_alloc_ifc;
struct _diva_streaming_idi_host_ifc_w_access;
struct _diva_streaming_idi_host_ifc_w;

#define DIVA_STREAMING_IDI_HOST_IFC_MAX_SEGMENTS 8
typedef struct _diva_streaming_idi_host_ifc_w {
	struct _diva_streaming_idi_host_ifc_w_access access;

	byte* segments[DIVA_STREAMING_IDI_HOST_IFC_MAX_SEGMENTS]; /**< buffer segments */
	dword segment_length[DIVA_STREAMING_IDI_HOST_IFC_MAX_SEGMENTS]; /**< length of every segment */
	dword segment_lo[DIVA_STREAMING_IDI_HOST_IFC_MAX_SEGMENTS];
	dword segment_hi[DIVA_STREAMING_IDI_HOST_IFC_MAX_SEGMENTS];
	dword  nr_segments; /**< number of available segments */

	struct _diva_segment_alloc* segment_alloc;

	struct {
		dword length; /**< overall length including all segments */
		dword used_length; /**< overall written and not acknowledged */
		dword free_length; /**< overall length available */

		dword write_buffer; /**< current write segment buffer */
		dword write_buffer_position; /**< position in write segment */
		dword write_buffer_free; /**< free space in write segment */

		dword acknowledge_buffer; /**< acknowledge segment */
		dword acknowledge_buffer_position; /**< position in acknowledge segment */
		dword acknowledge_buffer_free;
	} state;

	struct {
		int32 written; /**< incremented every time data written by amount of data written in the buffer and
                      written to opposite side */
	} remote;

	dword remote_counter_base;    /* Remote counter page BUS address */
	dword remote_counter_offset;  /* Remote counter offset from page start */
	void* remote_counter_mapped_base; /* Remote counter page BUS address mapped */
	volatile dword* remote_counter_mapped; /* Remote counter mapped */

	dword ack_rx;

	int free_ifc;

	char trace_ident[DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH+1];
} diva_streaming_idi_host_ifc_w_t;

#endif
