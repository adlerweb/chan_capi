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

/*
	LOCALS
	*/
static int pbx_capi_get_all_locks (struct capi_pvt *i, struct ast_channel** usedChannel);

/*!
		\brief Execute any capicommand

		\note Called from CLI or from AMI context
	*/
int pbx_capi_management_capicommand(const char *requiredChannelName, const char *chancapiCommand) {
	int ifc_type, retry_search, search_loops;
	struct capi_pvt *i;
	struct {
		struct capi_pvt *head;
		void (*lock_proc)(void);
		void (*unlock_proc)(void);
	} data[2];

	data[0].head        = capi_iflist;
	data[0].lock_proc   = pbx_capi_lock_interfaces;
	data[0].unlock_proc = pbx_capi_unlock_interfaces;
	data[1].head        = (struct capi_pvt*)pbx_capi_get_nulliflist();
	data[1].lock_proc   = pbx_capi_nulliflist_lock;
	data[1].unlock_proc = pbx_capi_nulliflist_unlock;

	if (ast_strlen_zero(requiredChannelName)) {
		return -2;
	}

	if (ast_strlen_zero(chancapiCommand)) {
		return -3;
	}

	if (strcmp(requiredChannelName, "none") == 0) {
		int ret = (pbx_capi_cli_exec_capicommand(NULL, chancapiCommand) == 0) ? 0 : -1;
		return (ret);
	}

	for (ifc_type = 0; ifc_type < sizeof(data)/sizeof(data[0]); ifc_type++) {
		search_loops = 10;
		do {
			data[ifc_type].lock_proc();
			for (i = data[ifc_type].head, retry_search = 0; i != 0; i = i->next) {
				struct ast_channel* c = NULL;

				if ((i->used == 0) || ((i->channeltype != CAPI_CHANNELTYPE_B) &&
					(i->channeltype != CAPI_CHANNELTYPE_NULL)))
					continue;
				if (i->data_plci != 0)
					continue;

				if (pbx_capi_get_all_locks (i, &c) != 0) {
					retry_search = 1;
					break;
				}
				if ((!ast_strlen_zero(c->name) && (strcmp(requiredChannelName, c->name) == 0)) ||
						strcmp(requiredChannelName, i->vname) == 0) {
					struct ast_channel* usedChannel = c;
					int ret;

					data[ifc_type].unlock_proc();

					if (i->channeltype != CAPI_CHANNELTYPE_NULL) {
						ret = (pbx_capi_cli_exec_capicommand(usedChannel, chancapiCommand) == 0) ? 0 : -1;
						cc_mutex_unlock(&i->lock);
						ast_channel_unlock(c);
					} else {
						ast_channel_unlock(c);
						ret = (pbx_capi_cli_exec_capicommand(usedChannel, chancapiCommand) == 0) ? 0 : -1;
						cc_mutex_unlock(&i->lock);
					}

					return ret;
				}
				cc_mutex_unlock(&i->lock);
				ast_channel_unlock(c);
			}
			data[ifc_type].unlock_proc();
			if (retry_search != 0) {
				usleep (100);
			}
		} while((retry_search != 0) && (search_loops-- > 0));
	}

	return -4;
}

/*!
 *  \brief Try to take all locks. Called with false lock order
 *  one of the list locks (iflock or nullif_lock) taken
 *  Used by CLI/AMI
 */
static int pbx_capi_get_all_locks (struct capi_pvt *i, struct ast_channel** usedChannel)
{
	struct ast_channel* c = (i->channeltype != CAPI_CHANNELTYPE_NULL) ? i->owner : i->used;
	if (c != 0) {
		if (ast_channel_trylock(c) == 0) {
			if (ast_mutex_trylock(&i->lock) == 0) {
				struct ast_channel* cref = (i->channeltype != CAPI_CHANNELTYPE_NULL) ? i->owner : i->used;
				if (cref == c) {
					*usedChannel = c;
					return (0);
				} else {
					ast_mutex_unlock(&i->lock);
					ast_channel_unlock (c);
				}
			} else {
				ast_channel_unlock (c);
			}
		}
	}

	return (-1);
}
