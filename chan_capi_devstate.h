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
#ifndef __CC_DEVICE_STATE_PROVIDER_INTERFACE_H__
#define __CC_DEVICE_STATE_PROVIDER_INTERFACE_H__

void pbx_capi_register_device_state_providers(void);
void pbx_capi_unregister_device_state_providers(void);
void pbx_capi_chat_room_state_event(const char* roomName, int inUse);
void pbx_capi_ifc_state_event(const struct cc_capi_controller* capiController, int channelsChanged);


#endif
