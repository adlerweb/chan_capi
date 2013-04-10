/*
 * An implementation of Common ISDN API 2.0 for Asterisk
 *
 * Copyright (C) 2006-2009 Cytronics & Melware
 *
 * Armin Schindler <armin@melware.de>
 * 
 * capi_sendf() by Eicon Networks / Dialogic
 *
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include "chan_capi_platform.h"
#include "xlaw.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_rtp.h"
#include "chan_capi_utils.h"
#include "chan_capi_supplementary.h"

#ifdef DIVA_STREAMING
#include "platform.h"
#include "diva_streaming_result.h"
#include "diva_streaming_messages.h"
#include "diva_streaming_vector.h"
#include "diva_streaming_manager.h"
#include "chan_capi_divastreaming_utils.h"
#endif
#ifdef DIVA_STATUS
#include "divastatus_ifc.h"
#define CC_HW_STATE_OK(__x__) ((pbx_capi_get_controller((__x__)) == NULL) || \
																(pbx_capi_get_controller((__x__))->hwState != (int)DivaStatusHardwareStateERROR))
#else
#define CC_HW_STATE_OK(__x__) (1)
#endif

int capidebug = 0;
char *emptyid = "\0";

AST_MUTEX_DEFINE_STATIC(verbose_lock);
AST_MUTEX_DEFINE_STATIC(messagenumber_lock);
AST_MUTEX_DEFINE_STATIC(capi_put_lock);
AST_MUTEX_DEFINE_STATIC(peerlink_lock);
AST_MUTEX_DEFINE_STATIC(nullif_lock);

static _cword capi_MessageNumber;

static struct capi_pvt *nulliflist = NULL;
static int controller_nullplcis[CAPI_MAX_CONTROLLERS];

#define CAPI_MAX_PEERLINKCHANNELS  32
static struct peerlink_s {
	struct ast_channel *channel;
	time_t age;
} peerlinkchannel[CAPI_MAX_PEERLINKCHANNELS];

/*
 * helper for <pbx>_verbose
 */
void cc_verbose_internal(char *text, ...)
{
	char line[4096];
	va_list ap;

	va_start(ap, text);
	vsnprintf(line, sizeof(line), text, ap);
	va_end(ap);
	line[sizeof(line)-1]=0;

#if 0
	{
		FILE *fp;
		if ((fp = fopen("/tmp/cclog", "a")) != NULL) {
			fprintf(fp, "%s", line);
			fclose(fp);
		}
	}
#endif

	cc_mutex_lock(&verbose_lock);
	cc_pbx_verbose("%s", line);
	cc_mutex_unlock(&verbose_lock);	
}

/*
 * hangup and remove null-interface
 */
void capi_remove_nullif(struct capi_pvt *i)
{
	struct capi_pvt *ii;
	struct capi_pvt *tmp = NULL;
	int state;

	if (i->channeltype != CAPI_CHANNELTYPE_NULL) {
		return;
	}

	cc_mutex_lock(&i->lock);
	if (i->line_plci != 0) {
		ii = i->line_plci;
		i->line_plci = 0;
		capi_remove_nullif(ii);
	}
	cc_mutex_unlock(&i->lock);

	if (i->PLCI != 0) {
		/* if the interface is in use, hangup first */
		cc_mutex_lock(&i->lock);
		state = i->state;
		i->state = CAPI_STATE_DISCONNECTING;
		capi_activehangup(i, state);

		cc_mutex_unlock(&i->lock);

		return;
	}

	cc_mutex_lock(&nullif_lock);
	ii = nulliflist;
	while (ii) {
		if (ii == i) {
			if (!tmp) {
				nulliflist = ii->next;
			} else {
				tmp->next = ii->next;
			}
			cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: removed null-interface from controller %d.\n",
				i->vname, i->controller);
			if (i->smoother) {
				ast_smoother_free(i->smoother);
				i->smoother = 0;
			}
			cc_mutex_destroy(&i->lock);
			ast_cond_destroy(&i->event_trigger);
			controller_nullplcis[i->controller - 1]--;
			ast_free(i);
			break;
		}
		tmp = ii;
		ii = ii->next;
	}
	cc_mutex_unlock(&nullif_lock);
}

int capi_verify_resource_plci(const struct capi_pvt *i) {
	const struct capi_pvt *ii;

	cc_mutex_lock(&nullif_lock);
	for (ii = nulliflist; ii != 0 && ii != i; ii = ii->next);
	cc_mutex_unlock(&nullif_lock);

	return ((ii == i) ? 0 : -1);
}

/*
 * create new null-interface
 */
struct capi_pvt *capi_mknullif(struct ast_channel *c, unsigned long long controllermask)
{
	struct capi_pvt *tmp;
	unsigned int controller = 1;
	int contrcount;
	int channelcount = 0xffff;
	int maxcontr = (CAPI_MAX_CONTROLLERS > (sizeof(controllermask)*8)) ?
		(sizeof(controllermask)*8) : CAPI_MAX_CONTROLLERS;

	cc_verbose(3, 1, VERBOSE_PREFIX_4 "capi_mknullif: find controller for mask 0x%lx\n",
		controllermask);
	/* find the next controller of mask with least plcis used */	
	for (contrcount = 0; contrcount < maxcontr; contrcount++) {
		if (((controllermask & (1ULL << contrcount)) != 0) && CC_HW_STATE_OK(contrcount + 1)) {
			if (controller_nullplcis[contrcount] < channelcount) {
				channelcount = controller_nullplcis[contrcount];
				controller = contrcount + 1;
			}
		}
	}

	tmp = ast_malloc(sizeof(struct capi_pvt));
	if (!tmp) {
		return NULL;
	}
	memset(tmp, 0, sizeof(struct capi_pvt));
	
	cc_mutex_init(&tmp->lock);
	ast_cond_init(&tmp->event_trigger, NULL);
	
	snprintf(tmp->name, sizeof(tmp->name) - 1, "%s-NULLPLCI", (c != 0) ? c->name : "BRIDGE");
	snprintf(tmp->vname, sizeof(tmp->vname) - 1, "%s", tmp->name);

	tmp->channeltype = CAPI_CHANNELTYPE_NULL;

	tmp->used = c;
	tmp->peer = c;
	if (c == NULL)
		tmp->virtualBridgePeer = 1;

	tmp->cip = CAPI_CIPI_SPEECH;
	tmp->transfercapability = PRI_TRANS_CAP_SPEECH;
	tmp->controller = controller;
	tmp->doEC = 1;
	tmp->doEC_global = 1;
	tmp->ecOption = EC_OPTION_DISABLE_NEVER;
	tmp->ecTail = EC_DEFAULT_TAIL;
	tmp->isdnmode = CAPI_ISDNMODE_MSN;
	tmp->ecSelector = FACILITYSELECTOR_ECHO_CANCEL;
	tmp->capability = capi_capability;

	tmp->rxgain = 1.0;
	tmp->txgain = 1.0;
	capi_gains(&tmp->g, 1.0, 1.0);

	if (c != 0) {
		if (!(capi_create_reader_writer_pipe(tmp))) {
			ast_free(tmp);
			return NULL;
		}
	}

	tmp->bproto = CC_BPROTO_TRANSPARENT;	
	tmp->doB3 = CAPI_B3_DONT;
	tmp->smoother = ast_smoother_new(CAPI_MAX_B3_BLOCK_SIZE);
	tmp->isdnstate |= CAPI_ISDN_STATE_PBX;
		
	cc_mutex_lock(&nullif_lock);
	tmp->next = nulliflist; /* prepend */
	nulliflist = tmp;
	controller_nullplcis[tmp->controller - 1]++;
	cc_mutex_unlock(&nullif_lock);

	/* connect to driver */
	tmp->outgoing = 1;
	tmp->state = CAPI_STATE_CONNECTPENDING;
	tmp->MessageNumber = get_capi_MessageNumber();

#ifdef DIVA_STREAMING
	tmp->diva_stream_entry = 0;
	if (pbx_capi_streaming_supported (tmp) != 0) {
		capi_DivaStreamingOn(tmp, 1, tmp->MessageNumber);
	}
#endif

	if (c == NULL) {
		cc_mutex_lock(&tmp->lock);
	}
	capi_sendf((c != NULL) ? NULL : tmp, c == NULL, CAPI_CONNECT_REQ, controller, tmp->MessageNumber,
		"w()()()()(www()()()())()()()((wwbbb)()()())",
		 0,       1,1,0,              3,0,0,0,0);
	if (c == NULL) {
		cc_mutex_unlock(&tmp->lock);
		if (tmp->PLCI == 0) {
			cc_log(LOG_WARNING, "%s: failed to create\n", tmp->vname);
			capi_remove_nullif(tmp);
			tmp = NULL;
		}
	}
	if (tmp != NULL) {
		cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: created null-interface on controller %d.\n",
				tmp->vname, tmp->controller);
	}

	return tmp;
}

struct capi_pvt *capi_mkresourceif(
	struct ast_channel *c,
	unsigned long long controllermask,
	struct capi_pvt *data_plci_ifc,
	cc_format_t codecs,
	int all)
{
	struct capi_pvt *data_ifc /*, *line_ifc */;
	unsigned int controller = 1;
	int fmt = 0;

	if (data_plci_ifc == 0) {
		int contrcount;
		int channelcount = 0xffff;
		int maxcontr = (CAPI_MAX_CONTROLLERS > (sizeof(controllermask)*8)) ?
			(sizeof(controllermask)*8) : CAPI_MAX_CONTROLLERS;

		cc_verbose(3, 1, VERBOSE_PREFIX_4 "capi_mkresourceif: find controller for mask 0x%lx\n",
			controllermask);

		/* find the next controller of mask with least plcis used */	
		for (contrcount = 0; contrcount < maxcontr; contrcount++) {
			if (((controllermask & (1ULL << contrcount)) != 0) && CC_HW_STATE_OK(contrcount + 1)) {
				if (controller_nullplcis[contrcount] < channelcount) {
					channelcount = controller_nullplcis[contrcount];
					controller = contrcount + 1;
				}
			}
		}
	} else {
		controller = data_plci_ifc->controller;
		codecs = (all != 0) ? pbx_capi_get_controller_codecs (controller) : codecs;
		fmt = pbx_capi_get_controller_codecs (controller) & codecs & cc_get_formats_as_bits(c->nativeformats);
		if (fmt != 0)
			fmt = cc_get_best_codec_as_bits(fmt);
	}

	data_ifc = ast_malloc(sizeof(struct capi_pvt));
	if (data_ifc == 0) {
		return NULL;
	}
	memset(data_ifc, 0, sizeof(struct capi_pvt));
	
	cc_mutex_init(&data_ifc->lock);
	ast_cond_init(&data_ifc->event_trigger, NULL);
	
	snprintf(data_ifc->name, sizeof(data_ifc->name) - 1, "%s-%sPLCI", c->name, (data_plci_ifc == 0) ? "DATA" : "LINE");
	snprintf(data_ifc->vname, sizeof(data_ifc->vname) - 1, "%s", data_ifc->name);

	data_ifc->channeltype = CAPI_CHANNELTYPE_NULL;
	data_ifc->resource_plci_type = (data_plci_ifc == 0) ? CAPI_RESOURCE_PLCI_DATA : CAPI_RESOURCE_PLCI_LINE;

	data_ifc->used = c;
	data_ifc->peer = c;

	data_ifc->cip = CAPI_CIPI_SPEECH;
	data_ifc->transfercapability = PRI_TRANS_CAP_SPEECH;
	data_ifc->controller = controller;
	data_ifc->doEC = 1;
	data_ifc->doEC_global = 1;
	data_ifc->ecOption = EC_OPTION_DISABLE_NEVER;
	data_ifc->ecTail = EC_DEFAULT_TAIL;
	data_ifc->isdnmode = CAPI_ISDNMODE_MSN;
	data_ifc->ecSelector = FACILITYSELECTOR_ECHO_CANCEL;
	data_ifc->capability = (fmt != 0 && data_plci_ifc != 0) ? fmt : capi_capability;
	data_ifc->codec      = (fmt != 0 && data_plci_ifc != 0) ? fmt : data_ifc->codec;

	data_ifc->rxgain = 1.0;
	data_ifc->txgain = 1.0;
	capi_gains(&data_ifc->g, 1.0, 1.0);

	if (data_plci_ifc == 0) {
		if (!(capi_create_reader_writer_pipe(data_ifc))) {
			ast_free(data_ifc);
			return NULL;
		}
	} else {
		data_ifc->readerfd = -1;
		data_ifc->writerfd = -1;
	}

	data_ifc->bproto = (fmt != 0 && data_plci_ifc != 0) ? CC_BPROTO_VOCODER : CC_BPROTO_TRANSPARENT;
	data_ifc->doB3 = CAPI_B3_DONT;
	data_ifc->smoother = ast_smoother_new(CAPI_MAX_B3_BLOCK_SIZE);
	data_ifc->isdnstate |= CAPI_ISDN_STATE_PBX;
		
	cc_mutex_lock(&nullif_lock);
	data_ifc->next = nulliflist; /* prepend */
	nulliflist = data_ifc;
	controller_nullplcis[data_ifc->controller - 1]++;
	cc_mutex_unlock(&nullif_lock);

	/* connect to driver */
	data_ifc->outgoing = 1;
	data_ifc->state = CAPI_STATE_CONNECTPENDING;
	data_ifc->MessageNumber = get_capi_MessageNumber();

	cc_mutex_lock(&data_ifc->lock);

#ifdef DIVA_STREAMING
	data_ifc->diva_stream_entry = 0;
	if (data_plci_ifc == 0) {
		capi_DivaStreamingStreamNotUsed(data_ifc, 1, data_ifc->MessageNumber);
	} else {
		if (pbx_capi_streaming_supported (data_ifc) != 0) {
			capi_DivaStreamingOn(data_ifc, 1, data_ifc->MessageNumber);
		}
	}
#endif

	capi_sendf(data_ifc,
		1,
		CAPI_MANUFACTURER_REQ,
		controller,
		data_ifc->MessageNumber,
		"dw(wbb(wwws()()()))",
		_DI_MANU_ID,
		_DI_ASSIGN_PLCI,
		(data_plci_ifc == 0) ? 4 : 5, /* data */
		(data_plci_ifc == 0) ? 0 : (unsigned char)(data_plci_ifc->PLCI >> 8), /* bchannel */
		1, /* connect */
		(data_ifc->bproto == CC_BPROTO_VOCODER) ? 0x1f : 1, 1, 0,
		diva_get_b1_conf(data_ifc));
	cc_mutex_unlock(&data_ifc->lock);

	if (data_plci_ifc != 0) {
		if (data_ifc->PLCI == 0) {
			cc_log(LOG_WARNING, "%s: failed to create\n", data_ifc->vname);
			capi_remove_nullif(data_ifc);
			data_ifc = 0;
		} else {
			cc_mutex_lock(&data_plci_ifc->lock);
			data_plci_ifc->line_plci = data_ifc;
			capi_sendf(data_plci_ifc, 1, CAPI_FACILITY_REQ, data_plci_ifc->PLCI, get_capi_MessageNumber(),
				"w(w(d()))",
				FACILITYSELECTOR_LINE_INTERCONNECT,
				0x0001, /* CONNECT */
				0x00000000 /* mask */
			);
			cc_mutex_unlock(&data_plci_ifc->lock);

			data_ifc->data_plci      = data_plci_ifc;

			data_ifc->writerfd = data_plci_ifc->writerfd;
			data_plci_ifc->writerfd = -1;
		}
	}

	if (data_ifc != 0) {
		cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: created %s-resource-interface on controller %d.\n",
			data_ifc->vname, (data_plci_ifc == 0) ? "data" : "line", data_ifc->controller);
	}

	return data_ifc;
}

/*
 * get a new capi message number atomically
 */
_cword get_capi_MessageNumber(void)
{
	_cword mn;

	cc_mutex_lock(&messagenumber_lock);

	capi_MessageNumber++;
	if (capi_MessageNumber == 0) {
	    /* avoid zero */
	    capi_MessageNumber = 1;
	}

	mn = capi_MessageNumber;

	cc_mutex_unlock(&messagenumber_lock);

	return mn;
}

/*
 * find the interface (pvt) the PLCI belongs to
 */
struct capi_pvt *capi_find_interface_by_plci(unsigned int plci)
{
	struct capi_pvt *i;

	if (unlikely(plci == 0))
		return NULL;

	for (i = capi_iflist; i; i = i->next) {
		if (i->PLCI == plci)
			return i;
	}

	cc_mutex_lock(&nullif_lock);
	for (i = nulliflist; i; i = i->next) {
		if (i->PLCI == plci)
			break;
	}
	cc_mutex_unlock(&nullif_lock);

	return i;
}

/*
 * find the interface (pvt) the messagenumber belongs to
 */
struct capi_pvt *capi_find_interface_by_msgnum(unsigned short msgnum)
{
	struct capi_pvt *i;

	if (msgnum == 0x0000)
		return NULL;

	for (i = capi_iflist; i; i = i->next) {
	    if ((i->PLCI == 0) && (i->MessageNumber == msgnum))
			return i;
	}

	cc_mutex_lock(&nullif_lock);
	for (i = nulliflist; i; i = i->next) {
	    if ((i->PLCI == 0) && (i->MessageNumber == msgnum))
			break;
	}
	cc_mutex_unlock(&nullif_lock);

	return i;
}

/*
 * wait for a specific message
 */
MESSAGE_EXCHANGE_ERROR capi_wait_conf(struct capi_pvt *i, unsigned short wCmd)
{
	MESSAGE_EXCHANGE_ERROR error = 0;
	struct timespec abstime;
	unsigned char command, subcommand;

	subcommand = wCmd & 0xff;
	command = (wCmd & 0xff00) >> 8;
	i->waitevent = (unsigned int)wCmd;
	abstime.tv_sec = time(NULL) + 2;
	abstime.tv_nsec = 0;
	cc_verbose(4, 1, "%s: wait for %s (0x%x)\n",
		i->vname, capi_cmd2str(command, subcommand), i->waitevent);
	if (ast_cond_timedwait(&i->event_trigger, &i->lock, &abstime) != 0) {
		error = -1;
		cc_log(LOG_WARNING, "%s: timed out waiting for %s\n",
			i->vname, capi_cmd2str(command, subcommand));
	} else {
		cc_verbose(4, 1, "%s: cond signal received for %s\n",
			i->vname, capi_cmd2str(command, subcommand));
	}
	return error;
}

/*
 * log an error in sending capi message
 */
static void log_capi_error_message(MESSAGE_EXCHANGE_ERROR err, unsigned char* msg)
{
	if (err) {
		_cmsg _CMSG, *CMSG = &_CMSG;

		capi_message2cmsg(CMSG, msg);
		cc_log(LOG_ERROR, "CAPI error sending %s (NCCI=%#x) (error=%#x %s)\n",
			capi_cmsg2str(CMSG), (unsigned int)HEADER_CID(CMSG),
			err, capi_info_string((unsigned int)err));
	}
}

/*
 * log verbose a capi message
 */
static void log_capi_message(_cmsg *CMSG)
{
	unsigned short wCmd;

	wCmd = HEADER_CMD(CMSG);
	if ((wCmd == CAPI_P_REQ(DATA_B3)) ||
	    (wCmd == CAPI_P_RESP(DATA_B3))) {
		cc_verbose(7, 1, "%s\n", capi_cmsg2str(CMSG));
	} else {
		cc_verbose(4, 1, "%s\n", capi_cmsg2str(CMSG));
	}
}

/*
 * write a capi message to capi device
 */
static MESSAGE_EXCHANGE_ERROR _capi_put_msg(unsigned char *msg)
{
	MESSAGE_EXCHANGE_ERROR error;
	_cmsg CMSG;
	
	if (cc_mutex_lock(&capi_put_lock)) {
		cc_log(LOG_WARNING, "Unable to lock chan_capi put!\n");
		return -1;
	} 

	if (cc_verbose_check(4, 1) != 0) {
		capi_message2cmsg(&CMSG, msg);
		log_capi_message(&CMSG);
	}

	error = capi20_put_message(capi_ApplID, msg);
	
	if (cc_mutex_unlock(&capi_put_lock)) {
		cc_log(LOG_WARNING, "Unable to unlock chan_capi put!\n");
		return -1;
	}

	log_capi_error_message(error, msg);

	return error;
}

/*
 * wait some time for a new capi message
 */
MESSAGE_EXCHANGE_ERROR capidev_check_wait_get_cmsg(_cmsg *CMSG)
{
	MESSAGE_EXCHANGE_ERROR Info;
	struct timeval tv;

	tv.tv_sec = 0;
#ifdef DIVA_STREAMING
	tv.tv_usec = 5000;
#else
	tv.tv_usec = 500000;
#endif

	Info = capi20_waitformessage(capi_ApplID, &tv);

	if (Info == 0x0000) {
		
		Info = capi_get_cmsg(CMSG, capi_ApplID);

#if (CAPI_OS_HINT == 1) || (CAPI_OS_HINT == 2)
		if (Info == 0x0000) {
			/*
			 * For BSD allow controller 0:
			 */
			if ((HEADER_CID(CMSG) & 0xFF) == 0) {
				HEADER_CID(CMSG) += capi_num_controllers;
		 	}
		}
#endif
	}

	if ((Info != 0x0000) && (Info != 0x1104)) {
		if (capidebug) {
			cc_log(LOG_DEBUG, "Error waiting for cmsg... INFO = %#x\n", Info);
		}
	}
    
	return Info;
}

/*
 * Eicon's capi_sendf() function to create capi messages easily
 * and send this message.
 * Copyright by Eicon Networks / Dialogic
 */
MESSAGE_EXCHANGE_ERROR capi_sendf(
	struct capi_pvt *capii, int waitconf,
	_cword command, _cdword Id, _cword Number, char * format, ...)
{
	MESSAGE_EXCHANGE_ERROR ret;
	int i, j;
	unsigned int d;
	unsigned char *p, *p_length;
	unsigned char *string;
	unsigned short header_length;
	va_list ap;
	capi_prestruct_t *s;
	unsigned char msg[2048];

	write_capi_word(&msg[2], capi_ApplID);
	msg[4] = (unsigned char)((command >> 8) & 0xff);
	msg[5] = (unsigned char)(command & 0xff);
	write_capi_word(&msg[6], Number);
	write_capi_dword(&msg[8], Id);

	p = &msg[12];
	p_length = 0;

	va_start(ap, format);
	for (i = 0; format[i]; i++) {
		if (unlikely(((p - (&msg[0])) + 12) >= sizeof(msg))) {
			cc_log(LOG_ERROR, "capi_sendf: message too big (%d)\n",
				(int)(p - (&msg[0])));
			return 0x1004;
		}
		switch(format[i]) {
		case 'b': /* byte */
			d = (unsigned char)va_arg(ap, unsigned int);
			*(p++) = (unsigned char) d;
			break;
		case 'w': /* word (2 bytes) */
			d = (unsigned short)va_arg(ap, unsigned int);
			*(p++) = (unsigned char) d;
			*(p++) = (unsigned char)(d >> 8);
			break;
		case 'd': /* double word (4 bytes) */
			d = va_arg(ap, unsigned int);
			*(p++) = (unsigned char) d;
			*(p++) = (unsigned char)(d >> 8);
			*(p++) = (unsigned char)(d >> 16);
			*(p++) = (unsigned char)(d >> 24);
			break;
		case 's': /* struct, length is the first byte */
			string = va_arg(ap, unsigned char *);
			if (string == NULL) {
				*(p++) = 0;
			} else {
				for (j = 0; j <= string[0]; j++)
					*(p++) = string[j];
			}
			break;
		case 'a': /* ascii string, NULL terminated string */
			string = va_arg(ap, unsigned char *);
			for (j = 0; string[j] != '\0'; j++)
				*(++p) = string[j];
			*((p++)-j) = (unsigned char) j;
			break;
		case 'c': /* predefined capi_prestruct_t */
			s = va_arg(ap, capi_prestruct_t *);
			if (s->wLen < 0xff) {
				*(p++) = (unsigned char)(s->wLen);
			} else	{
				*(p++) = 0xff;
				*(p++) = (unsigned char)(s->wLen);
				*(p++) = (unsigned char)(s->wLen >> 8);
			}
			for (j = 0; j < s->wLen; j++)
				*(p++) = s->info[j];
			break;
		case '(': /* begin of a structure */
			*p = (p_length) ? p - p_length : 0;
			p_length = p++;
			break;
		case ')': /* end of structure */
			if (p_length) {
				j = *p_length;
				*p_length = (unsigned char)((p - p_length) - 1);
				p_length = (j != 0) ? p_length - j : 0;
			} else {
				cc_log(LOG_ERROR, "capi_sendf: inconsistent format \"%s\"\n",
					format);
			}
			break;
		default:
			cc_log(LOG_ERROR, "capi_sendf: unknown format \"%s\"\n",
				format);
		}
	}
	va_end(ap);

	if (p_length) {
		cc_log(LOG_ERROR, "capi_sendf: inconsistent format \"%s\"\n", format);
	}

	header_length = (unsigned short)(p - (&msg[0]));

	if ((sizeof(void *) > 4) && (command == CAPI_DATA_B3_REQ)) {
		void* req_data;
		va_start(ap, format);
		req_data = va_arg(ap, void *);
		va_end(ap);

		header_length += 8;
		write_capi_dword(&msg[12], 0);
		memcpy(&msg[22], &req_data, sizeof(void *));
	}

	write_capi_word(&msg[0], header_length);

	ret = _capi_put_msg(&msg[0]);
	if ((!(ret)) && (waitconf)) {
		ret = capi_wait_conf(capii, (command & 0xff00) | CAPI_CONF);
	}

	return ret;
}

/*
 * decode capi 2.0 info word
 */
char *capi_info_string(unsigned int info)
{
	switch (info) {
	/* informative values (corresponding message was processed) */
	case 0x0001:
		return "NCPI not supported by current protocol, NCPI ignored";
	case 0x0002:
		return "Flags not supported by current protocol, flags ignored";
	case 0x0003:
		return "Alert already sent by another application";

	/* error information concerning CAPI_REGISTER */
	case 0x1001:
		return "Too many applications";
	case 0x1002:
		return "Logical block size to small, must be at least 128 Bytes";
	case 0x1003:
		return "Buffer exceeds 64 kByte";
	case 0x1004:
		return "Message buffer size too small, must be at least 1024 Bytes";
	case 0x1005:
		return "Max. number of logical connections not supported";
	case 0x1006:
		return "Reserved";
	case 0x1007:
		return "The message could not be accepted because of an internal busy condition";
	case 0x1008:
		return "OS resource error (no memory ?)";
	case 0x1009:
		return "CAPI not installed";
	case 0x100A:
		return "Controller does not support external equipment";
	case 0x100B:
		return "Controller does only support external equipment";

	/* error information concerning message exchange functions */
	case 0x1101:
		return "Illegal application number";
	case 0x1102:
		return "Illegal command or subcommand or message length less than 12 bytes";
	case 0x1103:
		return "The message could not be accepted because of a queue full condition !! The error code does not imply that CAPI cannot receive messages directed to another controller, PLCI or NCCI";
	case 0x1104:
		return "Queue is empty";
	case 0x1105:
		return "Queue overflow, a message was lost !! This indicates a configuration error. The only recovery from this error is to perform a CAPI_RELEASE";
	case 0x1106:
		return "Unknown notification parameter";
	case 0x1107:
		return "The Message could not be accepted because of an internal busy condition";
	case 0x1108:
		return "OS Resource error (no memory ?)";
	case 0x1109:
		return "CAPI not installed";
	case 0x110A:
		return "Controller does not support external equipment";
	case 0x110B:
		return "Controller does only support external equipment";

	/* error information concerning resource / coding problems */
	case 0x2001:
		return "Message not supported in current state";
	case 0x2002:
		return "Illegal Controller / PLCI / NCCI";
	case 0x2003:
		return "Out of PLCIs";
	case 0x2004:
		return "Out of NCCIs";
	case 0x2005:
		return "Out of LISTEN requests";
	case 0x2006:
		return "Out of FAX resources (protocol T.30)";
	case 0x2007:
		return "Illegal message parameter coding";

	/* error information concerning requested services */
	case 0x3001:
		return "B1 protocol not supported";
	case 0x3002:
		return "B2 protocol not supported";
	case 0x3003:
		return "B3 protocol not supported";
	case 0x3004:
		return "B1 protocol parameter not supported";
	case 0x3005:
		return "B2 protocol parameter not supported";
	case 0x3006:
		return "B3 protocol parameter not supported";
	case 0x3007:
		return "B protocol combination not supported";
	case 0x3008:
		return "NCPI not supported";
	case 0x3009:
		return "CIP Value unknown";
	case 0x300A:
		return "Flags not supported (reserved bits)";
	case 0x300B:
		return "Facility not supported";
	case 0x300C:
		return "Data length not supported by current protocol";
	case 0x300D:
		return "Reset procedure not supported by current protocol";
	case 0x300E:
		return "TEI assignment failed or supplementary service not supported";
	case 0x3010:
		return "Request not allowed in this state";

	/* informations about the clearing of a physical connection */
	case 0x3301:
		return "Protocol error layer 1 (broken line or B-channel removed by signalling protocol)";
	case 0x3302:
		return "Protocol error layer 2";
	case 0x3303:
		return "Protocol error layer 3";
	case 0x3304:
		return "Another application got that call";

	/* T.30 specific reasons */
	case 0x3311:
		return "Connecting not successful (remote station is no FAX G3 machine)";
	case 0x3312:
		return "Connecting not successful (training error)";
	case 0x3313:
		return "Disconnected before transfer (remote station does not support transfer mode, e.g. resolution)";
	case 0x3314:
		return "Disconnected during transfer (remote abort)";
	case 0x3315:
		return "Disconnected during transfer (remote procedure error, e.g. unsuccessful repetition of T.30 commands)";
	case 0x3316:
		return "Disconnected during transfer (local tx data underrun)";
	case 0x3317:
		return "Disconnected during transfer (local rx data overflow)";
	case 0x3318:
		return "Disconnected during transfer (local abort)";
	case 0x3319:
		return "Illegal parameter coding (e.g. SFF coding error)";

	/* disconnect causes from the network according to ETS 300 102-1/Q.931 */
	case 0x3481:
		return "Unallocated (unassigned) number";
	case 0x3482:
		return "No route to specified transit network";
	case 0x3483:
		return "No route to destination";
	case 0x3486:
		return "Channel unacceptable";
	case 0x3487:
		return "Call awarded and being delivered in an established channel";
	case 0x3490:
		return "Normal call clearing";
	case 0x3491:
		return "User busy";
	case 0x3492:
		return "No user responding";
	case 0x3493:
		return "No answer from user (user alerted)";
	case 0x3495:
		return "Call rejected";
	case 0x3496:
		return "Number changed";
	case 0x349A:
		return "Non-selected user clearing";
	case 0x349B:
		return "Destination out of order";
	case 0x349C:
		return "Invalid number format";
	case 0x349D:
		return "Facility rejected";
	case 0x349E:
		return "Response to STATUS ENQUIRY";
	case 0x349F:
		return "Normal, unspecified";
	case 0x34A2:
		return "No circuit / channel available";
	case 0x34A6:
		return "Network out of order";
	case 0x34A9:
		return "Temporary failure";
	case 0x34AA:
		return "Switching equipment congestion";
	case 0x34AB:
		return "Access information discarded";
	case 0x34AC:
		return "Requested circuit / channel not available";
	case 0x34AF:
		return "Resources unavailable, unspecified";
	case 0x34B1:
		return "Quality of service unavailable";
	case 0x34B2:
		return "Requested facility not subscribed";
	case 0x34B9:
		return "Bearer capability not authorized";
	case 0x34BA:
		return "Bearer capability not presently available";
	case 0x34BF:
		return "Service or option not available, unspecified";
	case 0x34C1:
		return "Bearer capability not implemented";
	case 0x34C2:
		return "Channel type not implemented";
	case 0x34C5:
		return "Requested facility not implemented";
	case 0x34C6:
		return "Only restricted digital information bearer capability is available";
	case 0x34CF:
		return "Service or option not implemented, unspecified";
	case 0x34D1:
		return "Invalid call reference value";
	case 0x34D2:
		return "Identified channel does not exist";
	case 0x34D3:
		return "A suspended call exists, but this call identity does not";
	case 0x34D4:
		return "Call identity in use";
	case 0x34D5:
		return "No call suspended";
	case 0x34D6:
		return "Call having the requested call identity has been cleared";
	case 0x34D7:
		return "User not a member of CUG";
	case 0x34D8:
		return "Incompatible destination";
	case 0x34DB:
		return "Invalid transit network selection";
	case 0x34DF:
		return "Invalid message, unspecified";
	case 0x34E0:
		return "Mandatory information element is missing";
	case 0x34E1:
		return "Message type non-existent or not implemented";
	case 0x34E2:
		return "Message not compatible with call state or message type non-existent or not implemented";
	case 0x34E3:
		return "Information element non-existent or not implemented";
	case 0x34E4:
		return "Invalid information element contents";
	case 0x34E5:
		return "Message not compatible with call state";
	case 0x34E6:
		return "Recovery on timer expiry";
	case 0x34EF:
		return "Protocol error, unspecified";
	case 0x34FF:
		return "Interworking, unspecified";

	/* B3 protocol 7 (Modem) */
	case 0x3500:
		return "Normal end of connection";
	case 0x3501:
		return "Carrier lost";
	case 0x3502:
		return "Error on negotiation, i.e. no modem with error correction at other end";
	case 0x3503:
		return "No answer to protocol request";
	case 0x3504:
		return "Remote modem only works in synchronous mode";
	case 0x3505:
		return "Framing fails";
	case 0x3506:
		return "Protocol negotiation fails";
	case 0x3507:
		return "Other modem sends wrong protocol request";
	case 0x3508:
		return "Sync information (data or flags) missing";
	case 0x3509:
		return "Normal end of connection from the other modem";
	case 0x350a:
		return "No answer from other modem";
	case 0x350b:
		return "Protocol error";
	case 0x350c:
		return "Error on compression";
	case 0x350d:
		return "No connect (timeout or wrong modulation)";
	case 0x350e:
		return "No protocol fall-back allowed";
	case 0x350f:
		return "No modem or fax at requested number";
	case 0x3510:
		return "Handshake error";

	/* error info concerning the requested supplementary service */
	case 0x3600:
		return "Supplementary service not subscribed";
	case 0x3603:
		return "Supplementary service not available";
	case 0x3604:
		return "Supplementary service not implemented";
	case 0x3606:
		return "Invalid served user number";
	case 0x3607:
		return "Invalid call state";
	case 0x3608:
		return "Basic service not provided";
	case 0x3609:
		return "Supplementary service not requested for an incoming call";
	case 0x360a:
		return "Supplementary service interaction not allowed";
	case 0x360b:
		return "Resource unavailable";

	/* error info concerning the context of a supplementary service request */
	case 0x3700:
		return "Duplicate invocation";
	case 0x3701:
		return "Unrecognized operation";
	case 0x3702:
		return "Mistyped argument";
	case 0x3703:
		return "Resource limitation";
	case 0x3704:
		return "Initiator releasing";
	case 0x3705:
		return "Unrecognized linked-ID";
	case 0x3706:
		return "Linked response unexpected";
	case 0x3707:
		return "Unexpected child operation";

	/* Line Interconnect */
	case 0x3800:
		return "PLCI has no B-channel";
	case 0x3801:
		return "Lines not compatible";
	case 0x3802:
		return "PLCI(s) is (are) not in any or not in the same interconnection";

	default:
		return NULL;
	}
}

/*
 * show the text for a CAPI message info value
 */
void show_capi_info(struct capi_pvt *i, _cword info)
{
	char *p;
	char *name = "?";
	
	if (info == 0x0000) {
		/* no error, do nothing */
		return;
	}

	if (!(p = capi_info_string((unsigned int)info))) {
		/* message not available */
		return;
	}

	if (i)
		name = i->vname;
	
	cc_verbose(3, 0, VERBOSE_PREFIX_4 "%s: CAPI INFO 0x%04x: %s\n",
		name, info, p);
}

/*
 * send Listen to specified controller
 */
unsigned capi_ListenOnController(unsigned int CIPmask, unsigned controller)
{
	MESSAGE_EXCHANGE_ERROR error;
	int waitcount = 50;
	_cmsg CMSG;

	error = capi_sendf(NULL, 0, CAPI_LISTEN_REQ, controller, get_capi_MessageNumber(),
		"ddd()()",
		0x0000ffff,
		CIPmask,
		0
	);

	if (error)
		goto done;

	while (waitcount) {
		error = capidev_check_wait_get_cmsg(&CMSG);

		if (IS_LISTEN_CONF(&CMSG)) {
			error = LISTEN_CONF_INFO(&CMSG);
			ListenOnSupplementary(controller);
			break;
		}
		usleep(30000);
		waitcount--;
	}
	if (!waitcount)
		error = 0x100F;

 done:
	return error;
}

/*
 * Activate access to vendor specific extensions
 */
unsigned capi_ManufacturerAllowOnController(unsigned controller)
{
	MESSAGE_EXCHANGE_ERROR error;
	int waitcount = 50;
	unsigned char manbuf[CAPI_MANUFACTURER_LEN];
	_cmsg CMSG;

	if (capi20_get_manufacturer(controller, manbuf) == NULL) {
		error = CapiRegOSResourceErr;
		goto done;
	}
	if ((strstr((char *)manbuf, "Eicon") == 0) &&
	    (strstr((char *)manbuf, "Dialogic") == 0)) {
		error = 0x100F;
		goto done;
	}

	error = capi_sendf (NULL, 0, CAPI_MANUFACTURER_REQ, controller, get_capi_MessageNumber(),
		"dw(d)", _DI_MANU_ID, _DI_OPTIONS_REQUEST, 0x00000020L);

	if (error)
		goto done;

	while (waitcount) {
		error = capidev_check_wait_get_cmsg(&CMSG);

		if (IS_MANUFACTURER_CONF(&CMSG) && (CMSG.ManuID == _DI_MANU_ID) &&
			((CMSG.Class & 0xffff) == _DI_OPTIONS_REQUEST)) {
			error = (MESSAGE_EXCHANGE_ERROR)(CMSG.Class >> 16);
			break;
		}
		usleep(30000);
		waitcount--;
	}
	if (!waitcount)
		error = 0x100F;

done:
	return error;
}

/*
 * convert a number
 */
char *capi_number_func(unsigned char *data, unsigned int strip, char *buf)
{
	unsigned int len;

	if (data == NULL) {
		buf[0] = '\0';
		return buf;
	}

	if (data[0] == 0xff) {
		len = read_capi_word(&data[1]);
		data += 2;
	} else {
		len = data[0];
		data += 1;
	}
	if (len > (AST_MAX_EXTENSION - 1))
		len = (AST_MAX_EXTENSION - 1);
	
	/* convert a capi struct to a \0 terminated string */
	if ((!len) || (len < strip))
		return NULL;
		
	len = len - strip;
	data += strip;

	memcpy(buf, data, len);
	buf[len] = '\0';
	
	return buf;
}

/*
 * parse the dialstring
 */
void capi_parse_dialstring(char *buffer, char **interface, char **dest, char **param, char **ocid)
{
	int cp = 0;
	char *buffer_p = buffer;
	char *oc;

	/* interface is the first part of the string */
	*interface = buffer;

	*dest = emptyid;
	*param = emptyid;
	*ocid = NULL;

	while (*buffer_p) {
		if (*buffer_p == '/') {
			*buffer_p = 0;
			buffer_p++;
			if (cp == 0) {
				*dest = buffer_p;
				cp++;
			} else if (cp == 1) {
				*param = buffer_p;
				cp++;
			} else {
				cc_log(LOG_WARNING, "Too many parts in dialstring '%s'\n",
					buffer);
			}
			continue;
		}
		buffer_p++;
	}
	if ((oc = strchr(*dest, ':')) != NULL) {
		*ocid = *dest;
		*oc = '\0';
		*dest = oc + 1;
	}
	cc_verbose(3, 1, VERBOSE_PREFIX_4 "parsed dialstring: '%s' '%s' '%s' '%s'\n",
		*interface, (*ocid) ? *ocid : "NULL", *dest, *param);
	return;
}

/*
 * Add a new peer link id
 */
int cc_add_peer_link_id(struct ast_channel *c)
{
	int a;

	cc_mutex_lock(&peerlink_lock);
	for (a = 0; a < CAPI_MAX_PEERLINKCHANNELS; a++) {
		if (peerlinkchannel[a].channel == NULL) {
			peerlinkchannel[a].channel = c;
			peerlinkchannel[a].age = time(NULL);
			break;
		} else {
			/* remove too old entries */
			if ((peerlinkchannel[a].age + 60) < time(NULL)) {
				peerlinkchannel[a].channel = NULL;
				cc_verbose(3, 1, VERBOSE_PREFIX_4 CC_MESSAGE_NAME
					": peerlink %d timeout-erase\n", a);
			}
		}
	}
	cc_mutex_unlock(&peerlink_lock);
	if (a == CAPI_MAX_PEERLINKCHANNELS) {
		return -1;
	}
	return a;
}

/*
 * Get and remove peer link id
 */
struct ast_channel *cc_get_peer_link_id(const char *p)
{
	int id = -1;
	struct ast_channel *chan = NULL;

	if (p) {
		id = (int)strtol(p, NULL, 0);
	}

	cc_mutex_lock(&peerlink_lock);
	if ((id >= 0) && (id < CAPI_MAX_PEERLINKCHANNELS)) {
		chan = peerlinkchannel[id].channel;
		peerlinkchannel[id].channel = NULL;
	}
	cc_verbose(3, 1, VERBOSE_PREFIX_4 CC_MESSAGE_NAME
		": peerlink %d allocated, peer is %s\n", id, (chan)?chan->name:"unlinked");
	cc_mutex_unlock(&peerlink_lock);
	return chan;
}

/*
 * create pipe for interface connection
 */
int capi_create_reader_writer_pipe(struct capi_pvt *i)
{
	int fds[2];
	int flags;

	if (pipe(fds) != 0) {
		cc_log(LOG_ERROR, "%s: unable to create pipe.\n",
			i->vname);
		return 0;
	}
	i->readerfd = fds[0];
	i->writerfd = fds[1];
	flags = fcntl(i->readerfd, F_GETFL);
	fcntl(i->readerfd, F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(i->writerfd, F_GETFL);
	fcntl(i->writerfd, F_SETFL, flags | O_NONBLOCK);

	return 1;
}

/*
 * read a frame from the pipe
 */
struct ast_frame *capi_read_pipeframe(struct capi_pvt *i)
{
	struct ast_frame *f;
	int readsize;

	if (i == NULL) {
		cc_log(LOG_ERROR, "channel has no interface\n");
		return NULL;
	}
	if (i->readerfd == -1) {
		cc_log(LOG_ERROR, "no readerfd\n");
		return NULL;
	}

	f = &i->f;
	f->frametype = AST_FRAME_NULL;
	FRAME_SUBCLASS_INTEGER(f->subclass) = 0;

	readsize = read(i->readerfd, f, sizeof(struct ast_frame));
	if ((readsize != sizeof(struct ast_frame)) && (readsize > 0)) {
		cc_log(LOG_ERROR, "did not read a whole frame (len=%d, err=%d)\n",
			readsize, errno);
	}
	
	f->mallocd = 0;
	f->FRAME_DATA_PTR = NULL;

	if ((f->frametype == AST_FRAME_CONTROL) &&
		(FRAME_SUBCLASS_INTEGER(f->subclass) == AST_CONTROL_HANGUP)) {
		return NULL;
	}

	if ((f->frametype == AST_FRAME_VOICE) && (f->datalen > 0)) {
		if (f->datalen > sizeof(i->frame_data)) {
			cc_log(LOG_ERROR, "f.datalen(%d) greater than space of frame_data(%d)\n",
				f->datalen, (int)sizeof(i->frame_data));
			f->datalen = sizeof(i->frame_data);
		}
		readsize = read(i->readerfd, i->frame_data + AST_FRIENDLY_OFFSET, f->datalen);
		if (readsize != f->datalen) {
			cc_log(LOG_ERROR, "did not read whole frame data\n");
		}
		f->FRAME_DATA_PTR = i->frame_data + AST_FRIENDLY_OFFSET;
	}
	return f;
}

/*
 * write for a channel
 */
int capi_write_frame(struct capi_pvt *i, struct ast_frame *f)
{
	MESSAGE_EXCHANGE_ERROR error;
	int j = 0;
	unsigned char *buf;
	struct ast_frame *fsmooth;
	int txavg=0;
	int ret = 0;
	int B3Blocks = 1;

	if (unlikely(!i)) {
		cc_log(LOG_ERROR, "channel has no interface\n");
		return -1;
	}

	{
		struct capi_pvt *ii = i;

		cc_mutex_lock(&ii->lock);

		if (i->line_plci != 0)
			i = i->line_plci;

		cc_mutex_unlock(&ii->lock);
	}
	 
	if (unlikely((!(i->isdnstate & CAPI_ISDN_STATE_B3_UP)) || (!i->NCCI) ||
	    ((i->isdnstate & (CAPI_ISDN_STATE_B3_CHANGE | CAPI_ISDN_STATE_LI))))) {
		return 0;
	}

	if ((!(i->ntmode)) && (i->state != CAPI_STATE_CONNECTED)) {
		return 0;
	}

	if (unlikely(f->frametype == AST_FRAME_NULL)) {
		return 0;
	}
	if (unlikely((!f->FRAME_DATA_PTR) || (!f->datalen))) {
		/* prodding from Asterisk, just returning */
		return 0;
	}
	if (unlikely(f->frametype == AST_FRAME_DTMF)) {
		cc_log(LOG_ERROR, "dtmf frame should be written\n");
		return 0;
	}
	if (unlikely(f->frametype != AST_FRAME_VOICE)) {
		cc_log(LOG_ERROR,"not a voice frame\n");
		return 0;
	}
	if (unlikely(i->FaxState & CAPI_FAX_STATE_ACTIVE)) {
		cc_verbose(3, 1, VERBOSE_PREFIX_2 "%s: write on fax activity?\n",
			i->vname);
		return 0;
	}
	if (i->isdnstate & CAPI_ISDN_STATE_RTP) {
		if (unlikely((!(GET_FRAME_SUBCLASS_CODEC(f->subclass) & i->codec)) &&
		    (GET_FRAME_SUBCLASS_CODEC(f->subclass) != capi_capability))) {
			cc_log(LOG_ERROR, "don't know how to write subclass %s(%d)\n",
				cc_getformatname(GET_FRAME_SUBCLASS_CODEC(f->subclass)),
					FRAME_SUBCLASS_INTEGER(f->subclass));
			return 0;
		}
		return capi_write_rtp(i, f);
	}

	if (unlikely(i->B3count >= CAPI_MAX_B3_BLOCKS)) {
		cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: B3count is full, dropping packet.\n",
			i->vname);
		return 0;
	}

	if (i->bproto == CC_BPROTO_VOCODER || (i->line_plci != 0 && i->line_plci->bproto == CC_BPROTO_VOCODER)) {
#ifdef DIVA_STREAMING
		capi_DivaStreamLock();
		if (i->diva_stream_entry != 0) {
			int written = 0, ready = 0;

			B3Blocks = 0;
			if ((ready = (i->diva_stream_entry->diva_stream_state == DivaStreamActive)) &&
					(i->diva_stream_entry->diva_stream->get_tx_free (i->diva_stream_entry->diva_stream) > 2*CAPI_MAX_B3_BLOCK_SIZE+128)) {
				written = i->diva_stream_entry->diva_stream->write (i->diva_stream_entry->diva_stream, 8U << 8 | DIVA_STREAM_MESSAGE_TX_IDI_REQUEST, f->FRAME_DATA_PTR, f->datalen);
				i->diva_stream_entry->diva_stream->flush_stream(i->diva_stream_entry->diva_stream);
			}
			capi_DivaStreamUnLock ();

			error = written != f->datalen;
			if (unlikely(error != 0)) {
				cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: stream is %s, dropping packet.\n", i->vname, (ready != 0) ? "full" : "not ready");
			}
		} else
#endif
		{
#ifdef DIVA_STREAMING
			capi_DivaStreamUnLock ();
#endif
			buf = &(i->send_buffer[(i->send_buffer_handle % CAPI_MAX_B3_BLOCKS) *
				(CAPI_MAX_B3_BLOCK_SIZE + AST_FRIENDLY_OFFSET)]);
			i->send_buffer_handle++;

			memcpy (buf, f->FRAME_DATA_PTR, f->datalen);

			error = capi_sendf(NULL, 0, CAPI_DATA_B3_REQ, i->NCCI, get_capi_MessageNumber(),
				"dwww", buf, f->datalen, i->send_buffer_handle, 0);
		}
		if (likely(error == 0)) {
			cc_mutex_lock(&i->lock);
			i->B3count += B3Blocks;
			i->B3q -= f->datalen;
			if (i->B3q < 0)
				i->B3q = 0;
			cc_mutex_unlock(&i->lock);
		}

		return 0;
	}

	if ((!i->smoother) || (ast_smoother_feed(i->smoother, f) != 0)) {
		cc_log(LOG_ERROR, "%s: failed to fill smoother\n", i->vname);
		return 0;
	}

	for (fsmooth = ast_smoother_read(i->smoother);
	     fsmooth != NULL;
	     fsmooth = ast_smoother_read(i->smoother)) {
		buf = &(i->send_buffer[(i->send_buffer_handle % CAPI_MAX_B3_BLOCKS) *
			(CAPI_MAX_B3_BLOCK_SIZE + AST_FRIENDLY_OFFSET)]);
		i->send_buffer_handle++;

		if ((i->doES == 1) && (!capi_tcap_is_digital(i->transfercapability))) {
			for (j = 0; j < fsmooth->datalen; j++) {
				buf[j] = capi_reversebits[ ((unsigned char *)fsmooth->FRAME_DATA_PTR)[j] ]; 
				if (capi_capability == AST_FORMAT_ULAW) {
					txavg += abs( capiULAW2INT[capi_reversebits[ ((unsigned char*)fsmooth->FRAME_DATA_PTR)[j]]] );
				} else {
					txavg += abs( capiALAW2INT[capi_reversebits[ ((unsigned char*)fsmooth->FRAME_DATA_PTR)[j]]] );
				}
			}
			txavg = txavg / j;
			for(j = 0; j < ECHO_TX_COUNT - 1; j++) {
				i->txavg[j] = i->txavg[j+1];
			}
			i->txavg[ECHO_TX_COUNT - 1] = txavg;
		} else {
			if ((i->txgain == 1.0) || (capi_tcap_is_digital(i->transfercapability))) {
				for (j = 0; j < fsmooth->datalen; j++) {
					buf[j] = capi_reversebits[((unsigned char *)fsmooth->FRAME_DATA_PTR)[j]];
				}
			} else {
				for (j = 0; j < fsmooth->datalen; j++) {
					buf[j] = i->g.txgains[capi_reversebits[((unsigned char *)fsmooth->FRAME_DATA_PTR)[j]]];
				}
			}
		}
   
		error = 1; 
		if (i->B3q > 0) {
#if defined(DIVA_STREAMING)
			if (i->diva_stream_entry != 0) {
				int written = 0, ready = 0;

				B3Blocks = 0;
				capi_DivaStreamLock();
				if ((ready = (i->diva_stream_entry->diva_stream_state == DivaStreamActive)) &&
						(i->diva_stream_entry->diva_stream->get_tx_free (i->diva_stream_entry->diva_stream) > 2*CAPI_MAX_B3_BLOCK_SIZE+128)) {
					written = i->diva_stream_entry->diva_stream->write (i->diva_stream_entry->diva_stream, 8U << 8 | DIVA_STREAM_MESSAGE_TX_IDI_REQUEST, buf, fsmooth->datalen);
					i->diva_stream_entry->diva_stream->flush_stream(i->diva_stream_entry->diva_stream);
				}
				capi_DivaStreamUnLock ();

				error = written != fsmooth->datalen;
				if (unlikely(error != 0)) {
					cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: stream is %s, dropping packet.\n", i->vname, (ready != 0) ? "full" : "not ready");
				}
			} else
#endif
			{
				error = capi_sendf(NULL, 0, CAPI_DATA_B3_REQ, i->NCCI, get_capi_MessageNumber(),
					"dwww", buf, fsmooth->datalen, i->send_buffer_handle, 0);
			}
		} else {
			cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: too much voice to send for NCCI=%#x\n",
				i->vname, i->NCCI);
		}

		if (likely(!error)) {
			cc_mutex_lock(&i->lock);
			i->B3count += B3Blocks;
			i->B3q -= fsmooth->datalen;
			if (i->B3q < 0)
				i->B3q = 0;
			cc_mutex_unlock(&i->lock);
		}
	}
	return ret;
}

/*
	 ast_channel_lock(chan) to be held while
	 while accessing returned pointer
	*/
const char* pbx_capi_get_cid(const struct ast_channel* c, const char* notAvailableVisual)
{
	const char* cid;

#ifdef CC_AST_HAS_VERSION_1_8
	cid = S_COR(c->caller.id.number.valid, c->caller.id.number.str, notAvailableVisual);
#else
	cid = c->cid.cid_num;
#endif

	return (cid);
}

/*
	 ast_channel_lock(chan) to be held while
	 while accessing returned pointer
	*/
const char* pbx_capi_get_callername(const struct ast_channel* c, const char* notAvailableVisual)
{
	const char* name;

#ifdef CC_AST_HAS_VERSION_1_8
	name = S_COR(c->caller.id.name.valid, c->caller.id.name.str, notAvailableVisual);
#else
	name = (c->cid.cid_name) ? c->cid.cid_name : notAvailableVisual;
#endif

	return (name);
}

/*
	 ast_channel_lock(chan) to be held while
	 while accessing returned pointer
	*/
const char* pbx_capi_get_connectedname(const struct ast_channel* c, const char* notAvailableVisual)
{
	const char* name;

#ifdef CC_AST_HAS_VERSION_1_8
	name = S_COR(c->connected.id.name.valid, c->connected.id.name.str, notAvailableVisual);
#else
	name = (c->cid.cid_name) ? c->cid.cid_name : notAvailableVisual;
#endif

	return (name);
}

const struct capi_pvt *pbx_capi_get_nulliflist(void)
{
	return nulliflist;
}

void pbx_capi_nulliflist_lock(void)
{
	cc_mutex_lock(&nullif_lock);
}

void pbx_capi_nulliflist_unlock(void)
{
	cc_mutex_unlock(&nullif_lock);
}

/*!
		\brief get list of controllers. Stop parsing
						after non digit detected after separator
						character or end of string is reached
	*/
char* pbx_capi_strsep_controller_list (char** param)
{
	char *src, *p;

	if ((param == NULL) || (*param == NULL) || (**param == 0))
		return NULL;

	if (strchr(*param, '|') != NULL)
		return (strsep(param, "|"));

	src = *param;
	p = src - 1;
	do {
		p = strchr(p+1, ',');
	} while ((p != NULL) && (isdigit(p[1]) != 0));

	if (p != NULL)
		*p++ = 0;

	*param = p;

	return src;
}
