/*
 *
  Copyright (c) Dialogic(R), 2010-2011

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

/*! \file
 * \brief Implements to access Diva streaming
 *
 */

#include <stdio.h>
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_utils.h"

#include "platform.h"
#include "diva_streaming_result.h"
#include "diva_streaming_messages.h"
#include "diva_streaming_vector.h"
#include "diva_streaming_manager.h"
#include "chan_capi_divastreaming_utils.h"

/*
	LOCALS
	*/
static int diva_streaming_disabled;
AST_MUTEX_DEFINE_STATIC(stream_write_lock);

static diva_entity_queue_t diva_streaming_new; /* protected by stream_write_lock, new streams */

int capi_DivaStreamingSupported (unsigned controller)
{
	MESSAGE_EXCHANGE_ERROR error;
	int waitcount = 50;
	unsigned char manbuf[CAPI_MANUFACTURER_LEN];
	_cmsg CMSG;
	int ret = 0;

	if (capi20_get_manufacturer(controller, manbuf) == NULL) {
		goto done;
	}
	if ((strstr((char *)manbuf, "Eicon") == 0) &&
	    (strstr((char *)manbuf, "Dialogic") == 0)) {
		goto done;
	}

	error = capi_sendf (NULL, 0, CAPI_MANUFACTURER_REQ, controller, get_capi_MessageNumber(),
		"dw(bs)", _DI_MANU_ID, _DI_STREAM_CTRL, 2, "");

	if (error)
		goto done;

	while (waitcount) {
		error = capidev_check_wait_get_cmsg(&CMSG);

		if (IS_MANUFACTURER_CONF(&CMSG) && (CMSG.ManuID == _DI_MANU_ID) &&
			((CMSG.Class & 0xffff) == _DI_STREAM_CTRL)) {
			error = (MESSAGE_EXCHANGE_ERROR)(CMSG.Class >> 16);
			ret = (error == 0);
			break;
		}
		usleep(30000);
		waitcount--;
	}

done:
	return ret;
}

static int divaStreamingMessageRx (void* user_context, dword message, dword length, const struct _diva_streaming_vector* v, dword nr_v)
{
	diva_stream_scheduling_entry_t* pE = (diva_stream_scheduling_entry_t*)user_context;
	dword message_type = (message & 0xff);

  if (message_type == 0) { /* IDI message */
		dword offset = 0;
		diva_streaming_vector_t vind[8];
		byte Ind = 0;
		int vind_nr = 0;
		int process_indication;

		do {
			vind_nr = sizeof(vind)/sizeof(vind[0]);
			offset = diva_streaming_get_indication_data (offset, message, length, v, nr_v, &Ind, vind, &vind_nr);

			process_indication = (diva_streaming_idi_supported_ind (Ind, vind_nr != 0, vind_nr != 0 ? (byte*)vind->data : (byte*)"") != 0);

			if (likely(process_indication != 0)) {
				if (likely(Ind == 8)) {
					if (likely(pE->i != 0 && pE->i->NCCI != 0)) {
						if (pE->i->virtualBridgePeer != 0) {
							if (pE->i->bridgePeer != 0) {
								struct capi_pvt* bridgePeer = pE->i->bridgePeer;
								if (bridgePeer->NCCI != 0 && bridgePeer->diva_stream_entry != 0 &&
										bridgePeer->diva_stream_entry->diva_stream_state == DivaStreamActive &&
										bridgePeer->diva_stream_entry->diva_stream->get_tx_in_use (bridgePeer->diva_stream_entry->diva_stream) < 512 &&
										bridgePeer->diva_stream_entry->diva_stream->get_tx_free (bridgePeer->diva_stream_entry->diva_stream) >
																																																2*CAPI_MAX_B3_BLOCK_SIZE+128) {
									dword i = 0, k = 0, b3len;
									byte b3buf[CAPI_MAX_B3_BLOCK_SIZE];
									b3len = diva_streaming_read_vector_data(vind, vind_nr, &i, &k, b3buf, CAPI_MAX_B3_BLOCK_SIZE);
									bridgePeer->diva_stream_entry->diva_stream->write (bridgePeer->diva_stream_entry->diva_stream,
																																		 8U << 8 | DIVA_STREAM_MESSAGE_TX_IDI_REQUEST,
																																		 b3buf, b3len);
									bridgePeer->diva_stream_entry->diva_stream->flush_stream(bridgePeer->diva_stream_entry->diva_stream);
								} else {
									if (bridgePeer->NCCI != 0 && bridgePeer->diva_stream_entry != 0 &&
										bridgePeer->diva_stream_entry->diva_stream_state == DivaStreamActive) {
										DBG_ERR(("%s PLCI %04x discarded bridge packet free: %u in use: %u",
															pE->i->name, pE->i->PLCI & 0xffffU, 
										bridgePeer->diva_stream_entry->diva_stream->get_tx_free (bridgePeer->diva_stream_entry->diva_stream),
										bridgePeer->diva_stream_entry->diva_stream->get_tx_in_use (bridgePeer->diva_stream_entry->diva_stream)))
									}
								}
							}
						} else {
							capidev_handle_data_b3_indication_vector (pE->i, vind, vind_nr);
						}
					}
				} else {
					dword i = 0, k = 0;
					word data_length;
					byte ind_data_buffer[2048+512];
					data_length = (word)diva_streaming_read_vector_data (vind, vind_nr, &i, &k, ind_data_buffer, sizeof(ind_data_buffer));

					DBG_TRC(("Ind: %02x length:%u", Ind, data_length))
				}
			}
		} while (offset != 0);
  } else if (message_type == 0xff) { /* System message */
    switch ((byte)(message >> 8)) {
      case DIVA_STREAM_MESSAGE_INIT: /* Stream active */
				if (pE->PLCI == 0 && pE->i != 0) {
					pE->PLCI = pE->i->PLCI;
				}
				cc_verbose(3, 0, VERBOSE_PREFIX_2 "%s: stream active (PLCI=%#x)\n", pE->vname, pE->PLCI);
				if (pE->diva_stream_state == DivaStreamCreated) {
					pE->diva_stream_state = DivaStreamActive;
				} else if (pE->diva_stream_state == DivaStreamCancelSent) {
					pE->diva_stream->release_stream(pE->diva_stream);
					pE->i = 0;
					pE->diva_stream_state = DivaStreamDisconnectSent;
				}
        break; 

			case DIVA_STREAM_MESSAGE_RX_TX_ACK: /* Resolved Tx flow control */
				/* cc_verbose(4, 1, "%s: stream tx ack (PLCI=%#x)\n", pE->vname, pE->PLCI); */
				break;

      case DIVA_STREAM_MESSAGE_SYNC_ACK: /* Sync ack request acknowledge */
				cc_verbose(4, 1, "%s: stream sync ack (PLCI=%#x, sequence=%08x)\n", pE->vname, pE->PLCI, length);
        break; 

      case DIVA_STREAM_MESSAGE_RELEASE_ACK: /* Release stream acknowledge */
				pE->diva_stream_state = DivaStreamDisconnected;
				pE->diva_stream = 0;
				cc_verbose(3, 0, VERBOSE_PREFIX_2 "%s: stream released (PLCI=%#x)\n", pE->vname, pE->PLCI);
				break; 

      case DIVA_STREAM_MESSAGE_INIT_ERROR: /* Failed to initialize stream */
				pE->diva_stream_state = DivaStreamDisconnected;
				pE->diva_stream = 0;
				cc_verbose(3, 0, VERBOSE_PREFIX_2 "%s: stream init error (PLCI=%#x, error=%d)\n", pE->vname, pE->PLCI, length);
        break; 

      default: 
				cc_verbose(3, 0, VERBOSE_PREFIX_2 "%s: stream unknown system message (PLCI=%#x, message=%08x)\n", pE->vname, pE->PLCI, message);
        break; 
    }
  } else {
		cc_verbose(3, 0, VERBOSE_PREFIX_2 "%s: unknown stream message (PLCI=%#x, message=%08x)\n", pE->vname, pE->PLCI, message);
  }

  return (0);
}

/*
 * Create Diva stream
 *
 */
void capi_DivaStreamingOn(struct capi_pvt *i, byte streamCommand, _cword messageNumber)
{
	diva_stream_scheduling_entry_t* pE;
	int ret;
	char trace_ident[8];
	unsigned int effectivePLCI;

	if (diva_streaming_disabled)
		return;

	pE = ast_malloc (sizeof(*pE));
	if (pE == 0)
		return;

	snprintf (trace_ident, sizeof(trace_ident), "C%02x", (byte)i->PLCI);
	trace_ident[sizeof(trace_ident)-1] = 0;

	cc_mutex_lock(&stream_write_lock);

	ret = diva_stream_create (&pE->diva_stream, NULL, 255, divaStreamingMessageRx, pE, trace_ident);

	if (ret == 0) {
		static byte addie[] = { 0x2d /* UID */, 0x01, 0x00, 0x04 /* BC */, 0x04, 0x0, 0x0, 0x0, 0x00 /* 0x20 DMA polling */, 0 /* END */};
		byte* description = (byte*)pE->diva_stream->description (pE->diva_stream, addie, (byte)sizeof(addie));
		MESSAGE_EXCHANGE_ERROR error;

		description[1] = streamCommand;

		description[3] |= 0x01;

		if (streamCommand == 0) {
			messageNumber = get_capi_MessageNumber();
			effectivePLCI = i->PLCI;
		} else {
			/*
				PLCI still not assigned. Send to controller and tag with message number
				where command receives effective
				*/
			effectivePLCI = i->controller;
		}

		error = capi_sendf (NULL, 0, CAPI_MANUFACTURER_REQ, effectivePLCI, messageNumber,
												"dws", _DI_MANU_ID, _DI_STREAM_CTRL, description);
		if (error == 0) {
			pE->diva_stream_state = DivaStreamCreated;
			pE->PLCI              = i->PLCI;
			pE->i                 = i;
			i->diva_stream_entry  = pE;
			memcpy (pE->vname, i->vname, MIN(sizeof(pE->vname), sizeof(i->vname)));
			pE->vname[sizeof(pE->vname)-1] = 0;
			pE->rx_flow_control = 0;
			pE->tx_flow_control = 0;
			diva_q_add_tail (&diva_streaming_new, &pE->link);
		} else {
			pE->diva_stream->release (pE->diva_stream);
			ast_free (pE);
		}
	}

	cc_mutex_unlock(&stream_write_lock);
}

/*
 * Remove stream info
 * 
 * To remove stream from one active connection:
 *   remove stream info
 *   disconnect B3
 *   remove stream
 *   select_b
 */
void capi_DivaStreamingRemoveInfo(struct capi_pvt *i)
{
	byte description[] = { 2, 0, 0 };
	MESSAGE_EXCHANGE_ERROR error;
	int send;

	cc_mutex_lock(&stream_write_lock);
	send = i->diva_stream_entry != 0;
	cc_mutex_unlock(&stream_write_lock);

	if (send != 0)
		error = capi_sendf (NULL, 0, CAPI_MANUFACTURER_REQ, i->PLCI, get_capi_MessageNumber(),
											"dws", _DI_MANU_ID, _DI_STREAM_CTRL, description);
}

/*!
 * \brief Send empty stream to inform no Diva streaming is used for this PLCI
 */
void capi_DivaStreamingStreamNotUsed(struct capi_pvt *i, byte streamCommand, _cword messageNumber)
{
	byte description[] = { 0x04, 0x00, 0x02, 0x00, 0x00 };
	unsigned int effectivePLCI;
	MESSAGE_EXCHANGE_ERROR error;

	description[1] = streamCommand;

	if (streamCommand == 0) {
		messageNumber = get_capi_MessageNumber();
		effectivePLCI = i->PLCI;
	} else {
		/*
			PLCI still not assigned. Send to controller and tag with message number
			where command receives effective
			*/
		effectivePLCI = i->controller;
	}

	error = capi_sendf (NULL, 0, CAPI_MANUFACTURER_REQ, effectivePLCI, messageNumber,
											"dws", _DI_MANU_ID, _DI_STREAM_CTRL, description);
}

void capi_DivaStreamingRemove(struct capi_pvt *i)
{
	diva_stream_scheduling_entry_t* pE = i->diva_stream_entry;
	int send_cancel = 0;

	cc_mutex_lock(&stream_write_lock);
	pE = i->diva_stream_entry;
	if (pE != 0) {
		i->diva_stream_entry = 0;
		pE->i = 0;
		if (pE->diva_stream_state == DivaStreamCreated) {

			if (i->NCCI != 0) {
				/*
					If NCCI is not sen then this is no possibility to send cancel request
					to queued in the IDI L2 streaming info. But in user mode this is OK,
					if removing PLCI CAPI removes networking entity and this operation
          causes cancellation of create streaming request.
          Timeout is only for the rare case where create streaming request was newer
          sent to hardware.
					*/
				send_cancel = 1;
			}
			pE->diva_stream_state = DivaStreamCancelSent;
			pE->cancel_start = time(NULL) + 5;
			DBG_LOG(("stream cancelled [%p]", pE->diva_stream))
		} else if (pE->diva_stream_state == DivaStreamActive) {
			pE->diva_stream->release_stream(pE->diva_stream);
			pE->diva_stream_state = DivaStreamDisconnectSent;
		}
	}
	cc_mutex_unlock(&stream_write_lock);

	if (send_cancel != 0) {
		static byte data[] = { 0x8 /* CONTROL */, 0x01 /* CANCEL */};
		MESSAGE_EXCHANGE_ERROR error;

		error = capi_sendf(NULL, 0, CAPI_DATA_B3_REQ, i->NCCI, get_capi_MessageNumber(),
											"dwww", data, sizeof(data), 0, 1U << 4);
		if (likely(error == 0)) {
			cc_mutex_lock(&i->lock);
			i->B3count++;
			cc_mutex_unlock(&i->lock);
		}
	}
}

/*
	This is only one used to access streaming scheduling list routine.
	This ensures list can be accessed w/o anly locks

	To ensure exclusive access scheduling queue is static function variable
	*/
void divaStreamingWakeup (void)
{
	static diva_entity_queue_t active_streams;
	diva_entity_link_t* link;
	time_t current_time = time (NULL);

	cc_mutex_lock(&stream_write_lock);
	while ((link = diva_q_get_head (&diva_streaming_new)) != 0) {
		diva_stream_scheduling_entry_t* pE = DIVAS_CONTAINING_RECORD(link, diva_stream_scheduling_entry_t, link);
		diva_q_remove (&diva_streaming_new, &pE->link);
		diva_q_add_tail (&active_streams, &pE->link);
	}
	cc_mutex_unlock(&stream_write_lock);

	for (link = diva_q_get_head (&active_streams); likely(link != 0);) {
		diva_entity_link_t* next = diva_q_get_next(link);
		diva_stream_scheduling_entry_t* pE = DIVAS_CONTAINING_RECORD(link, diva_stream_scheduling_entry_t, link);

		cc_mutex_lock(&stream_write_lock);
		pE->diva_stream->wakeup (pE->diva_stream);
		if (unlikely(pE->diva_stream_state == DivaStreamCancelSent && pE->cancel_start < current_time)) {
			DBG_LOG(("stream reclaimed [%p]", pE->diva_stream))
			pE->diva_stream->release (pE->diva_stream);
			pE->diva_stream_state = DivaStreamDisconnected;
			pE->diva_stream = 0;
		}

		if (unlikely(pE->diva_stream == 0)) {
			diva_q_remove (&active_streams, &pE->link);
			if (pE->i != 0) {
				pE->i->diva_stream_entry = 0;
			}
			ast_free (pE);
		}
		cc_mutex_unlock(&stream_write_lock);

		link = next;
	}
}

unsigned int capi_DivaStreamingGetStreamInUse(const struct capi_pvt* i)
{
	unsigned int ret = 0;

	capi_DivaStreamLock();
	if ((i != NULL) && (i->diva_stream_entry != NULL) &&
			(i->diva_stream_entry->diva_stream_state == DivaStreamActive) &&
			(i->diva_stream_entry->diva_stream != NULL)) {
		ret = i->diva_stream_entry->diva_stream->get_tx_in_use (i->diva_stream_entry->diva_stream);
	}
	capi_DivaStreamUnLock();

	return (ret);
}

void capi_DivaStreamLock(void)
{
	cc_mutex_lock(&stream_write_lock);
}

void capi_DivaStreamUnLock(void)
{
	cc_mutex_unlock(&stream_write_lock);
}

void capi_DivaStreamingDisable (void) {
	diva_streaming_disabled = 1;
}

