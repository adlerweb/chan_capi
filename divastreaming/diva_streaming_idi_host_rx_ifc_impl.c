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
#include "diva_streaming_vector.h"
#include "diva_streaming_messages.h"
#include "diva_streaming_result.h"
#include "diva_streaming_manager.h"
#include "diva_streaming_result.h"
#include "diva_segment_alloc_ifc.h"
#include "diva_streaming_idi_host_ifc.h"
#include "diva_streaming_idi_host_rx_ifc_impl.h"

/*
 * LOCALS
 */
static void update_buffer (struct _diva_streaming_idi_host_ifc_r* ifc);
static void align_buffer (struct _diva_streaming_idi_host_ifc_r* ifc, dword length, diva_streaming_vector_t* v, dword* nr_v);
static int diva_streaming_idi_host_rx_data (struct _diva_streaming_idi_host_ifc_r* ifc, dword length);
static dword copy_message_data (byte* dst, const diva_streaming_vector_t* v, dword nr_v);
static int diva_streaming_idi_host_rx_process_message (struct _diva_streaming_idi_host_ifc_r* ifc,
																											 dword message,
																											 dword data_length,
																											 const diva_streaming_vector_t* v, dword nr_v);
static byte description (diva_streaming_idi_host_ifc_r_t* ifc, byte* dst, byte max_length);
static int diva_streaming_idi_host_rx_ifc_rx (struct _diva_streaming_idi_host_ifc_r* ifc);
static int diva_streaming_idi_host_rx_ifc_cleanup (struct _diva_streaming_idi_host_ifc_r* ifc);

int diva_streaming_idi_host_rx_ifc_create (struct _diva_streaming_idi_host_ifc_r** ifc,
																					 dword number_segments,
																					 struct _diva_segment_alloc* segment_alloc,
																					 struct _diva_streaming_idi_host_ifc_w* tx,
																					 struct _diva_streaming_idi_host_ifc_w_access* tx_ifc,
																					 diva_streaming_idi_rx_notify_user_proc_t notify_user_proc,
																					 void* user_context,
																					 struct _diva_stream* diva_streaming_manager_ifc,
																					 const char* trace_ident) {
	struct _diva_streaming_idi_host_ifc_r* ifc_r = *ifc;
	diva_segment_alloc_access_t* segment_access = diva_get_segment_alloc_ifc (segment_alloc);
	dword i;
	int free_ifc = 0;

	if (ifc_r == 0) {
		ifc_r = diva_os_malloc (0, sizeof(*ifc_r));
		free_ifc = 1;
	}

	if (ifc_r == 0) {
		DBG_ERR(("failed to create idi r interface [%s]", trace_ident))
		return (-1);
	}

	memset (ifc_r, 0x00, sizeof(*ifc_r));
	memcpy(ifc_r->trace_ident, trace_ident, MIN(strlen(trace_ident), DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH));
	ifc_r->trace_ident[DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH] = 0;

	ifc_r->nr_segments = MIN(number_segments, DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS);
	ifc_r->segment_alloc = segment_alloc;
	ifc_r->free_ifc = free_ifc;
	ifc_r->diva_streaming_manager_ifc = diva_streaming_manager_ifc;

	for (i = 0; i < ifc_r->nr_segments; i++) {
		ifc_r->segments[i] = (*(segment_access->segment_alloc))(segment_alloc,
													&ifc_r->segment_lo[i],
													&ifc_r->segment_hi[i]);
		if (ifc_r->segments[i] == 0) {
			DBG_ERR(("failed to alloc segment [%s]", trace_ident))
			diva_streaming_idi_host_rx_ifc_cleanup (ifc_r);
			return (-1);
		}


		DBG_TRC(("alloc %p %08x:%08x [%s]", ifc_r->segments[i], ifc_r->segment_lo[i], ifc_r->segment_hi[i], trace_ident))

		ifc_r->segment_length[i] = (*(segment_access->get_segment_length))(segment_alloc);

		if (i == 0) {
			ifc_r->remote_counter     = (int32*)ifc_r->segments[i];
			ifc_r->segments[i]       += sizeof(dword);
			ifc_r->segment_length[i] -= sizeof(dword);
		}

		ifc_r->state.length += ifc_r->segment_length[i];
	}

	ifc_r->remote_counter[0] = 0;

	ifc_r->tx               = tx;
	ifc_r->tx_ifc           = tx_ifc;
	ifc_r->notify_user_proc = notify_user_proc;
	ifc_r->user_context     = user_context;

	ifc_r->current_segment = 0;
	ifc_r->current_pos = 0;
	ifc_r->current_free = ifc_r->segment_length[ifc_r->current_segment];

	ifc_r->access.release     = diva_streaming_idi_host_rx_ifc_cleanup;
	ifc_r->access.wakeup      = diva_streaming_idi_host_rx_ifc_rx;
	ifc_r->access.description = description;

	*ifc = ifc_r;

	return (0);
}

struct _diva_streaming_idi_host_ifc_r_access* diva_streaming_idi_host_rx_ifc_get_access (
                                                      struct _diva_streaming_idi_host_ifc_r* ifc) {
	return (&ifc->access);
}

static int diva_streaming_idi_host_rx_ifc_cleanup (struct _diva_streaming_idi_host_ifc_r* ifc) {
	diva_segment_alloc_access_t* segment_access = diva_get_segment_alloc_ifc (ifc->segment_alloc);
	dword i;

  for (i = 0; i < ifc->nr_segments; i++) {
		if (ifc->segments[i] != 0) {
			(*(segment_access->segment_free))(ifc->segment_alloc,
																				ifc->segments[i] - ((i == 0) ? sizeof(dword) : 0),
																				ifc->segment_lo[i],
																				ifc->segment_hi[i]);
		}
  }

	if (ifc->free_ifc != 0) {
		diva_os_free (0, ifc);
	}

	return (0);
}

/*
 * Receive data
 */
static int diva_streaming_idi_host_rx_ifc_rx (struct _diva_streaming_idi_host_ifc_r* ifc) {
	int32 local_counter;
	int32 length;
	int ret = 0;
	int msg_count = 0, ack_ret;

	do {
		local_counter = ifc->remote_counter[0];
		length = local_counter - ifc->local_counter;

		if (length != 0) {
			ret += length;
			ifc->local_counter = local_counter;
			ack_ret = diva_streaming_idi_host_rx_data (ifc, length);
			if (ifc->released == DIVA_STREAM_MESSAGE_RELEASE_ACK || ifc->released == DIVA_STREAM_MESSAGE_INIT_ERROR) {
				if (ifc->notify_user_proc (ifc->user_context, ((dword)ifc->released) << 8 | 0xffU, ifc->released_info, 0, 0) == 0) {
					ifc->diva_streaming_manager_ifc->release(ifc->diva_streaming_manager_ifc);
				} else {
					ifc->released = DIVA_STREAM_MESSAGE_RELEASED;
				}
				return (0);
			}
			ifc->tx_ifc->ack_rx (ifc->tx, length, msg_count != 0 || ack_ret == 0);
			msg_count++;
		}
	} while(length != 0);

	return (ret);
}

static void update_buffer (struct _diva_streaming_idi_host_ifc_r* ifc) {
	if (ifc->current_free == 0) {
		ifc->current_segment++;
		if (ifc->current_segment >= ifc->nr_segments) {
			ifc->current_segment = 0;
		}
		ifc->current_free = ifc->segment_length[ifc->current_segment];
		ifc->current_pos  = 0;
	}
}

static void align_buffer (struct _diva_streaming_idi_host_ifc_r* ifc, dword length, diva_streaming_vector_t* v, dword* nr_v) {
	while (length != 0) {
		dword to_copy = MIN(ifc->current_free, length);

		if (v != 0) {
			v[*nr_v].data   = ifc->segments[ifc->current_segment] + ifc->current_pos;
			v[*nr_v].length = to_copy;
			nr_v[0]++;
		}

		ifc->current_pos  += to_copy;
		ifc->current_free -= to_copy;
		length            -= to_copy;
		update_buffer (ifc);
	}
}

static dword copy_message_data (byte* dst, const diva_streaming_vector_t* v, dword nr_v) {
	dword i, length;

	for (i = 0, length = 0; i < nr_v; i++) {
		memcpy (&dst[length], v[i].data, v[i].length);
		length += v[i].length;
	}

	return (length);
}

/*
	Return one if processed DIVA_STREAMING_IDI_TX_ACK_MSG
	*/
static int diva_streaming_idi_host_rx_process_message (struct _diva_streaming_idi_host_ifc_r* ifc,
																											 dword message,
																											 dword data_length,
																											 const diva_streaming_vector_t* v, dword nr_v) {
	int ret = 0;

	switch (message) {
		case DIVA_STREAMING_IDI_TX_INIT_MSG: /* Initialization */
			if (data_length != (DIVA_STREAMING_IDI_TX_INIT_MSG_LENGTH - sizeof(dword) - sizeof(word))) {
				DBG_ERR(("wrong message length %02x %u [%s]", DIVA_STREAMING_IDI_TX_INIT_MSG, data_length, ifc->trace_ident))
				return (0);
			}
			{
				dword counter;
				dword info;
				byte version;

				copy_message_data (ifc->system_message, v, nr_v);

				version = ifc->system_message[0];
				counter = READ_DWORD(&ifc->system_message[1]);
				info    = READ_DWORD(&ifc->system_message[5]);

				DBG_TRC(("version:%u counter:%08x info:%08x [%s]", version, counter, info, ifc->trace_ident))

				ifc->tx_ifc->init (ifc->tx, version, counter, info);
				ifc->tx_ifc->trace_ident (ifc->tx);
				ifc->notify_user_proc (ifc->user_context, DIVA_STREAM_MESSAGE_INIT << 8 | 0xffU, 0, 0, 0);
			}
			break;

		case DIVA_STREAMING_IDI_TX_ACK_MSG:
			if (data_length < (DIVA_STREAMING_IDI_TX_ACK_MSG_LENGTH - sizeof(dword) - sizeof(word))) {
				DBG_ERR(("wrong message length %02x %u [%s]", DIVA_STREAMING_IDI_TX_ACK_MSG, data_length, ifc->trace_ident))
				return (0);
			}
			copy_message_data (ifc->system_message, v, nr_v);
			DBG_TRC(("Ind:%02x %u seq:%u [%s]",
							DIVA_STREAMING_IDI_TX_ACK_MSG,
							((word)ifc->system_message[0]) | (((word)ifc->system_message[1]) << 8),
							ifc->system_message[2],
							ifc->trace_ident))
			ifc->tx_ifc->ack (ifc->tx, ((word)ifc->system_message[0]) | (((word)ifc->system_message[1]) << 8));
			ret = 1;
			ifc->notify_user_proc (ifc->user_context, DIVA_STREAM_MESSAGE_RX_TX_ACK << 8 | 0xffU, 0, 0, 0);
			break;

		case DIVA_STREAMING_IDI_TX_SYNC_ACK:
			if (data_length != (DIVA_STREAMING_IDI_TX_SYNC_ACK_LENGTH - sizeof(dword) - sizeof(word))) {
				DBG_ERR(("wrong message length %02x %u [%s]", DIVA_STREAMING_IDI_TX_SYNC_ACK, data_length, ifc->trace_ident))
				return (0);
			}
			copy_message_data (ifc->system_message, v, nr_v);
			{
				dword ident = READ_DWORD(&ifc->system_message[0]);
				DBG_TRC(("sync ack %08x [%s]", ident, ifc->trace_ident))

				ifc->notify_user_proc (ifc->user_context, DIVA_STREAM_MESSAGE_SYNC_ACK << 8 | 0xffU, ident, 0, 0);
			}
			break;

		case DIVA_STREAMING_IDI_RELEASE_ACK:
			if (data_length != (DIVA_STREAMING_IDI_RELEASE_ACK_LENGTH - sizeof(dword) - sizeof(word))) {
				DBG_ERR(("wrong message length %02x %u [%s]", DIVA_STREAMING_IDI_RELEASE_ACK, data_length, ifc->trace_ident))
				return (0);
			}
			DBG_LOG(("stream release ack [%s]", ifc->trace_ident))
			ifc->released = DIVA_STREAM_MESSAGE_RELEASE_ACK;
			ifc->released_info = 0;
			break;

		case DIVA_STREAMING_IDI_TX_INIT_ERROR:
			if (data_length < DIVA_STREAMING_IDI_TX_INIT_ERROR_LENGTH - sizeof(dword) - sizeof(word) || data_length >= sizeof(ifc->system_message)) {
				DBG_ERR(("wrong message length %02x %u [%s]", DIVA_STREAMING_IDI_TX_INIT_ERROR, data_length, ifc->trace_ident))
			}
			copy_message_data (ifc->system_message, v, nr_v);
			{
				dword error = READ_DWORD(&ifc->system_message[0]);
				DBG_LOG(("stream init error %08x [%s]", error, ifc->trace_ident))
				ifc->released = DIVA_STREAM_MESSAGE_INIT_ERROR;
				ifc->released_info = error;
			}
			break;

			default:
				DBG_ERR(("unknown message %08x %u [%s]", message, data_length, ifc->trace_ident))
				break;
	}

	return (ret);
}

/*
	Return one of only one system ack tx message was processed
	*/
static int diva_streaming_idi_host_rx_data (struct _diva_streaming_idi_host_ifc_r* ifc, dword length) {
	diva_streaming_vector_t v[DIVA_STREAMING_IDI_HOST_RX_IFC_MAX_SEGMENTS+2];
	dword nr_v;
	int msg_count = 0, ack_msg = 0;


	while (length != 0) {
		dword hdr = *(dword*)(ifc->segments[ifc->current_segment] + ifc->current_pos);
		dword data_length_lo = hdr & 0xffU;
		dword data_length_hi = (hdr >> 8) & 0xffU;
		dword data_length = data_length_lo | (data_length_hi << 8);
		dword message_length =  ((data_length + (sizeof(dword)-1)) & ~(sizeof(dword)-1));
		dword message_type = (hdr >> 16) & 0xff;
		dword message      = (hdr >> 24) & 0xff;

		length      -= (message_length + sizeof(dword)-1) & ~(sizeof(dword)-1);

		nr_v = 0;
		if (message_type != 0xff && (message & 0x0f) == 0x0f) {
			/*
				Tx ack sent as part of IDI N_COMBI_IND message
				*/
			byte tmp[sizeof(dword)+sizeof(word)];
			dword tx_ack;

			align_buffer (ifc, sizeof(dword)+sizeof(word), v, &nr_v); /* Header */
			copy_message_data (tmp, v, nr_v);
			tx_ack = ((dword)tmp[4]) | (((dword)tmp[5]) << 8);

			if (tx_ack != 0) {
				ifc->tx_ifc->ack (ifc->tx, tx_ack);
				ifc->notify_user_proc (ifc->user_context, DIVA_STREAM_MESSAGE_RX_TX_ACK << 8 | 0xffU, 0, 0, 0);
			}

			nr_v = 0;
		} else {
			align_buffer (ifc, sizeof(dword)+sizeof(word), 0, 0); /* Header */
		}
		align_buffer (ifc, data_length - sizeof(dword) - sizeof(word), v, &nr_v);

		if (message_type == 0xff) {
			ack_msg |= diva_streaming_idi_host_rx_process_message (ifc, message, data_length - sizeof(dword) - sizeof(word), v, nr_v);
			if (ifc->released != 0)
				return (1);
		} else if (nr_v != 0) {
			ifc->notify_user_proc (ifc->user_context, message << 8, data_length - sizeof(dword) - sizeof(word), v, nr_v);
		}
		align_buffer (ifc, message_length - data_length, 0, 0);
		msg_count++;
	}

	return (msg_count == 1 && ack_msg != 0);
}

static byte description (diva_streaming_idi_host_ifc_r_t* ifc, byte* dst, byte max_length) {
	byte length = 0;
	dword i;

	DBG_TRC(("rx description %u segments [%s]", ifc->nr_segments, ifc->trace_ident))

	for (i = 0; i < ifc->nr_segments; i++) {
		DBG_TRC((" rx lo[%u] %08x [%s]", i, ifc->segment_lo[i], ifc->trace_ident))
		WRITE_DWORD(&dst[length], ifc->segment_lo[i]);
		length += sizeof(dword);
	}
	for (i = 0; i < ifc->nr_segments; i++) {
		DBG_TRC((" rx hi[%u] %08x [%s]", i, ifc->segment_hi[i], ifc->trace_ident))
		WRITE_DWORD(&dst[length], ifc->segment_hi[i]);
		length += sizeof(dword);
	}

	return (length);
}


