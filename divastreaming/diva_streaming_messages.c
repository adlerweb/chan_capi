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
#include "diva_streaming_messages.h"

int diva_streaming_idi_supported_ind (byte idiInd, dword length, const byte* data) {
	byte Ind = idiInd & 0x0f;

	if (likely(Ind == N_DATA))
		return (1);

	if (Ind == N_UDATA && length != 0) {
		switch (*data) {
			case UDATA_INDICATION_DTMF_DIGITS_SENT:
			case UDATA_INDICATION_DTMF_DIGITS_RECEIVED:
			case UDATA_INDICATION_MIXER_TAP_DATA:
			case UDATA_INDICATION_RTCP_PACKET:
			case UDATA_INDICATION_RTCP_GENERATED:
			case UDATA_INDICATION_RTCP_ACCEPTED:
			case UDATA_INDICATION_FEC_PACKET:
				return (1);
		}
	}

	return (0);
}

int diva_streaming_idi_supported_req (byte idiReq, dword length, const byte* data) {
	byte Req = idiReq & 0x0f;

	if (likely(Req == N_DATA))
		return (1);

	if (Req == N_UDATA && length != 0) {
		switch (*data) {
			case UDATA_REQUEST_SEND_DTMF_DIGITS:
			case UDATA_REQUEST_MIXER_TAP_DATA:
			case UDATA_REQUEST_RTCP_PACKET:
			case UDATA_REQUEST_RTCP_GENERATE:
			case UDATA_REQUEST_RTCP_ACCEPT:
			case UDATA_REQUEST_FEC_PACKET:
				return (1);
		}
	}

	return (0);
}

