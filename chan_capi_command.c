/*
 *
  Copyright (c) Dialogic, 2009.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
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
#include <sys/time.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <ctype.h>

#include "chan_capi_platform.h"
#include "xlaw.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_rtp.h"
#include "chan_capi_qsig.h"
#include "chan_capi_qsig_ecma.h"
#include "chan_capi_qsig_asn197ade.h"
#include "chan_capi_qsig_asn197no.h"
#include "chan_capi_utils.h"
#include "chan_capi_supplementary.h"
#include "chan_capi_chat.h"
#include "chan_capi_command.h"

typedef struct _pbx_capi_voice_command {
	diva_entity_link_t link;
	pbx_capi_command_proc_t pbx_capi_command;
	char channel_command_digits[AST_MAX_EXTENSION+1];
	int length; /* channel_command_digits length */
	char command_name[64];
	char command_parameters[128];
} pbx_capi_voice_command_t;

/*
 * LOCALS
 */
static const char* pbx_capi_voicecommand_digits = "1234567890ABCD*#";
static int pbx_capi_command_nop(struct ast_channel *c, char *param);
static pbx_capi_voice_command_t* pbx_capi_find_command(struct capi_pvt *i, const char* name);
static pbx_capi_voice_command_t* pbx_capi_find_command_by_key(struct capi_pvt *i, const char* key);
static pbx_capi_voice_command_t* pbx_capi_voicecommand_find_digit_command(diva_entity_queue_t* q, const char* digits, int length, int* info);
static void pbx_capi_voicecommand_insert_command(diva_entity_queue_t* q, pbx_capi_voice_command_t* cmd);


/*
 * voicecommand|key|param1|param2|...
 *
 */
int pbx_capi_voicecommand(struct ast_channel *c, char *param)
{
	struct capi_pvt *i;
	pbx_capi_voice_command_t* cmd;
	const char* command[2];
	const char* key[2];
	size_t length;

	if (c->tech == &capi_tech) {
		i = CC_CHANNEL_PVT(c);
	} else {
		i = pbx_check_resource_plci(c);
	}
	if (i == 0) {
		/*
			Ignore command silently to ensure same context can be used to process
			all types of calls or in case of fallback to NULL PLCI
			*/
		return 0;
	}


	if ((param == NULL) || (*param == 0)) { /* Remove all voice commands */
		cc_mutex_lock(&i->lock);
		pbx_capi_voicecommand_cleanup(i);
		cc_mutex_unlock(&i->lock);
		return 0;
	}

	command[0] = param;
	command[1] = strchr(command[0], '|');

	if (command[1] == 0) {
		/*
		 * Remove command
		 */
		cc_mutex_lock(&i->lock);
		while ((cmd = pbx_capi_find_command(i, command[0])) != 0) {
			cc_verbose(2, 0, VERBOSE_PREFIX_4"%s: voicecommand:%s removed\n",
				i->vname, cmd->command_name);
			diva_q_remove(&i->channel_command_q, &cmd->link);
			ast_free (cmd);
		}
		cc_mutex_unlock(&i->lock);
	} else {
		if ((command[1] - command[0]) < 2 || (command[1] - command[0]) >= sizeof(cmd->command_name) ||
				strchr(pbx_capi_voicecommand_digits, command[1][1]) == 0) {
			cc_log(LOG_WARNING, CC_MESSAGE_NAME" voicecommand requires an argument im format 'voicecommand[|key[|param1|param2|...]]'\n");
			return -1;
		}
		key[0] = &command[1][1];
		key[1] = strchr (key[0], '|');
    length = 0;
		if ((key[1] == 0 && strlen(key[0]) >= sizeof(cmd->channel_command_digits)) ||
				(key[1] != 0 && ((key[1] - key[0]) == 0 || (key[1] - key[0]) >= sizeof(cmd->channel_command_digits) ||
				key[1][1] == 0 || (length = strlen (&key[1][1])) >= sizeof(cmd->command_parameters)))) {

			cc_log(LOG_WARNING, CC_MESSAGE_NAME
				" voicecommand requires an argument im format 'voicecommand[|key[|param1|param2|...]]'\n");
			return -1;
		}
		if (key[1] == 0) {
			key[1] = key[0] + strlen(key[0]);
			length = 0;
		}

		{
			const char* p = key[0];

			for (p = key[0]; p < key[1]; p++) {
				if (strchr(pbx_capi_voicecommand_digits, *p) == 0) {
					cc_log(LOG_WARNING, CC_MESSAGE_NAME
						" voicecommand key can use only '%s'\n", pbx_capi_voicecommand_digits);
					return -1;
				}
			}
		}

		cmd = ast_malloc(sizeof(*cmd));
		if (cmd == NULL) {
			cc_log(LOG_WARNING, CC_MESSAGE_NAME " can not allocate memory for voice command\n");
			return -1;
		}

		memcpy (cmd->command_parameters, &key[1][1], length);
		cmd->command_parameters[length] = 0;

		length = command[1] - command[0];
		memcpy (cmd->command_name, command[0], length);
		cmd->command_name[length] = 0;

		length = key[1] - key[0];
		memcpy (cmd->channel_command_digits, key[0], length);
		cmd->channel_command_digits[length] = 0;
		cmd->length = length;

		cmd->pbx_capi_command = pbx_capi_lockup_command_by_name(cmd->command_name);
		if ( cmd->pbx_capi_command == 0) {
			cmd->pbx_capi_command = pbx_capi_command_nop; /* accept unknown commands for compatibility reason */
		}


		cc_verbose(2, 0, VERBOSE_PREFIX_4 "%s: %svoicecommand:%s|%s|%s\n",
			i->vname, (cmd->pbx_capi_command == pbx_capi_command_nop) ? "dummy " : "",
			cmd->command_name, cmd->channel_command_digits, cmd->command_parameters);

		{
			pbx_capi_voice_command_t* present_cmd;

			cc_mutex_lock(&i->lock);
			if ((present_cmd = pbx_capi_find_command_by_key(i, cmd->command_name)) != 0) {
				diva_q_remove(&i->channel_command_q, &present_cmd->link);
			}
			pbx_capi_voicecommand_insert_command(&i->channel_command_q, cmd);
			cc_mutex_unlock(&i->lock);

			if (present_cmd != 0) {
				ast_free (present_cmd);
			}
		}
	}

	return 0;
}

int pbx_capi_voicecommand_transparency(struct ast_channel *c, char *param)
{
	struct capi_pvt *i;

	if (c->tech == &capi_tech) {
		i = CC_CHANNEL_PVT(c);
	} else {
		i = pbx_check_resource_plci(c);
	}
	if (i == 0) {
		/*
			Ignore command silently to ensure same context can be used to process
			all types of calls or in case of fallback to NULL PLCI
			*/
		return 0;
	}

	if ((param == NULL) || (*param == 0)) {
		cc_log(LOG_WARNING, "Parameter for voicecommand transparency missing.\n");
		return -1;
	}

	if (ast_true(param)) {
		i->command_pass_digits = 1;
	} else if (ast_false(param)) {
		i->command_pass_digits = 0;
	} else {
		cc_log(LOG_WARNING, "Wrong parameter for voicecommand transparency.\n");
		return -1;
	}

	return 0;
}

int pbx_capi_voicecommand_cleanup(struct capi_pvt *i)
{
	diva_entity_link_t* link;

	while ((link = diva_q_get_head(&i->channel_command_q)) != NULL) {
		diva_q_remove(&i->channel_command_q, link);
		ast_free(link);
	}

	i->channel_command_digit      = 0;
	i->channel_command_timestamp  = 0;
	i->command_pass_digits        = 0;

	return 0;
}

static pbx_capi_voice_command_t* pbx_capi_find_command(struct capi_pvt *i, const char* name)
{
	diva_entity_link_t* link;

	for (link = diva_q_get_head (&i->channel_command_q); link != 0; link = diva_q_get_next(link)) {
		if (strcmp(((pbx_capi_voice_command_t*)link)->command_name, name) == 0) {
			return ((pbx_capi_voice_command_t*)link);
		}
	}

	return 0;
}

static pbx_capi_voice_command_t* pbx_capi_find_command_by_key(struct capi_pvt *i, const char* key)
{
	diva_entity_link_t* link;

	for (link = diva_q_get_head (&i->channel_command_q); link != 0; link = diva_q_get_next(link)) {
		if (strcmp (((pbx_capi_voice_command_t*)link)->channel_command_digits, key) == 0) {
			return ((pbx_capi_voice_command_t*)link);
		}
	}

	return 0;
}

/*
 * Process received DTMF digit
 *
 * returs zero if digit should be processed as usually
 * returns -1 if digit should be discarded
 */
int pbx_capi_voicecommand_process_digit(struct capi_pvt *i, struct ast_channel *owner, char digit)
{
	struct ast_channel *c = (owner == 0) ? i->owner : owner;
	pbx_capi_voice_command_t* cmd;
	int info;
	time_t t;

	/*
    Simple algorithm due to low amount of entries, moreover all sequences will be short, only 1 ... 2 digits
    */
	if ((c == NULL) || (diva_q_get_head(&i->channel_command_q) == 0) ||
			(strchr(pbx_capi_voicecommand_digits, digit) == 0)) {
		i->channel_command_digit = 0;
		return 0;
	}

	t = time(0);
	if (((i->channel_command_digit != 0) && (difftime(t, i->channel_command_timestamp) > 2)) ||
			(i->channel_command_digit >= (sizeof(i->channel_command_digits) - 1))) {
		i->channel_command_digit = 0;
	}

	i->channel_command_timestamp = t;

	i->channel_command_digits[i->channel_command_digit++] = digit;
	i->channel_command_digits[i->channel_command_digit]   = 0;
	cmd = pbx_capi_voicecommand_find_digit_command(
		&i->channel_command_q,
		i->channel_command_digits,
		i->channel_command_digit,
		&info);

	if (cmd != 0) {
		char command_parameters_copy[sizeof( cmd->command_parameters)];

		i->channel_command_digit = 0;

		cc_verbose(2, 0, VERBOSE_PREFIX_4 "%s: call voicecommand:%s|%s|%s\n",
			i->vname, cmd->command_name, cmd->channel_command_digits, cmd->command_parameters);

		strcpy (command_parameters_copy, cmd->command_parameters);
		info = ((*(cmd->pbx_capi_command))(c, command_parameters_copy));

		cc_verbose(2, 0, VERBOSE_PREFIX_4 "%s: voicecommand:%s|%s|%s %s\n",
			i->vname, cmd->command_name, cmd->channel_command_digits, cmd->command_parameters, info == 0 ? "OK" : "ERROR");

	} else if (info == 0) {
		i->channel_command_digit = 0;
		return 0;
	}

	return ((i->command_pass_digits != 0) ? 0 : -1);
}

static pbx_capi_voice_command_t* pbx_capi_voicecommand_find_digit_command(
	diva_entity_queue_t* q,
	const char* digits,
	int length,
	int* info)
{
	diva_entity_link_t* link;

	for (*info = 0, link = diva_q_get_head(q);
			 link != 0 && length <= ((pbx_capi_voice_command_t*)link)->length;
			 link = diva_q_get_next (link)) {
		pbx_capi_voice_command_t* cmd = (pbx_capi_voice_command_t*)link;

		if (memcmp(digits, cmd->channel_command_digits, length) == 0) {
			*info = 1;
			if (length == cmd->length) {
				return cmd;
			}
		}
  }

  return 0;
}

static int pbx_capi_command_nop(struct ast_channel *c, char *param)
{
	return 0;
}

static void pbx_capi_voicecommand_insert_command(diva_entity_queue_t* q, pbx_capi_voice_command_t* cmd)
{
	diva_entity_link_t* link;

	for (link = diva_q_get_head (q); link != 0; link = diva_q_get_next (link)) {
		if (((pbx_capi_voice_command_t*)link)->length <= cmd->length) {
			diva_q_insert_before (q, link, &cmd->link);
			return;
		}
	}

	diva_q_add_tail(q, &cmd->link);
}


/*
 * vim:ts=2
 */
