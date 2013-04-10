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
#ifndef __DIVA_STREAMING_MESSAGES_H__
#define __DIVA_STREAMING_MESSAGES_H__

/*
 * Tx direction (to IDI)
 */
#define DIVA_STREAMING_IDI_SYSTEM_MESSAGE 0x80000000U
#define DIVA_STREAMING_IDI_RX_ACK_MSG 0x02 /* Ack processed by host data */
#define DIVA_STREAMING_IDI_SYNC_REQ   0x03 /* Sync request */
#define DIVA_STREAMING_IDI_RELEASE    0x04 /* Release stream */
#define DIVA_STREAMING_IDI_SET_DEBUG_IDENT 0x05 /* Set debug ident */
#define DIVA_STREAMING_IDI_TX_REQUEST 0x07


/*
 * Rx direction (to host)
 */
#define DIVA_STREAMING_IDI_TX_INIT_MSG 0x01 /* Init message, created version, counter address */
#define DIVA_STREAMING_IDI_TX_INIT_MSG_LENGTH 15
#define DIVA_STREAMING_IDI_TX_ACK_MSG  0x02 /* Ack processed by tx data */
#define DIVA_STREAMING_IDI_TX_ACK_MSG_LENGTH  0x09
#define DIVA_STREAMING_IDI_TX_SYNC_ACK 0x03 /* Sync request acknowledge */
#define DIVA_STREAMING_IDI_TX_SYNC_ACK_LENGTH 0x0a
#define DIVA_STREAMING_IDI_RELEASE_ACK 0x04
#define DIVA_STREAMING_IDI_RELEASE_ACK_LENGTH 0x08
#define DIVA_STREAMING_IDI_TX_INIT_ERROR 0x05
#define DIVA_STREAMING_IDI_TX_INIT_ERROR_LENGTH 0x0a


#ifndef UDATA_REQUEST_SEND_DTMF_DIGITS
#define UDATA_REQUEST_SEND_DTMF_DIGITS      16
#endif
#ifndef UDATA_INDICATION_DTMF_DIGITS_SENT
#define UDATA_INDICATION_DTMF_DIGITS_SENT   16
#endif
#ifndef UDATA_INDICATION_DTMF_DIGITS_RECEIVED
#define UDATA_INDICATION_DTMF_DIGITS_RECEIVED 17
#endif
 
#ifndef UDATA_REQUEST_MIXER_TAP_DATA
#define UDATA_REQUEST_MIXER_TAP_DATA        27
#endif
#ifndef UDATA_INDICATION_MIXER_TAP_DATA
#define UDATA_INDICATION_MIXER_TAP_DATA     27
#endif

#ifndef UDATA_REQUEST_RTCP_PACKET
#define UDATA_REQUEST_RTCP_PACKET           67
#endif
#ifndef UDATA_INDICATION_RTCP_PACKET
#define UDATA_INDICATION_RTCP_PACKET        67
#endif
 
#ifndef UDATA_REQUEST_RTCP_GENERATE
#define UDATA_REQUEST_RTCP_GENERATE         68
#endif
#ifndef UDATA_INDICATION_RTCP_GENERATED
#define UDATA_INDICATION_RTCP_GENERATED     68
#endif
#ifndef UDATA_REQUEST_RTCP_ACCEPT
#define UDATA_REQUEST_RTCP_ACCEPT           69
#endif
#ifndef UDATA_INDICATION_RTCP_ACCEPTED
#define UDATA_INDICATION_RTCP_ACCEPTED      69
#endif

#ifndef UDATA_REQUEST_FEC_PACKET
#define UDATA_REQUEST_FEC_PACKET            70
#endif
#ifndef UDATA_INDICATION_FEC_PACKET
#define UDATA_INDICATION_FEC_PACKET         70
#endif

int diva_streaming_idi_supported_ind (byte idiInd, dword length, const byte* data);
int diva_streaming_idi_supported_req (byte idiReq, dword length, const byte* data);

#endif
