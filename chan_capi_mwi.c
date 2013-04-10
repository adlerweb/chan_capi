/*
 *
  Copyright (c) Dialogic(R), 2010

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
#include "chan_capi_utils.h"
#if defined(CC_AST_HAS_EVENT_MWI)
#include <asterisk/event.h>
#endif
#include "chan_capi_mwi.h"

/*
	LOCALS
	*/
static unsigned char* time2X208(time_t t);
#if defined(CC_AST_HAS_EVENT_MWI)
static void pbx_capi_mwi_event(const struct ast_event *event, void *userdata);
#endif

/*
	MWI command parameters of pbx_capi_mwi
	*/
typedef enum _mwiAddSubscribtionParams {
	mwiAddSubscribtionController = 1,
	mwiAddSubscribtionReceivingUserNumber_TypeOfFacilityPartyNumber,
	mwiAddSubscribtionReceivingUserNumber_TypeOfNumberAndNumberingPlan,
	mwiAddSubscribtionReceivingUserNumber_PresentationAndScreening,
	mwiAddSubscribtionReceivingUserNumber,
	mwiAddSubscribtionControllingUserNumber_TypeOfFacilityPartyNumber,
	mwiAddSubscribtionControllingUserNumber_TypeOfNumberAndNumberingPlan,
	mwiAddSubscribtionControllingUserNumber_PresentationAndScreening,
	mwiAddSubscribtionControllingUserNumber,
	mwiAddSubscribtionControllingUserProvidedNumber_TypeOfFacilityPartyNumber,
	mwiAddSubscribtionControllingUserProvidedNumber_TypeOfNumberAndNumberingPlan,
	mwiAddSubscribtionControllingUserProvidedNumber_PresentationAndScreening,
	mwiAddSubscribtionControllingUserProvidedNumber,
	mwiAddSubscribtionMax
} mwiAddSubscribtionParams_t;

typedef enum _mwiRemoveSubscribtionParam {
	mwiRemoveSubscribtionController = 1,
	mwiRemoveSubscribtionReceivingUserNumber,
	mwiRemoveSubscribtionMax
} mwiRemoveSubscribtionParam_t;

typedef enum _mwiXmitActivateParams {
	mwiXmitActivate = 1,
	mwiXmitActivateController,
	mwiXmitActivateBasicService,
	mwiXmitActivateNumberOfMessages,
	mwiXmitActivateMessageStatus,
	mwiXmitActivateMessageReference,
	mwiXmitActivateInvocationMode,
	mwiXmitActivateReceivingUserNumber_TypeOfFacilityPartyNumber,
	mwiXmitActivateReceivingUserNumber_TypeOfNumberAndNumberingPlan,
	mwiXmitActivateReceivingUserNumber_PresentationAndScreening,
	mwiXmitActivateReceivingUserNumber,
	mwiXmitActivateControllingUserNumber_TypeOfFacilityPartyNumber,
	mwiXmitActivateControllingUserNumber_TypeOfNumberAndNumberingPlan,
	mwiXmitActivateControllingUserNumber_PresentationAndScreening,
	mwiXmitActivateControllingUserNumber,
	mwiXmitActivateControllingUserProvidedNumber_TypeOfFacilityPartyNumber,
	mwiXmitActivateControllingUserProvidedNumber_TypeOfNumberAndNumberingPlan,
	mwiXmitActivateControllingUserProvidedNumber_PresentationAndScreening,
	mwiXmitActivateControllingUserProvidedNumber,
	mwiXmitActivatesMax
} mwiXmitActivateParams_t;

typedef enum _mwiXmitDeactivateParams {
	mwiXmitDeactivate = 1,
	mwiXmitDeactivateController,
	mwiXmitDeactivateBasicService,
	mwiXmitDeactivateInvocationMode,
	mwiXmitDeactivateReceivingUserNumber_TypeOfFacilityPartyNumber,
	mwiXmitDeactivateReceivingUserNumber_TypeOfNumberAndNumberingPlan,
	mwiXmitDeactivateReceivingUserNumber_PresentationAndScreening,
	mwiXmitDeactivateReceivingUserNumber,
	mwiXmitDeactivateControllingUserNumber_TypeOfFacilityPartyNumber,
	mwiXmitDeactivateControllingUserNumber_TypeOfNumberAndNumberingPlan,
	mwiXmitDeactivateControllingUserNumber_PresentationAndScreening,
	mwiXmitDeactivateControllingUserNumber,
	mwiXmitDeactivateMax
} mwiXmitDeactivateParams_t;

/*
	add|controller|fpn|ton|pres|receivingUserNumber|fpn|ton|pres|controllingUserNumber|fpn|ton|pres|controllingUserProvidedNumber
	remove|conntroller|receivingUserNumber
	xmit|activate|controller|basicService|numberOfMessages|messageStatus|messageReference|invocationMode|
				fpn|ton|pres|receivingUserNumber|fpn|ton|pres|controllingUserNumber|fpn|ton|pres|controllingUserProvidedNumber
	xmit|deactivate|controller|basicService|invocationMode|fpn|ton|pres|receivingUserNumber|fpn|ton|pres|controllingUserNumber

	add    - add MWI subscription
	remoce - remove MWI subscription
	xmit   - xmit MWI activate or deactivate
	*/
int pbx_capi_mwi(struct ast_channel *c, char *info)
{
	const char* params[MAX((MAX(mwiAddSubscribtionMax, mwiXmitActivatesMax)),(MAX(mwiRemoveSubscribtionMax, mwiXmitDeactivateMax)))];
	int ret = -1;
	int i;

	for (i = 0; i < sizeof(params)/sizeof(params[0]); i++) {
		params[i] = strsep (&info, COMMANDSEPARATOR);
	}

	if (params[0] == NULL)
		return (-1);
	if (strcmp(params[0], "add") == 0) {

	} else if (strcmp (params[0], "remove") == 0) {

	} else if (strcmp (params[0], "xmit") == 0) {
		if (strcmp (params[mwiXmitActivate], "activate") == 0) {
			int unit;

			if (params[mwiXmitActivateController] != 0 &&  params[mwiXmitActivateController][0] != 0) {
				unit = atoi(params[mwiXmitActivateController]);

			} else {
				struct capi_pvt *i = CC_CHANNEL_PVT(c);

				unit = (i != 0) ? i->controller : 0;
			}

			if (pbx_capi_get_controller(unit) != 0 && params[mwiXmitActivateReceivingUserNumber] != 0) {
				unsigned short basicService     = params[mwiXmitActivateBasicService] != 0 ? (unsigned short)atoi(params[mwiXmitActivateBasicService]) : 1;
				unsigned short numberOfMessages = params[mwiXmitActivateNumberOfMessages] != 0 ? (unsigned int)atoi(params[mwiXmitActivateNumberOfMessages]) : 1;
				unsigned short messageStatus    = params[mwiXmitActivateMessageStatus] != 0 ? (unsigned short)atoi(params[mwiXmitActivateMessageStatus]) : 0;
				unsigned short messageReference = params[mwiXmitActivateMessageReference] != 0 ? (unsigned short)atoi(params[mwiXmitActivateMessageReference]) : 0;
				unsigned short invocationMode   = params[mwiXmitActivateInvocationMode] != 0 ? (unsigned short)atoi(params[mwiXmitActivateInvocationMode]) : 2;
				unsigned char ReceivingUserNumber_TypeOfFacilityPartyNumber = params[mwiXmitActivateReceivingUserNumber_TypeOfFacilityPartyNumber] != 0 ?
																				(unsigned char)atoi(params[mwiXmitActivateReceivingUserNumber_TypeOfFacilityPartyNumber]) : 0;
				unsigned char ReceivingUserNumber_TypeOfNumberAndNumberingPlan = params[mwiXmitActivateReceivingUserNumber_TypeOfNumberAndNumberingPlan] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitActivateReceivingUserNumber_TypeOfNumberAndNumberingPlan])) & ~0x80) : 0;
				unsigned char ReceivingUserNumber_PresentationAndScreening = params[mwiXmitActivateReceivingUserNumber_PresentationAndScreening] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitActivateReceivingUserNumber_PresentationAndScreening])) & ~0x80) : 0;
				const char*   ReceivingUserNumber = params[mwiXmitActivateReceivingUserNumber];
				unsigned char ControllingUserNumber_TypeOfFacilityPartyNumber = params[mwiXmitActivateControllingUserNumber_TypeOfFacilityPartyNumber] != 0 ?
																				(unsigned char)atoi(params[mwiXmitActivateControllingUserNumber_TypeOfFacilityPartyNumber]) : 0;
				unsigned char ControllingUserNumber_TypeOfNumberAndNumberingPlan = params[mwiXmitActivateControllingUserNumber_TypeOfNumberAndNumberingPlan] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitActivateControllingUserNumber_TypeOfNumberAndNumberingPlan])) & ~0x80) : 0;
				unsigned char ControllingUserNumber_PresentationAndScreening = params[mwiXmitActivateControllingUserNumber_PresentationAndScreening] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitActivateControllingUserNumber_PresentationAndScreening])) & ~0x80) : 0;
				const char*   ControllingUserNumber = params[mwiXmitActivateControllingUserNumber];
				unsigned char ControllingUserProvidedNumber_TypeOfFacilityPartyNumber = params[mwiXmitActivateControllingUserProvidedNumber_TypeOfFacilityPartyNumber] != 0 ?
																				(unsigned char)atoi(params[mwiXmitActivateControllingUserProvidedNumber_TypeOfFacilityPartyNumber]) : 0;
				unsigned char ControllingUserProvidedNumber_TypeOfNumberAndNumberingPlan = params[mwiXmitActivateControllingUserProvidedNumber_TypeOfNumberAndNumberingPlan] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitActivateControllingUserProvidedNumber_TypeOfNumberAndNumberingPlan])) & ~0x80) : 0;
				unsigned char ControllingUserProvidedNumber_PresentationAndScreening = params[mwiXmitActivateControllingUserProvidedNumber_PresentationAndScreening] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitActivateControllingUserProvidedNumber_PresentationAndScreening])) & ~0x80) : 0;
				const char*   ControllingUserProvidedNumber = params[mwiXmitActivateControllingUserProvidedNumber];
				unsigned char *facilityReceivingUserNumber = pbx_capi_build_facility_number (ReceivingUserNumber_TypeOfFacilityPartyNumber,
																																										 ReceivingUserNumber_TypeOfNumberAndNumberingPlan,
																																										 ReceivingUserNumber_PresentationAndScreening,
																																										 ReceivingUserNumber);
				unsigned char *facilityControllingUserNumber = pbx_capi_build_facility_number (ControllingUserNumber_TypeOfFacilityPartyNumber,
																																											 ControllingUserNumber_TypeOfNumberAndNumberingPlan,
																																											 ControllingUserNumber_PresentationAndScreening,
																																											 ControllingUserNumber);
				unsigned char *facilityControllingUserProvidedNumber = pbx_capi_build_facility_number (ControllingUserProvidedNumber_TypeOfFacilityPartyNumber,
																																															 ControllingUserProvidedNumber_TypeOfNumberAndNumberingPlan,
																																															 ControllingUserProvidedNumber_PresentationAndScreening,
																																															 ControllingUserProvidedNumber);
				unsigned char* t = time2X208(time(NULL));

				ret = pbx_capi_xmit_mwi(pbx_capi_get_controller(unit),
                              basicService,
                              numberOfMessages,
                              messageStatus,
                              messageReference,
                              invocationMode,
                              facilityReceivingUserNumber,
															facilityControllingUserNumber,
															facilityControllingUserProvidedNumber,
                              t);

				ast_free(facilityReceivingUserNumber);
				ast_free(facilityControllingUserNumber);
				ast_free(facilityControllingUserProvidedNumber);
				ast_free(t);
			}
		} else if (strcmp (params[mwiXmitDeactivate], "deactivate") == 0) {
			int unit;

			if (params[mwiXmitDeactivateController] != 0 &&  params[mwiXmitDeactivateController][0] != 0) {
				unit = atoi(params[mwiXmitDeactivateController]);

			} else {
				struct capi_pvt *i = CC_CHANNEL_PVT(c);

				unit = (i != 0) ? i->controller : 0;
			}
			if (pbx_capi_get_controller(unit) != 0 && params[mwiXmitDeactivateReceivingUserNumber] != 0) {
				unsigned short basicService     = params[mwiXmitDeactivateBasicService] != 0 ? (unsigned short)atoi(params[mwiXmitDeactivateBasicService]) : 1;
				unsigned short invocationMode   = params[mwiXmitDeactivateInvocationMode] != 0 ? (unsigned short)atoi(params[mwiXmitDeactivateInvocationMode]) : 2;
				unsigned char ReceivingUserNumber_TypeOfFacilityPartyNumber = params[mwiXmitDeactivateReceivingUserNumber_TypeOfFacilityPartyNumber] != 0 ?
																				(unsigned char)atoi(params[mwiXmitDeactivateReceivingUserNumber_TypeOfFacilityPartyNumber]) : 0;
				unsigned char ReceivingUserNumber_TypeOfNumberAndNumberingPlan = params[mwiXmitDeactivateReceivingUserNumber_TypeOfNumberAndNumberingPlan] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitDeactivateReceivingUserNumber_TypeOfNumberAndNumberingPlan])) & ~0x80) : 0;
				unsigned char ReceivingUserNumber_PresentationAndScreening = params[mwiXmitDeactivateReceivingUserNumber_PresentationAndScreening] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitDeactivateReceivingUserNumber_PresentationAndScreening])) & ~0x80) : 0;
				const char*   ReceivingUserNumber = params[mwiXmitDeactivateReceivingUserNumber];
				unsigned char ControllingUserNumber_TypeOfFacilityPartyNumber = params[mwiXmitDeactivateControllingUserNumber_TypeOfFacilityPartyNumber] != 0 ?
																				(unsigned char)atoi(params[mwiXmitDeactivateControllingUserNumber_TypeOfFacilityPartyNumber]) : 0;
				unsigned char ControllingUserNumber_TypeOfNumberAndNumberingPlan = params[mwiXmitDeactivateControllingUserNumber_TypeOfNumberAndNumberingPlan] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitDeactivateControllingUserNumber_TypeOfNumberAndNumberingPlan])) & ~0x80) : 0;
				unsigned char ControllingUserNumber_PresentationAndScreening = params[mwiXmitDeactivateControllingUserNumber_PresentationAndScreening] != 0 ?
																				(((unsigned char)atoi(params[mwiXmitDeactivateControllingUserNumber_PresentationAndScreening])) & ~0x80) : 0;
				const char*   ControllingUserNumber = params[mwiXmitDeactivateControllingUserNumber];
				unsigned char *facilityReceivingUserNumber = pbx_capi_build_facility_number (ReceivingUserNumber_TypeOfFacilityPartyNumber,
																																										 ReceivingUserNumber_TypeOfNumberAndNumberingPlan,
																																										 ReceivingUserNumber_PresentationAndScreening,
																																										 ReceivingUserNumber);
				unsigned char *facilityControllingUserNumber = pbx_capi_build_facility_number (ControllingUserNumber_TypeOfFacilityPartyNumber,
																																											 ControllingUserNumber_TypeOfNumberAndNumberingPlan,
																																											 ControllingUserNumber_PresentationAndScreening,
																																											 ControllingUserNumber);

				ret = pbx_capi_xmit_mwi_deactivate(pbx_capi_get_controller(unit),
												basicService, invocationMode, facilityReceivingUserNumber, facilityControllingUserNumber);

				ast_free (facilityReceivingUserNumber);
				ast_free (facilityControllingUserNumber);
			}
		}
	}

	return (ret);
}

int pbx_capi_xmit_mwi(
	const struct cc_capi_controller *controller,
	unsigned short basicService, 
	unsigned int   numberOfMessages,
	unsigned short messageStatus,
	unsigned short messageReference,
	unsigned short invocationMode,
	const unsigned char* receivingUserNumber,
	const unsigned char* controllingUserNumber,
	const unsigned char* controllingUserProvidedNumber,
	const unsigned char* timeX208)
{
	MESSAGE_EXCHANGE_ERROR error;
	_cword messageNumber = get_capi_MessageNumber();

	error = capi_sendf(NULL, 0, CAPI_FACILITY_REQ, controller->controller, messageNumber,
		"w(w(wdwwwssssd))",
		0x0003, /* Suppl. Service */
		0x0013, /* MWI Activate */
		basicService, /* Basic Service */
		numberOfMessages, /* Number of messages */
		messageStatus, /* Added messages */
		messageReference, /* Message reference */
		invocationMode, /* Invocation mode */
		receivingUserNumber, /* Receiving user number */
		controllingUserNumber, /* Controlling user number */
		controllingUserProvidedNumber, /* Controlling user provided number */
		timeX208, /* time */
		messageNumber);

	return ((error == CapiNoError) ? 0 : -1);
}

int pbx_capi_xmit_mwi_deactivate(
	const struct cc_capi_controller *controller,
	unsigned short basicService,
	unsigned short invocationMode,
	const unsigned char* receivingUserNumber,
	const unsigned char* controllingUserNumber)
{
	MESSAGE_EXCHANGE_ERROR error;
	_cword messageNumber = get_capi_MessageNumber();

	error = capi_sendf(NULL, 0, CAPI_FACILITY_REQ, controller->controller, messageNumber,
		 "w(w(wwss))",
		 0x0003, /* Suppl. Service */
		 0x0014, /* MWI Activate */
		 basicService, /* Basic Service */
		 invocationMode, /* Invocation mode */
		 receivingUserNumber, /* Receiving user number */
		 controllingUserNumber /* Controlling user number */);

	return ((error == CapiNoError) ? 0 : -1);
}

static unsigned char* time2X208 (time_t t) {
	unsigned char* ret = 0;
#if defined(CC_AST_HAS_VERSION_1_6)
	unsigned char tX208[] = { 0x0c, 0x32, 0x30, 0x30, 0x31, 0x30, 0x35, 0x31, 0x31, 0x30, 0x39, 0x33, 0x36, 0x00 };
  struct timeval tv = {
    .tv_sec = t,
  };
  struct ast_tm tm;

  ast_localtime(&tv, &tm, "utc");
  ast_strftime((char*)&tX208[1], sizeof(tX208)-1, "%Y%d%m%H%M", &tm);

	ret = ast_malloc (sizeof(tX208));
	if (ret != 0)
		memcpy (ret, tX208, sizeof(tX208));
#endif

	return (ret);
}

#if defined(CC_AST_HAS_EVENT_MWI)
static void pbx_capi_mwi_event(const struct ast_event *event, void *userdata)
{
	cc_capi_mwi_mailbox_t* mwiSubscribtion = userdata;
	/* const char *mbox_context; */
	const char *mbox_number;
	int num_messages, num_old_messages;
	unsigned char* t;
	int ret;

	mbox_number = ast_event_get_ie_str(event, AST_EVENT_IE_MAILBOX);
	if (ast_strlen_zero(mbox_number)) {
		 return;
	 }

	/*
	mbox_context = ast_event_get_ie_str(event, AST_EVENT_IE_CONTEXT);
	if (ast_strlen_zero(mbox_context)) {
		return;
	}
	*/

	num_messages = ast_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	num_old_messages = ast_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);

	t = time2X208 (time(NULL));

	cc_verbose (4, 0, "CAPI%d MWI event for '%s@%s' %d messages\n",
		mwiSubscribtion->controller->controller,
		mwiSubscribtion->mailboxNumber+4,
		mwiSubscribtion->mailboxContext,
		num_messages);

	if ((num_messages != 0) || (num_old_messages != 0)) {
		ret = pbx_capi_xmit_mwi(mwiSubscribtion->controller,
			 mwiSubscribtion->basicService, 
			 num_messages,
			 0,
			 0,
			 mwiSubscribtion->invocationMode,
			 mwiSubscribtion->mailboxNumber,
			 mwiSubscribtion->controllingUserNumber,
			 mwiSubscribtion->controllingUserProvidedNumber,
			 t);
	} else {
		ret = pbx_capi_xmit_mwi_deactivate(mwiSubscribtion->controller,
			mwiSubscribtion->basicService,
			mwiSubscribtion->invocationMode,
			mwiSubscribtion->mailboxNumber,
			mwiSubscribtion->controllingUserNumber);
	}

	ast_free(t);
}
#endif

void pbx_capi_register_mwi(struct cc_capi_controller *controller)
{
	cc_capi_mwi_mailbox_t* mwiSubscribtion;

	AST_LIST_TRAVERSE(&controller->mwiSubscribtions, mwiSubscribtion, link) {
#if defined(CC_AST_HAS_EVENT_MWI)
		mwiSubscribtion->mwiSubscribtion = ast_event_subscribe(AST_EVENT_MWI, pbx_capi_mwi_event,
#ifdef CC_AST_HAS_VERSION_1_8
      "CHAN_CAPI mbox event",
#endif
			mwiSubscribtion,
      AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, &mwiSubscribtion->mailboxNumber[4],
      AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR,  mwiSubscribtion->mailboxContext,
      AST_EVENT_IE_END);
#endif
		if (mwiSubscribtion->mwiSubscribtion == 0) {
			cc_log(LOG_WARNING, "CAPI%d failed to activate MWI subscribtion for '%s@%s'\n",
						 mwiSubscribtion->controller->controller,
						 &mwiSubscribtion->mailboxNumber[4],
						 mwiSubscribtion->mailboxContext);
		}
	}
}

void pbx_capi_refresh_mwi(struct cc_capi_controller *controller)
{
	cc_capi_mwi_mailbox_t* mwiSubscribtion;

	AST_LIST_TRAVERSE(&controller->mwiSubscribtions, mwiSubscribtion, link) {
		if (mwiSubscribtion->mwiSubscribtion != 0) {
#if defined(CC_AST_HAS_EVENT_MWI)
			struct ast_event *event = ast_event_get_cached(AST_EVENT_MWI,
					AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, &mwiSubscribtion->mailboxNumber[4],
					AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, mwiSubscribtion->mailboxContext,
					AST_EVENT_IE_END);
			if (event != 0) {
				pbx_capi_mwi_event(event, mwiSubscribtion);
				ast_event_destroy(event);
			}
#endif
		}
	}
}

void pbx_capi_unregister_mwi(struct cc_capi_controller *controller)
{
	cc_capi_mwi_mailbox_t* mwiSubscribtion;

	AST_LIST_TRAVERSE(&controller->mwiSubscribtions, mwiSubscribtion, link) {
		if (mwiSubscribtion->mwiSubscribtion != 0) {
#if defined(CC_AST_HAS_EVENT_MWI)
			ast_event_unsubscribe(mwiSubscribtion->mwiSubscribtion);
#endif
			mwiSubscribtion->mwiSubscribtion = 0;
		}
	}
}

void pbx_capi_cleanup_mwi(struct cc_capi_controller *controller)
{
	cc_capi_mwi_mailbox_t* mwiSubscribtion;

	pbx_capi_unregister_mwi(controller);

	while ((mwiSubscribtion = AST_LIST_REMOVE_HEAD(&controller->mwiSubscribtions, link)) != 0) {
		ast_free (mwiSubscribtion->mailboxNumber);
		ast_free (mwiSubscribtion->mailboxContext);
		ast_free (mwiSubscribtion->controllingUserNumber);
		ast_free (mwiSubscribtion->controllingUserProvidedNumber);
		ast_free (mwiSubscribtion);
	}
}

unsigned char* pbx_capi_build_facility_number(
	unsigned char mwifacptynrtype,
	unsigned char mwifacptynrton,
	unsigned char mwifacptynrpres,
	const char* number)
{
	unsigned char* fnr = 0;

	if (number != 0) {
		fnr = ast_malloc (strlen(number)+7);
		if (fnr != 0) {
			fnr[0] = strlen(number) + 3;
			fnr[1] = mwifacptynrtype;
			fnr[2] = mwifacptynrton;
			fnr[3] = mwifacptynrpres | 0x80;
			strcpy((char*)&fnr[4], number);
		}
	}

	return (fnr);
}

/*
	Init MWI subscriptions
	*/
void pbx_capi_init_mwi_server (
	struct cc_capi_controller *mwiController,
	const struct cc_capi_conf *conf) {

	if ((mwiController != 0) && (conf->mwimailbox != 0)) {
		char* mailboxList = conf->mwimailbox;
		char* mailboxMember;

		while ((mailboxMember = strsep (&mailboxList, ",")) != 0) {
			/*
				Mailbox format: extension[:extension1[:extension2]][@context]
			*/
			char* mailboxNumbers                      = strsep(&mailboxMember, "@");
			const char* mailboxContext                = (mailboxMember != 0) ? mailboxMember : "default";
			const char* mailboxNumber                 = strsep (&mailboxNumbers, ":");
			const char* controllingUserNumber         = strsep (&mailboxNumbers, ":");
			const char* controllingUserProvidedNumber = mailboxNumbers;
			if ((mailboxNumber != 0) && (*mailboxNumber != 0)) {
				cc_capi_mwi_mailbox_t* mwiSubscribtion = ast_malloc(sizeof(*mwiSubscribtion));
				if (mwiSubscribtion != 0) {
					mwiSubscribtion->mailboxNumber = pbx_capi_build_facility_number(conf->mwifacptynrtype, conf->mwifacptynrton, conf->mwifacptynrpres, mailboxNumber);
					mwiSubscribtion->mailboxContext = ast_strdup(mailboxContext);
					mwiSubscribtion->controllingUserNumber = pbx_capi_build_facility_number(conf->mwifacptynrtype, conf->mwifacptynrton, conf->mwifacptynrpres, controllingUserNumber);
					mwiSubscribtion->controllingUserProvidedNumber = pbx_capi_build_facility_number(conf->mwifacptynrtype, conf->mwifacptynrton, conf->mwifacptynrpres, controllingUserProvidedNumber);
					mwiSubscribtion->controller = mwiController;
					mwiSubscribtion->mwiSubscribtion = 0;
					mwiSubscribtion->basicService    = conf->mwibasicservice;
					mwiSubscribtion->invocationMode  = conf->mwiinvocation;

					if ((mwiSubscribtion->mailboxNumber == 0) || (mwiSubscribtion->mailboxContext == 0) ||
							((mwiSubscribtion->controllingUserNumber == 0) && (controllingUserNumber != 0)) ||
							((mwiSubscribtion->controllingUserProvidedNumber == 0) && (controllingUserProvidedNumber != 0))) {
						ast_free(mwiSubscribtion->mailboxNumber);
						ast_free(mwiSubscribtion->mailboxContext);
						ast_free(mwiSubscribtion->controllingUserNumber);
						ast_free(mwiSubscribtion->controllingUserProvidedNumber);
						ast_free(mwiSubscribtion);
					} else {
						cc_verbose (4, 0, "CAPI%d add MWI subscribtion for '%s@%s' user '%s' control '%s'\n",
												mwiSubscribtion->controller->controller,
												mwiSubscribtion->mailboxNumber + 4,
												mwiSubscribtion->mailboxContext,
												(mwiSubscribtion->controllingUserNumber != 0) ? (char*)mwiSubscribtion->controllingUserNumber+4 : "",
												(mwiSubscribtion->controllingUserProvidedNumber != 0) ? (char*)mwiSubscribtion->controllingUserProvidedNumber+4 : "");

						memset(&mwiSubscribtion->link, 0x00, sizeof(mwiSubscribtion->link));
						AST_LIST_INSERT_TAIL(&mwiController->mwiSubscribtions, mwiSubscribtion, link);
					}
				}
			}
		}
	}
}

