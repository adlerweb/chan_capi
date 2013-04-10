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
#include "platform.h"
#include "pc.h"
#include "diva_streaming_result.h"
#include "diva_streaming_vector.h"
#include "diva_streaming_manager.h"
#include "diva_streaming_messages.h"
#include "diva_segment_alloc_ifc.h"
#include "diva_streaming_idi_host_ifc.h"


typedef struct _diva_stream_manager {
	diva_stream_t ifc;
	int user_segment_alloc;
	struct _diva_segment_alloc* segment_alloc;
	diva_segment_alloc_access_t* segment_alloc_ifc;
	struct _diva_streaming_idi_host_ifc_w* tx;
	struct _diva_streaming_idi_host_ifc_w_access* tx_ifc;
	struct _diva_streaming_idi_host_ifc_r* rx;
	struct _diva_streaming_idi_host_ifc_r_access* rx_ifc;
	diva_streaming_idi_rx_notify_user_proc_t rx_proc;
	void* rx_proc_context;

	byte description[0xff-32];

	char trace_ident[DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH+1];
} diva_stream_manager_t;

/*
 * LOCALS
 */
static int message_input_proc (void* user_context, dword message, dword length, const diva_streaming_vector_t* v, dword nr_v);
static diva_streaming_idi_result_t diva_stream_manager_release (struct _diva_stream* ifc);
static diva_streaming_idi_result_t diva_stream_manager_release_stream (struct _diva_stream* ifc);
static diva_streaming_idi_result_t diva_stream_manager_write (struct _diva_stream* ifc, dword message, const void* data, dword length);
static diva_streaming_idi_result_t diva_stream_manager_wakeup (struct _diva_stream* ifc);
static const byte* diva_stream_manager_description (struct _diva_stream* ifc, const byte* addie, byte addielen);
static diva_streaming_idi_result_t diva_stream_manager_sync_req (struct _diva_stream* ifc, dword ident);
static diva_streaming_idi_result_t diva_stream_flush (struct _diva_stream* ifc);
static dword diva_stream_get_tx_free (const struct _diva_stream* ifc);
static dword diva_stream_get_tx_in_use (const struct _diva_stream* ifc);
static void diva_notify_os_resource_removed (struct _diva_stream* ifc);

diva_streaming_idi_result_t diva_stream_create (struct _diva_stream** ifc,
																								void* os_context,
																								dword length,
																								diva_streaming_idi_rx_notify_user_proc_t rx,
																								void* rx_context,
																								const char* trace_ident) {
	return (diva_stream_create_with_user_segment_alloc (ifc, os_context, length, rx, rx_context, trace_ident, 0));
}

diva_streaming_idi_result_t diva_stream_create_with_user_segment_alloc (struct _diva_stream** ifc,
																																				void* os_context,
																																				dword length,
																																				diva_streaming_idi_rx_notify_user_proc_t rx,
																																				void* rx_context,
																																				const char* trace_ident,
																																				struct _diva_segment_alloc* user_segment_alloc) {
	diva_stream_manager_t* pI;
	diva_streaming_idi_result_t ret = DivaStreamingIdiResultError;

#ifdef DIVA_USERMODE
	dbg_init ("DIVA STREAM", "109-1", 0);
#endif

	pI = diva_os_malloc (0, sizeof(*pI));

	if (pI != 0) {
		memset (pI, 0x00, sizeof(*pI));
		memcpy(pI->trace_ident, trace_ident, MIN(strlen(trace_ident), DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH));
		pI->trace_ident[DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH] = 0;

		if (user_segment_alloc != 0) {
			pI->segment_alloc = user_segment_alloc;
			pI->user_segment_alloc = 1;
			ret = 0;
		} else {
			ret = diva_create_segment_alloc  (os_context, &pI->segment_alloc);
			pI->user_segment_alloc = 0;
		}

		if (ret == 0) {
			dword number_segments = length/4096 + ((length%4096) != 0);

			pI->segment_alloc_ifc = diva_get_segment_alloc_ifc (pI->segment_alloc);

			ret = diva_streaming_idi_host_ifc_create (&pI->tx, number_segments, pI->segment_alloc, trace_ident);
			if (ret == 0) {
				pI->tx_ifc = diva_streaming_idi_host_ifc_get_access (pI->tx);

				ret = diva_streaming_idi_host_rx_ifc_create (&pI->rx, number_segments, pI->segment_alloc, pI->tx, pI->tx_ifc, message_input_proc, pI, &pI->ifc, trace_ident);
				if (ret == 0) {
					pI->rx_ifc = diva_streaming_idi_host_rx_ifc_get_access (pI->rx);

					pI->ifc.release     = diva_stream_manager_release;
					pI->ifc.release_stream     = diva_stream_manager_release_stream;
					pI->ifc.write       = diva_stream_manager_write;
					pI->ifc.wakeup      = diva_stream_manager_wakeup;
					pI->ifc.description = diva_stream_manager_description;
					pI->ifc.sync        = diva_stream_manager_sync_req;
					pI->ifc.flush_stream = diva_stream_flush;
					pI->ifc.get_tx_free = diva_stream_get_tx_free;
					pI->ifc.get_tx_in_use = diva_stream_get_tx_in_use;
					pI->ifc.notify_os_resource_removed = diva_notify_os_resource_removed;

					pI->rx_proc         = rx;
					pI->rx_proc_context = rx_context;

					*ifc = &pI->ifc;
					ret = DivaStreamingIdiResultOK;
				} else {
					DBG_ERR(("failed to create rx stream [%s]", trace_ident))
				}
			} else {
				DBG_ERR(("failed to create tx stream [%s]", trace_ident))
			}
		} else {
			DBG_ERR(("failed to create segment alloc [%s]", trace_ident))
		}
	}

	if (ret != 0)
		diva_stream_manager_release (&pI->ifc);

	return (ret);
}

static int message_input_proc (void* user_context, dword message, dword length, const diva_streaming_vector_t* v, dword nr_v) {
	diva_stream_manager_t* pI = (diva_stream_manager_t*)user_context;

	return (pI->rx_proc (pI->rx_proc_context, message, length, v, nr_v));
}

static diva_streaming_idi_result_t diva_stream_manager_release (struct _diva_stream* ifc) {
	if (ifc != 0) {
		diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

		if (pI->rx != 0)
			pI->rx_ifc->release (pI->rx);
		if (pI->tx != 0)
			pI->tx_ifc->release (pI->tx);
		if (pI->segment_alloc != 0 && pI->user_segment_alloc == 0)
			pI->segment_alloc_ifc->release (&pI->segment_alloc);
		diva_os_free (0, pI);
	}

	return (DivaStreamingIdiResultOK);
}

static diva_streaming_idi_result_t diva_stream_manager_release_stream (struct _diva_stream* ifc) {
	if (ifc != 0) {
		diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

		return (pI->tx_ifc->release_stream (pI->tx));
	}

	return (DivaStreamingIdiResultError);
}

static diva_streaming_idi_result_t diva_stream_manager_write (struct _diva_stream* ifc, dword message, const void* data, dword length) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

	return (pI->tx_ifc->write_message (pI->tx, message, data, length));
}

static diva_streaming_idi_result_t diva_stream_manager_wakeup (struct _diva_stream* ifc) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

	return (pI->rx_ifc->wakeup (pI->rx));
}

const byte* diva_stream_manager_description (struct _diva_stream* ifc, const byte* addie, byte addielen) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);
	byte length = 4, len_tx, len_rx;

	len_tx = pI->tx_ifc->description (pI->tx, &pI->description[length], sizeof(pI->description)-length);

	if (len_tx != 0) {
		length += len_tx;
		len_rx = pI->rx_ifc->description (pI->rx, &pI->description[length], sizeof(pI->description)-length);
		if (len_rx != 0) {
			length += len_rx;
			pI->description[0] = length-1; /* Structure length */
			pI->description[1] = 0; /* Request */
			pI->description[2] = len_rx+len_tx+1;  /* Structure length */
			pI->description[3] = 0; /* Version */

			if (addie != 0 && addielen != 0) {
				byte* description = &pI->description[0];
				byte* start = &description[3];

				start[0] |= 2U;
				start = start + start[-1];
				memcpy (start, addie, addielen);
				start += addielen;
				*start++ = 0;

				description[2] += addielen+1;
				description[0] += addielen+1;
				length         += addielen+1;
			}

			DBG_TRC(("description length %u [%s]", length, pI->trace_ident))
			DBG_BLK(((void*)pI->description, length))

			return (&pI->description[0]);
		}
	}

	return (0);
}

static diva_streaming_idi_result_t diva_stream_manager_sync_req (struct _diva_stream* ifc, dword ident) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

	return (pI->tx_ifc->sync (pI->tx, ident));
}

static diva_streaming_idi_result_t diva_stream_flush (struct _diva_stream* ifc) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

	return (pI->tx_ifc->update_remote (pI->tx) == 0 ? DivaStreamingIdiResultOK : DivaStreamingIdiResultError);
}

static dword diva_stream_get_tx_free (const struct _diva_stream* ifc) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

	return (pI->tx_ifc->get_free (pI->tx));
}

static dword diva_stream_get_tx_in_use (const struct _diva_stream* ifc) {
	const diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, const diva_stream_manager_t, ifc);

	return (pI->tx_ifc->get_in_use (pI->tx));
}

static void diva_notify_os_resource_removed (struct _diva_stream* ifc) {
	diva_stream_manager_t* pI = DIVAS_CONTAINING_RECORD(ifc, diva_stream_manager_t, ifc);

	if (pI->segment_alloc != 0) {
		pI->segment_alloc_ifc->resource_removed (pI->segment_alloc);
	}
}

dword diva_streaming_read_vector_data (const diva_streaming_vector_t* v, int nr_v, dword *vector_offset, dword *vector_position, byte* dst, dword length) {
	dword to_copy;
	dword count;

	for (count = 0; vector_offset[0] < ((dword)nr_v) && length != 0;) {
		to_copy = MIN((v[vector_offset[0]].length - vector_position[0]), length);
		if (dst != 0) {
			const byte* tmp = v[vector_offset[0]].data;
			memcpy (dst, &tmp[vector_position[0]], to_copy);
			dst += to_copy;
		}
		length -= to_copy;
		count  += to_copy;
		if (v[vector_offset[0]].length == vector_position[0]+to_copy) {
			vector_offset[0]++;
			vector_position[0]=0;
		} else {
			vector_position[0] += to_copy;
		}
	}

	return (count);
}


dword diva_streaming_get_indication_data (dword handle, dword message, dword length, const diva_streaming_vector_t* v, int nr_v, byte* pInd, diva_streaming_vector_t* vind, int *pvind_nr) {
	dword vector_offset = (byte)handle;
	dword vector_position = handle >> 8;
	byte Ind = (byte)(message >> 8);
	byte header[4];
	dword indication_data_length;
	int dst_nr_v = *pvind_nr;
	int i;

	if ((Ind & 0x0f) == 8 /* N_DATA */) {
		*pInd = Ind;
		for (i = 0; i < nr_v; i++) {
			vind[i] = v[i];
		}
		*pvind_nr = nr_v;
		return (0);
	}

/*
	Combined indication
		data[0] = Ind;
		data[1] = IndCh;
		data[2] = (byte)length;
		data[3] = (byte)(length >> 8);
	*/
	if (vector_offset >= ((dword)nr_v) || vector_position >= v[vector_offset].length) {
		DBG_ERR(("%s at %d wrong combined indication format", __FILE__, __LINE__))
		*pInd = 0;
		return (0);
	}
	{
		const byte* tmp = v[vector_offset].data;
		if (tmp[vector_position] == 0) {
			DBG_ERR(("%s at %d wrong combined indication format", __FILE__, __LINE__))
			*pInd = 0;
			return (0);
		}
	}
	if (diva_streaming_read_vector_data (v, nr_v, &vector_offset, &vector_position, header, sizeof(header)) != sizeof(header)) {
		DBG_ERR(("%s at %d wrong combined indication format", __FILE__, __LINE__))
		*pInd = 0;
		return (0);
	}
	indication_data_length =  ((word)header[2] | (word)header[3] << 8);

	for (i = 0; i < dst_nr_v && indication_data_length != 0; i++) {
		const byte* tmp = v[vector_offset].data;
		vind[i].data   = &tmp[vector_position];
		vind[i].length = MIN((v[vector_offset].length - vector_position), indication_data_length);
		*pvind_nr = i + 1;

		indication_data_length -= vind[i].length;

		if (diva_streaming_read_vector_data (v, nr_v, &vector_offset, &vector_position, 0, vind[i].length) != vind[i].length) {
			DBG_ERR(("%s at %d wrong combined indication format", __FILE__, __LINE__))
			*pInd = 0;
			return (0);
		}
	}
	if (indication_data_length != 0) {
		DBG_ERR(("%s at %d wrong combined indication format", __FILE__, __LINE__))
		*pInd = 0;
		return (0);
	}

	*pInd = header[0];

	{
		const byte* tmp = v[vector_offset].data;
		if (tmp[vector_position] == 0) {
			return (0);
		}
	}

	return (vector_offset | (vector_position << 8));
}

