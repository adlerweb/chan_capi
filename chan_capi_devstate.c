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
#include <stdio.h>
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_qsig.h"
#include "chan_capi_utils.h"
#include "chan_capi_chat.h"
#include "chan_capi_devstate.h"

/*
	LOCALS
	*/
static
#ifdef CC_AST_HAS_VERSION_1_6
enum ast_device_state
#else
int
#endif
pbx_capi_chat_room_state(const char *data);

static int capiChatProviderRegistered;

void pbx_capi_register_device_state_providers(void)
{
	int i, capi_num_controllers;

	capiChatProviderRegistered = (ast_devstate_prov_add("Capichat", pbx_capi_chat_room_state) == 0);

	/*
		Set initial device state for all supported interface
		*/
	for (i = 1, capi_num_controllers = pbx_capi_get_num_controllers();
			 i <= capi_num_controllers;
			 i++) {
		const struct cc_capi_controller *capiController = pbx_capi_get_controller(i);

		if (capiController != NULL) {
			pbx_capi_ifc_state_event(capiController, 0);
		}
	}
}

void pbx_capi_unregister_device_state_providers(void)
{
	if (capiChatProviderRegistered != 0) {
		ast_devstate_prov_del("Capichat");
	}
}

/*!
 * \brief Read conference room state
 */
static
#ifdef CC_AST_HAS_VERSION_1_6
enum ast_device_state
#else
int
#endif
pbx_capi_chat_room_state(const char *data)
{
	const struct capichat_s *room;
#ifdef CC_AST_HAS_VERSION_1_6
	enum ast_device_state ret = AST_DEVICE_NOT_INUSE;
#else
	int ret = AST_DEVICE_NOT_INUSE;
#endif

	if (data == 0)
		return AST_DEVICE_INVALID;

	pbx_capi_lock_chat_rooms();
	for (room = pbx_capi_chat_get_room_c(NULL);
			 room != 0;
			 room = pbx_capi_chat_get_room_c(room)) {
		if (strcmp(data, pbx_capi_chat_get_room_name(room)) == 0) {
			ret = AST_DEVICE_INUSE;
			break;
		}
	}
	pbx_capi_unlock_chat_rooms();

	return ret;
}

/*!
 * \brief Conference room state change
 */
void pbx_capi_chat_room_state_event(const char* roomName, int inUse)
{
	if (capiChatProviderRegistered != 0) {
#ifdef CC_AST_HAS_VERSION_1_6
		ast_devstate_changed((inUse != 0) ? AST_DEVICE_INUSE : AST_DEVICE_NOT_INUSE,
#ifdef CC_AST_HAS_AST_DEVSTATE_CACHE
			AST_DEVSTATE_CACHABLE,
#endif
			"capichat:%s", roomName);
#else
		ast_device_state_changed("capichat:%s", roomName);
#endif
	}
}

void pbx_capi_ifc_state_event(const struct cc_capi_controller* capiController, int channelsChanged)
{
	if ((channelsChanged == 0) ||
			(capiController->nbchannels == capiController->nfreebchannels) ||
			(capiController->nfreebchannels == 0) ||
			((capiController->nfreebchannels < capiController->nfreebchannelsHardThr) &&
				(capiController->nfreebchannels - channelsChanged >= capiController->nfreebchannelsHardThr)) ||
			((capiController->nfreebchannels >= capiController->nfreebchannelsHardThr) &&
				(capiController->nfreebchannels - channelsChanged < capiController->nfreebchannelsHardThr))) {
#ifdef CC_AST_HAS_VERSION_1_6
		ast_devstate_changed(AST_DEVICE_UNKNOWN,
#ifdef CC_AST_HAS_AST_DEVSTATE_CACHE
			AST_DEVSTATE_CACHABLE,
#endif
			CC_MESSAGE_BIGNAME"/I%d/congestion", capiController->controller);
#else
		ast_device_state_changed (CC_MESSAGE_BIGNAME"/I%d/congestion", capiController->controller);
#endif
	}
}

