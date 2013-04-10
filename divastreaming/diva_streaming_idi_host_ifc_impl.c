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
/*
 * vim:ts=2:
 */
#include "platform.h"
#include "pc.h"
#include "diva_streaming_result.h"
#include "diva_streaming_vector.h"
#include "diva_streaming_messages.h"
#include "diva_streaming_manager.h"
#include "diva_streaming_result.h"
#include "diva_segment_alloc_ifc.h"
#include "diva_streaming_idi_host_ifc.h"
#include "diva_streaming_idi_host_ifc_impl.h"
#include "spi_descriptor.h"

static int diva_streaming_idi_host_ifc_cleanup (struct _diva_streaming_idi_host_ifc_w* ifc);
static int write_message (struct _diva_streaming_idi_host_ifc_w* ifc,
													dword info,
													const void* data,
													dword data_length);
static byte description (diva_streaming_idi_host_ifc_w_t* ifc, byte* dst, byte max_length);
static int init (struct _diva_streaming_idi_host_ifc_w* ifc, dword version, dword counter, dword info);
static diva_streaming_idi_result_t sync_req (struct _diva_streaming_idi_host_ifc_w* ifc, dword ident);
static int ack_data (struct _diva_streaming_idi_host_ifc_w* ifc, dword length);
static int ack_rx_data (struct _diva_streaming_idi_host_ifc_w* ifc, dword length, int flush_ack);
static int write_data (struct _diva_streaming_idi_host_ifc_w* ifc, const void* data, dword data_length);
static int write_indirect (struct _diva_streaming_idi_host_ifc_w* ifc, dword lo, dword hi, dword length);
static int update_remote (struct _diva_streaming_idi_host_ifc_w* ifc);
static dword get_in_use (const struct _diva_streaming_idi_host_ifc_w* ifc);
static dword get_free (const struct _diva_streaming_idi_host_ifc_w* ifc);
static dword get_length (const struct _diva_streaming_idi_host_ifc_w* ifc);
static void write_buffer (diva_streaming_idi_host_ifc_w_t* ifc, const void* data, dword data_length);
static void update_write_buffer (diva_streaming_idi_host_ifc_w_t* ifc);
static diva_streaming_idi_result_t set_trace_ident (struct _diva_streaming_idi_host_ifc_w* ifc);
static diva_streaming_idi_result_t release_stream (struct _diva_streaming_idi_host_ifc_w* ifc);


int diva_streaming_idi_host_ifc_create (struct _diva_streaming_idi_host_ifc_w** ifc,
                                        dword number_segments,
                                        struct _diva_segment_alloc* segment_alloc,
																				const char* trace_ident) {
	struct _diva_streaming_idi_host_ifc_w* ifc_w = *ifc;
	diva_segment_alloc_access_t* segment_alloc_access = diva_get_segment_alloc_ifc (segment_alloc);
	dword i;
	int free_ifc = 0;

	if (ifc_w == 0) {
		ifc_w = diva_os_malloc (0, sizeof(*ifc_w));
		free_ifc = 1;
	}

	if (ifc_w == 0) {
		DBG_ERR(("failed to create idi w interface [%s]", trace_ident))
		return (-1);
	}

	memset (ifc_w, 0x00, sizeof(*ifc_w));
	memcpy(ifc_w->trace_ident, trace_ident, MIN(strlen(trace_ident), DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH));
	ifc_w->trace_ident[DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH] = 0;

	ifc_w->nr_segments = MIN(number_segments, DIVA_STREAMING_IDI_HOST_IFC_MAX_SEGMENTS);
	ifc_w->segment_alloc = segment_alloc;
	ifc_w->free_ifc = free_ifc;

	for (i = 0; i < ifc_w->nr_segments; i++) {
		ifc_w->segments[i] = (*(segment_alloc_access->segment_alloc))(segment_alloc,
													&ifc_w->segment_lo[i],
													&ifc_w->segment_hi[i]);
		if (ifc_w->segments[i] == 0) {
			DBG_ERR(("failed to alloc segment [%s]", trace_ident))
			diva_streaming_idi_host_ifc_cleanup (ifc_w);
			return (-1);
		}

		DBG_TRC(("alloc %p %08x:%08x [%s]", ifc_w->segments[i], ifc_w->segment_lo[i], ifc_w->segment_hi[i], trace_ident))

		ifc_w->segment_length[i] = (*(segment_alloc_access->get_segment_length))(segment_alloc);
		if (i == 0) {
			*(volatile dword*)(ifc_w->segments[i]+ifc_w->segment_length[i]-sizeof(dword)) = 0;
		}

		ifc_w->state.length += ifc_w->segment_length[i];
	}

	ifc_w->ack_rx = 0;

	ifc_w->state.used_length = 0;
	ifc_w->state.free_length = ifc_w->state.length;

	ifc_w->state.write_buffer = 0;
	ifc_w->state.write_buffer_position = 0;
	ifc_w->state.write_buffer_free =  ifc_w->segment_length[ifc_w->state.write_buffer];

	ifc_w->state.acknowledge_buffer = 0;
	ifc_w->state.acknowledge_buffer_position = 0;

	ifc_w->remote.written = 0;

	ifc_w->access.release = diva_streaming_idi_host_ifc_cleanup;
	ifc_w->access.release_stream = release_stream;
	ifc_w->access.write_message = write_message;
	ifc_w->access.ack = ack_data;
	ifc_w->access.ack_rx = ack_rx_data;
	ifc_w->access.write = write_data;
	ifc_w->access.write_indirect = write_indirect;
	ifc_w->access.update_remote = update_remote;
	ifc_w->access.get_in_use = get_in_use;
	ifc_w->access.get_free   = get_free;
	ifc_w->access.get_length = get_length;
	ifc_w->access.description = description;
	ifc_w->access.init = init;
	ifc_w->access.sync = sync_req;
	ifc_w->access.trace_ident = set_trace_ident;

	*ifc = ifc_w;

	return (0);
}

struct _diva_streaming_idi_host_ifc_w_access* diva_streaming_idi_host_ifc_get_access (
                                                      struct _diva_streaming_idi_host_ifc_w* ifc) {
	return (&ifc->access);
}

static int diva_streaming_idi_host_ifc_cleanup (struct _diva_streaming_idi_host_ifc_w* ifc) {
	diva_segment_alloc_access_t* segment_alloc_access = diva_get_segment_alloc_ifc (ifc->segment_alloc);
	dword i;

  for (i = 0; i < ifc->nr_segments; i++) {
		if (ifc->segments[i] != 0) {
			(*(segment_alloc_access->segment_free))(ifc->segment_alloc,
																				ifc->segments[i],
																				ifc->segment_lo[i],
																				ifc->segment_hi[i]);
		}
  }

	if (ifc->remote_counter_mapped_base != 0) {
		segment_alloc_access->umap_address (ifc->segment_alloc, ifc->remote_counter_base, 0, ifc->remote_counter_mapped_base);
	}

	if (ifc->free_ifc != 0) {
		diva_os_free (0, ifc);
	}

	return (0);
}

static void update_write_buffer (diva_streaming_idi_host_ifc_w_t* ifc) {
	if (ifc->state.write_buffer_free == 0) {
		ifc->state.write_buffer++;
		if (ifc->state.write_buffer >= ifc->nr_segments) {
			ifc->state.write_buffer = 0;
		}
		ifc->state.write_buffer_free     = ifc->segment_length[ifc->state.write_buffer];
		ifc->state.write_buffer_position = 0;
	}
}

static void write_buffer (diva_streaming_idi_host_ifc_w_t* ifc, const void* data, dword data_length) {
	const byte* src = (const byte*)data;
	dword to_write = data_length;

	while (data_length != 0) {
		dword to_copy = MIN(ifc->state.write_buffer_free, data_length);
		memcpy (ifc->segments[ifc->state.write_buffer]+ifc->state.write_buffer_position, src, to_copy);
		src += to_copy;
		ifc->state.write_buffer_position += to_copy;
		ifc->state.write_buffer_free -= to_copy;
		data_length -= to_copy;
		update_write_buffer (ifc);
	}

	ifc->state.free_length -= to_write;
	ifc->state.used_length += to_write;
	ifc->remote.written    += ((int32)to_write);
}

static void align_write_buffer (diva_streaming_idi_host_ifc_w_t* ifc, dword data_length) {
	dword to_write = data_length;

	while (data_length != 0) {
		dword to_copy = MIN(ifc->state.write_buffer_free, data_length);
		ifc->state.write_buffer_position += to_copy;
		ifc->state.write_buffer_free -= to_copy;
		data_length -= to_copy;
		update_write_buffer (ifc);
	}

	ifc->state.free_length -= to_write;
	ifc->state.used_length += to_write;
	ifc->remote.written    += ((int32)to_write);
}

static int write_message (struct _diva_streaming_idi_host_ifc_w* ifc,
													dword info,
													const void* data,
													dword data_length) {
	dword idi_header_length = ((info & 0xff) == DIVA_STREAM_MESSAGE_TX_IDI_REQUEST && (info & DIVA_STREAMING_IDI_SYSTEM_MESSAGE) == 0) ? (sizeof(diva_spi_msg_hdr_t)+1) : 0;
	dword length = data_length + idi_header_length + 2 * sizeof(dword); /* data length + message length + info */
	dword required_length = (length + 31) & ~31;
	byte tmp[sizeof(dword)+1];
	byte Req = 0;

	if (required_length > get_free (ifc)) {
		return (0);
	}

	WRITE_DWORD(tmp, data_length + idi_header_length + sizeof(dword)); /* Write message length without length dword */
	write_buffer (ifc, tmp, sizeof(dword));

	if ((info & DIVA_STREAMING_IDI_SYSTEM_MESSAGE) == 0) {
		switch (info & 0xff) {
			case DIVA_STREAM_MESSAGE_TX_IDI_REQUEST: {
				dword ack_info = (word)ifc->ack_rx;
				Req = (byte)(info >> 8);
				info = DIVA_STREAMING_IDI_TX_REQUEST | (ack_info << 8);
				ifc->ack_rx -= ack_info;
			} break;
		}
	}

	WRITE_DWORD(tmp, info);
	write_buffer (ifc, tmp, sizeof(dword)); /* Write info */

	if (idi_header_length != 0) {
		diva_spi_msg_hdr_t* hdr = (diva_spi_msg_hdr_t*)tmp;
		dword message_length = data_length + idi_header_length;
		byte* message_data = (byte*)&hdr[1];

		hdr->Id  = 0;
		hdr->Ind = Req;
		hdr->length_lo = (byte)message_length;
		hdr->length_hi = (byte)(message_length >> 8);
		message_data[0] = 0;

		write_buffer (ifc, hdr, idi_header_length);
	}

	write_buffer (ifc, data, data_length); /* Write data */

	align_write_buffer (ifc, required_length - length); /* Move to next message */

	return (data_length);
}

static int ack_data (struct _diva_streaming_idi_host_ifc_w* ifc, dword length) {
	if (length > get_in_use (ifc)) {
		DBG_ERR(("ack error ack:%u in use:%u free:%u [%s]", length, get_in_use (ifc), ifc->state.free_length, ifc->trace_ident))
		return (-1);
	}

	ifc->state.free_length += length;
	ifc->state.used_length -= length;

	return (0);
}

static int ack_rx_data (struct _diva_streaming_idi_host_ifc_w* ifc, dword length, int flush_ack) {
	flush_ack |= (ifc->ack_rx != 0);

	ifc->ack_rx += length;

	while (flush_ack != 0 && ifc->ack_rx != 0 && get_free (ifc) > 128) {
		dword info = (word)ifc->ack_rx;

		ifc->ack_rx -= info;
		info = ((dword)DIVA_STREAMING_IDI_RX_ACK_MSG) | (info << 8) | DIVA_STREAMING_IDI_SYSTEM_MESSAGE;

		write_message (ifc, info, 0, 0);
		update_remote (ifc);
	}

	return (0);
}

static byte description (diva_streaming_idi_host_ifc_w_t* ifc, byte* dst, byte max_length) {
	byte length = 0;
	dword i;

	DBG_TRC(("tx description %u segments [%s]", ifc->nr_segments, ifc->trace_ident))

	dst[length++] = (byte)(ifc->nr_segments);

	for (i = 0; i < ifc->nr_segments; i++) {
		DBG_TRC((" tx lo[%u] %08x [%s]", i, ifc->segment_lo[i], ifc->trace_ident))
		WRITE_DWORD(&dst[length], ifc->segment_lo[i]);
		length += sizeof(dword);
	}
	for (i = 0; i < ifc->nr_segments; i++) {
		DBG_TRC((" tx hi[%u] %08x [%s]", i, ifc->segment_hi[i], ifc->trace_ident))
		WRITE_DWORD(&dst[length], ifc->segment_hi[i]);
		length += sizeof(dword);
	}
	for (i = 0; i < ifc->nr_segments; i++) {
		DBG_TRC((" length[%u] %u [%s]", i, ifc->segment_length[i], ifc->trace_ident))
		WRITE_DWORD(&dst[length], ifc->segment_length[i]);
		length += sizeof(dword);
	}

	return (length);
}

static int init (struct _diva_streaming_idi_host_ifc_w* ifc, dword version, dword counter, dword info) {
	diva_segment_alloc_access_t* segment_alloc_access = diva_get_segment_alloc_ifc (ifc->segment_alloc);

	if ((info & DIVA_STREAMING_MANAGER_TX_COUNTER_IN_TX_PAGE) != 0) {
		ifc->segment_length[0]       -= sizeof(dword);
		ifc->state.length            -= sizeof(dword);
		ifc->state.write_buffer_free -= sizeof(dword);
		ifc->state.free_length       -= sizeof(dword);
		ifc->remote_counter_mapped = (dword*)(ifc->segments[0]+ifc->segment_length[0]);
		ifc->remote_counter_mapped_base = 0;
	} else {
		ifc->remote_counter_offset = counter % (4*1024);
		ifc->remote_counter_base   = counter - ifc->remote_counter_offset;

		ifc->remote_counter_mapped_base = segment_alloc_access->map_address (ifc->segment_alloc,
																					ifc->remote_counter_base, 0,
																					(info & DIVA_STREAMING_MANAGER_HOST_USER_MODE_STREAM) != 0);
		if (ifc->remote_counter_mapped_base != 0) {
			byte* p = ifc->remote_counter_mapped_base;
			ifc->remote_counter_mapped = (dword*)&p[ifc->remote_counter_offset];
		} else {
			DBG_TRC(("stream uses system call [%s]", ifc->trace_ident))
		}
	}

	return (0);
}

static diva_streaming_idi_result_t sync_req (struct _diva_streaming_idi_host_ifc_w* ifc, dword ident) {
	if (get_free (ifc) > 128) {
		dword data[2];

		DBG_TRC(("sync request %08x [%s]", ident, ifc->trace_ident))

		data[0] = ident;
		data[1] = 0;

		write_message (ifc, DIVA_STREAMING_IDI_SYNC_REQ|DIVA_STREAMING_IDI_SYSTEM_MESSAGE, data, sizeof(data));
		update_remote (ifc);

		return (DivaStreamingIdiResultOK);
	} else {
		return (DivaStreamingIdiResultBusy);
	}
}

static diva_streaming_idi_result_t set_trace_ident (struct _diva_streaming_idi_host_ifc_w* ifc) {
	if (get_free (ifc) > 128) {
		dword data[2];
		DBG_TRC(("set trace ident [%s]", ifc->trace_ident))

		memcpy (data, ifc->trace_ident, sizeof(data[0]));
		data[1] = 0;

		write_message (ifc, DIVA_STREAMING_IDI_SET_DEBUG_IDENT|DIVA_STREAMING_IDI_SYSTEM_MESSAGE, data, sizeof(data));
		update_remote (ifc);

		return (DivaStreamingIdiResultOK);
	} else {
		return (DivaStreamingIdiResultBusy);
	}
}

static diva_streaming_idi_result_t release_stream (struct _diva_streaming_idi_host_ifc_w* ifc) {
	dword data[1];
	int ret;

	DBG_LOG(("stream release [%s]", ifc->trace_ident))

	data[0] = 0;

	ret = write_message (ifc, DIVA_STREAMING_IDI_RELEASE|DIVA_STREAMING_IDI_SYSTEM_MESSAGE, data, sizeof(data));
	update_remote (ifc);

	return (ret != 0 ? DivaStreamingIdiResultOK : DivaStreamingIdiResultBusy);
}

static int write_data (struct _diva_streaming_idi_host_ifc_w* ifc, const void* data, dword data_length) {
	return (-1);
}

static int write_indirect (struct _diva_streaming_idi_host_ifc_w* ifc, dword lo, dword hi, dword length) {
	return (-1);
}

static int update_remote (struct _diva_streaming_idi_host_ifc_w* ifc) {
	int ret = 0;

	if (ifc->remote_counter_mapped != 0) {
		ifc->remote_counter_mapped[0] = ifc->remote.written;
	} else {
		diva_segment_alloc_access_t* segment_alloc_access = diva_get_segment_alloc_ifc (ifc->segment_alloc);

		ret = segment_alloc_access->write_address (ifc->segment_alloc,
																							 ifc->remote_counter_base + ifc->remote_counter_offset,
																							 0,
																							 ifc->remote.written);
	}

	return (ret);
}

static dword get_in_use (const struct _diva_streaming_idi_host_ifc_w* ifc) {
	return (ifc->state.used_length);
}

static dword get_free (const struct _diva_streaming_idi_host_ifc_w* ifc) {
	return (ifc->state.free_length);
}

static dword get_length (const struct _diva_streaming_idi_host_ifc_w* ifc) {
	return (ifc->state.length);
}

