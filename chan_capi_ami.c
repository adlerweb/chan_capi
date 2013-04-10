/*
 *
  Copyright (c) Dialogic (R) 2009 - 2010
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
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
 *
 * Based on apps/app_meetme.c
 *
 */
#include <stdio.h>
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_qsig.h"
#include "chan_capi_utils.h"
#include "chan_capi_chat.h"
#include "chan_capi_management_common.h"
#include "asterisk/manager.h"

#ifdef CC_AST_HAS_VERSION_1_6

#define CC_AMI_ACTION_NAME_CHATLIST    "CapichatList"
#define CC_AMI_ACTION_NAME_CHATMUTE    "CapichatMute"
#define CC_AMI_ACTION_NAME_CHATUNMUTE  "CapichatUnmute"
#define CC_AMI_ACTION_NAME_CHATREMOVE  "CapichatRemove"
#define CC_AMI_ACTION_NAME_CAPICOMMAND "CapiCommand"

/*
	LOCALS
	*/
static int pbx_capi_ami_capichat_list(struct mansession *s, const struct message *m);
static int pbx_capi_ami_capichat_mute(struct mansession *s, const struct message *m);
static int pbx_capi_ami_capichat_unmute(struct mansession *s, const struct message *m);
static int pbx_capi_ami_capichat_remove(struct mansession *s, const struct message *m);
static int pbx_capi_ami_capichat_control(struct mansession *s, const struct message *m, int chatMute);
static int pbx_capi_ami_capicommand(struct mansession *s, const struct message *m);
static int capiChatListRegistered;
static int capiChatMuteRegistered;
static int capiChatUnmuteRegistered;
static int capiChatRemoveRegistered;
static int capiCommandRegistered;

static char mandescr_capichatlist[] =
"Description: Lists all users in a particular CapiChat conference.\n"
"CapichatList will follow as separate events, followed by a final event called\n"
"CapichatListComplete.\n"
"Variables:\n"
"    *ActionId: <id>\n"
"    *Conference: <confname>\n";

static char mandescr_capichatmute[] =
"Description: Mutes user in a particular CapiChat conference.\n"
"Variables:\n"
"    *ActionId: <id>\n"
"    *Conference: <confname>\n"
"    *Member: <membername>\n"
"    *Path: <Rx or Tx>\n";

static char mandescr_capichatunmute[] =
"Description: Unmutes user in a particular CapiChat conference.\n"
"Variables:\n"
"    *ActionId: <id>\n"
"    *Conference: <confname>\n"
"    *Member: <membername>\n"
"    *Path: <Rx or Tx>\n";

static char mandescr_capichatremove[] =
"Description: Removes user in a particular CapiChat conference.\n"
"Variables:\n"
"    *ActionId: <id>\n"
"    *Conference: <confname>\n"
"    *Member: <membername>\n";

static char mandescr_capicommand[] =
"Description: Exec capicommand.\n"
"Variables:\n"
"    *ActionId: <id>\n"
"    *Channel: <channame>\n"
"    *Capicommand: <capicommand>\n";

void pbx_capi_ami_register(void)
{
	capiChatListRegistered = ast_manager_register2(CC_AMI_ACTION_NAME_CHATLIST,
																								EVENT_FLAG_REPORTING,
																								pbx_capi_ami_capichat_list,
																								"List participants in a conference",
																								mandescr_capichatlist) == 0;

	capiChatMuteRegistered = ast_manager_register2(CC_AMI_ACTION_NAME_CHATMUTE,
																								EVENT_FLAG_CALL,
																								pbx_capi_ami_capichat_mute,
																								"Mute a conference user",
																								mandescr_capichatmute) == 0;

	capiChatUnmuteRegistered = ast_manager_register2(CC_AMI_ACTION_NAME_CHATUNMUTE,
																								EVENT_FLAG_CALL,
																								pbx_capi_ami_capichat_unmute,
																								"Unmute a conference user",
																								mandescr_capichatunmute) == 0;

	capiChatRemoveRegistered = ast_manager_register2(CC_AMI_ACTION_NAME_CHATREMOVE,
																								EVENT_FLAG_CALL,
																								pbx_capi_ami_capichat_remove,
																								"Remove a conference user",
																								mandescr_capichatremove) == 0;

	capiCommandRegistered = ast_manager_register2(CC_AMI_ACTION_NAME_CAPICOMMAND,
																								EVENT_FLAG_CALL,
																								pbx_capi_ami_capicommand,
																								"Exec capicommand",
																								mandescr_capicommand) == 0;
}

void pbx_capi_ami_unregister(void)
{
	if (capiChatListRegistered != 0)
		ast_manager_unregister(CC_AMI_ACTION_NAME_CHATLIST);

	if (capiChatMuteRegistered != 0)
		ast_manager_unregister(CC_AMI_ACTION_NAME_CHATMUTE);

	if (capiChatUnmuteRegistered != 0)
		ast_manager_unregister(CC_AMI_ACTION_NAME_CHATUNMUTE);

	if (capiChatRemoveRegistered != 0)
		ast_manager_unregister(CC_AMI_ACTION_NAME_CHATREMOVE);

	if (capiCommandRegistered != 0)
		ast_manager_unregister(CC_AMI_ACTION_NAME_CAPICOMMAND);
}

static int pbx_capi_ami_capichat_list(struct mansession *s, const struct message *m) {
	const char *actionid = astman_get_header(m, "ActionID");
	const char *conference = astman_get_header(m, "Conference");
	char idText[80] = "";
	int total = 0;
	const struct capichat_s *capiChatRoom;

	if (!ast_strlen_zero(actionid))
		snprintf(idText, sizeof(idText), "ActionID: %s\r\n", actionid);

	if (pbx_capi_chat_get_room_c(NULL) == NULL) {
		astman_send_error(s, m, "No active conferences.");
		return 0;
	}

	astman_send_listack(s, m, CC_AMI_ACTION_NAME_CHATLIST" user list will follow", "start");

	/* Find the right conference */
	pbx_capi_lock_chat_rooms();

	for (capiChatRoom = pbx_capi_chat_get_room_c(NULL), total = 0;
			 capiChatRoom != NULL;
			 capiChatRoom = pbx_capi_chat_get_room_c(capiChatRoom)) {
		const char*  roomName = pbx_capi_chat_get_room_name(capiChatRoom);
		/* If we ask for one particular, and this isn't it, skip it */
		if (!ast_strlen_zero(conference) && strcmp(roomName, conference))
			continue;

		{
			unsigned int roomNumber      = pbx_capi_chat_get_room_number(capiChatRoom);
			struct ast_channel *c        = pbx_capi_chat_get_room_channel(capiChatRoom);
			const struct capi_pvt* i     = pbx_capi_chat_get_room_interface_c(capiChatRoom);
			int isMemberOperator         = pbx_capi_chat_is_member_operator(capiChatRoom);
			int isCapiChatRoomMuted      = pbx_capi_chat_is_room_muted(capiChatRoom);
			int isCapiChatMemberMuted    = pbx_capi_chat_is_member_muted(capiChatRoom);
			int isCapiChatMemberListener = pbx_capi_chat_is_member_listener(capiChatRoom);
			int isCapiChatMostRecentMember = pbx_capi_chat_is_most_recent_user(capiChatRoom);
			const char* mutedVisualName = "No";
			char* cidVisual;
			char* callerNameVisual;

			if ((c == NULL) || (i == NULL))
				continue;

			cidVisual        = ast_strdup(pbx_capi_get_cid (c, "<unknown>"));
			callerNameVisual = ast_strdup(pbx_capi_get_callername (c, "<no name>"));

			if (isCapiChatMemberListener || isCapiChatRoomMuted || isCapiChatMemberMuted) {
				if (isMemberOperator) {
					if (isCapiChatMemberMuted)
						mutedVisualName = "By self";
				} else if (isCapiChatMemberListener || isCapiChatRoomMuted) {
					mutedVisualName = "By admin";
				} else {
					mutedVisualName = "By self";
				}
			}

			total++;
			astman_append(s,
				"Event: "CC_AMI_ACTION_NAME_CHATLIST"\r\n"
				"%s"
				"Conference: %s/%u\r\n"
				"UserNumber: %d\r\n"
				"CallerIDNum: %s\r\n"
				"CallerIDName: %s\r\n"
				"Channel: %s\r\n"
				"Admin: %s\r\n"
				"Role: %s\r\n"
				"MarkedUser: %s\r\n"
				"Muted: %s\r\n"
				"Talking: %s\r\n"
				"Domain: %s\r\n"
				"DTMF: %s\r\n"
				"EchoCancel: %s\r\n"
				"NoiseSupp: %s\r\n"
				"RxAGC: %s\r\n"
				"TxAGC: %s\r\n"
				"RxGain: %.1f%s\r\n"
				"TxGain: %.1f%s\r\n"
				"\r\n",
				idText,
				roomName,
				roomNumber,
				total,
				(cidVisual != 0) ? cidVisual : "?",
				(callerNameVisual != 0) ? callerNameVisual : "?",
				c->name,
				(isMemberOperator != 0) ? "Yes" : "No",
				(isCapiChatMemberListener != 0) ? "Listen only" : "Talk and listen" /* "Talk only" */,
				(isCapiChatMostRecentMember != 0) ? "Yes" : "No",
				mutedVisualName,
				/* "Yes" "No" */ "Not monitored",
				(i->channeltype == CAPI_CHANNELTYPE_B) ? "TDM" : "IP",
				(i->isdnstate & CAPI_ISDN_STATE_DTMF) ? "Y" : "N",
				(i->isdnstate & CAPI_ISDN_STATE_EC)   ? "Y" : "N",
				(i->divaAudioFlags & 0x0080) ? "Y" : "N", /* Noise supression */
				(i->divaAudioFlags & 0x0008) ? "Y" : "N", /* Rx AGC */
				(i->divaAudioFlags & 0x0004) ? "Y" : "N", /* Tx AGC */
				i->divaDigitalRxGainDB, "dB",
				i->divaDigitalTxGainDB, "dB");

				ast_free (cidVisual);
				ast_free (callerNameVisual);
		}
	}
	pbx_capi_unlock_chat_rooms();
	/* Send final confirmation */
	astman_append(s,
	"Event: "CC_AMI_ACTION_NAME_CHATLIST"Complete\r\n"
	"EventList: Complete\r\n"
	"ListItems: %d\r\n"
	"%s"
	"\r\n", total, idText);
	return 0;
}

static int pbx_capi_ami_capichat_mute(struct mansession *s, const struct message *m)
{
	return pbx_capi_ami_capichat_control(s, m, 1);
}

static int pbx_capi_ami_capichat_unmute(struct mansession *s, const struct message *m)
{
	return pbx_capi_ami_capichat_control(s, m, 0);
}

static int pbx_capi_ami_capichat_control(struct mansession *s, const struct message *m, int chatMute)
{
	const char *roomName  = astman_get_header(m, "Conference");
	const char *userName  = astman_get_header(m, "Member");
	const char *voicePath = astman_get_header(m, "Path");
	const char* capiCommand;
	int ret;

	if (ast_strlen_zero(roomName)) {
		astman_send_error(s, m, "Capi Chat conference not specified");
		return 0;
	}

	if (ast_strlen_zero(userName)) {
		char* param = ast_strdupa((chatMute != 0) ? "yes" : "no");
		int ret = pbx_capi_chat_mute(NULL, param);
		if (ret == 0) {
			astman_send_ack(s, m, (chatMute != 0) ? "Conference muted" : "Conference unmuted");
		} else {
			astman_send_error(s, m, "Failed to change mode of Capi Chat conference");
		}
		return 0;
	}

	if ((voicePath != NULL) && (strcmp(voicePath, "Rx") == 0)) {
		capiCommand = (chatMute != 0) ? "rxdgain,-128" : "rxdgain,0";
	} else {
		capiCommand = (chatMute != 0) ? "txdgain,-128" : "txdgain,0";
	}

	ret = pbx_capi_management_capicommand(userName, capiCommand);

	switch (ret) {
		case 0:
			astman_send_ack(s, m, (chatMute != 0) ? "User muted" : "User unmuted");
			break;

		case -4:
			astman_send_error(s, m, "User not found");
			break;

		default:
			astman_send_error(s, m, "Command error");
			break;
	}

	return 0;
}

static int pbx_capi_ami_capichat_remove(struct mansession *s, const struct message *m)
{
	const char *roomName  = astman_get_header(m, "Conference");
	const char *userName  = astman_get_header(m, "Member");
	int ret;

	if (ast_strlen_zero(roomName)) {
		astman_send_error(s, m, "Capi Chat conference not specified");
		return 0;
	}
	if (ast_strlen_zero(userName)) {
		astman_send_error(s, m, "Capi Chat member not specified");
		return 0;
	}

	ret = pbx_capi_chat_remove_user (roomName, userName);
	if (ret == 0) {
		astman_send_ack(s, m, "Member removed");
	} else {
		astman_send_error(s, m, "Member not found");
	}

	return 0;
}

static int pbx_capi_ami_capicommand(struct mansession *s, const struct message *m)
{
	const char *requiredChannelName  = astman_get_header(m, "Channel");
	const char *chancapiCommand      = astman_get_header(m, "Command");
	int ret = pbx_capi_management_capicommand(requiredChannelName, chancapiCommand);

	switch (ret) {
		case 0:
			astman_send_ack(s, m, "OK");
			break;

		case -2:
			astman_send_error(s, m, "Channel name not specified");
			break;

		case -3:
			astman_send_error(s, m, "Capi command name not specified");
			break;

		case -4:
			astman_send_error(s, m, "Channel not found");
			break;

		case -1:
		default:
			astman_send_error(s, m, "Command error");
			break;
	}

	return 0;
}

#else
void pbx_capi_ami_register(void)
{
}
void pbx_capi_ami_unregister(void)
{
}
#endif

void pbx_capi_chat_join_event(struct ast_channel* c, const struct capichat_s * room)
{
#ifdef CC_AST_HAS_VERSION_1_8
	ast_manager_event(c,
#else
	manager_event(
#endif
		EVENT_FLAG_CALL, "CapichatJoin",
		"Channel: %s\r\n"
		"Uniqueid: %s\r\n"
		"Conference: %s\r\n"
		"Conferencenum: %u\r\n"
		"CallerIDnum: %s\r\n"
		"CallerIDname: %s\r\n",
		c->name, c->uniqueid,
		pbx_capi_chat_get_room_name(room),
		pbx_capi_chat_get_room_number(room),
		pbx_capi_get_cid (c, "<unknown>"),
		pbx_capi_get_callername (c, "<no name>"));
}

void pbx_capi_chat_leave_event(struct ast_channel* c,
															 const struct capichat_s *room,
															 long duration)
{
#ifdef CC_AST_HAS_VERSION_1_8
	ast_manager_event(c,
#else
	manager_event(
#endif
		EVENT_FLAG_CALL, "CapichatLeave",
		"Channel: %s\r\n"
		"Uniqueid: %s\r\n"
		"Conference: %s\r\n"
		"Conferencenum: %u\r\n"
		"CallerIDNum: %s\r\n"
		"CallerIDName: %s\r\n"
		"Duration: %ld\r\n",
		c->name, c->uniqueid,
		pbx_capi_chat_get_room_name(room),
		pbx_capi_chat_get_room_number(room),
		pbx_capi_get_cid (c, "<unknown>"),
		pbx_capi_get_callername (c, "<no name>"),
		duration);
}

void pbx_capi_chat_conference_end_event(const char* roomName)
{
	manager_event(EVENT_FLAG_CALL, "CapichatEnd", "Conference: %s\r\n", roomName);
}


