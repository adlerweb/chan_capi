/*
 *
  Copyright (c) Dialogic(R), 2011

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
 * \brief Implements interface to diva trace system
 */
#include <stdio.h>
#include "chan_capi20.h"
#include "chan_capi.h"

#ifdef CC_AST_HAS_VERSION_1_4
#include <asterisk.h>
#endif

#include <asterisk/logger.h>
#include <asterisk/module.h>
#include <asterisk/cli.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "divaverbose.h"

static int verbose_registered;
AST_MUTEX_DEFINE_STATIC(dbgIfcLock);
static volatile int divaDbgIfc = -1;

#ifdef CC_AST_HAS_VERSION_1_6
static
#endif
char capi_do_verbose_usage[] =
"Usage: " CC_MESSAGE_NAME " verbose\n"
"       Connect to debug driver.\n";

#ifdef CC_AST_HAS_VERSION_1_6
static
#endif
char capi_no_verbose_usage[] =
"Usage: " CC_MESSAGE_NAME " no verbose\n"
"       Disconnect from debug driver.\n";

static void open_diva_mnt(void)
{
	cc_mutex_lock(&dbgIfcLock);
	if (divaDbgIfc < 0) {
		divaDbgIfc = open("/dev/DivasDBGIFC", O_WRONLY);
	}
	cc_mutex_unlock(&dbgIfcLock);
}

#if (!defined(CC_AST_HAS_VERSION_1_4) && !defined(CC_AST_HAS_VERSION_1_6) && !defined(CC_AST_HAS_VERSION_1_8))
static void diva_verbose_write (const char *info, int opos, int replacelast, int complete)
#else
static void diva_verbose_write (const char *info)
#endif
{
	if (divaDbgIfc < 0) {
		open_diva_mnt();
	}
	if (divaDbgIfc >= 0) {
		write (divaDbgIfc, info, strlen(info));
	}
}

static int diva_verbose_start(void)
{
	if (verbose_registered == 0) {
		verbose_registered = (ast_register_verbose(diva_verbose_write) == 0);
	}

	return ((verbose_registered != 0) ? 0 : -1);
}

static int diva_verbose_stop(void)
{
	if (verbose_registered != 0) {
		verbose_registered = !(ast_unregister_verbose(diva_verbose_write) == 0);
	}
	if ((verbose_registered == 0) && (divaDbgIfc >= 0)) {
		close(divaDbgIfc);
		divaDbgIfc = -1;
	}

	return ((verbose_registered == 0) ? 0 : -1);
}

/*
 * enable debugging
 */
#ifdef CC_AST_HAS_VERSION_1_6
char *pbxcli_capi_do_verbose(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
#else
int pbxcli_capi_do_verbose(int fd, int argc, char *argv[])
#endif
{
	int ret;
#ifdef CC_AST_HAS_VERSION_1_6
	int fd = a->fd;

	if (cmd == CLI_INIT) {
		e->command = CC_MESSAGE_NAME " verbose";
		e->usage = capi_do_verbose_usage;
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;
	if (a->argc != e->args)
		return CLI_SHOWUSAGE;
#else
	if (argc != 2)
		return RESULT_SHOWUSAGE;
#endif
		
	ret = diva_verbose_start();
	if (ret == 0) {
		ast_cli(fd, CC_MESSAGE_BIGNAME " Message Verboser Enabled\n");
	} else {
		ast_cli(fd, CC_MESSAGE_BIGNAME " Failed to enable Message Verboser\n");
	}

#ifdef CC_AST_HAS_VERSION_1_6
	return ((ret == 0) ? CLI_SUCCESS : CLI_FAILURE);
#else
	return ((ret == 0) ? RESULT_SUCCESS : RESULT_FAILURE);
#endif
}

/*
 * disable debugging
 */
#ifdef CC_AST_HAS_VERSION_1_6
char *pbxcli_capi_no_verbose(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
#else
int pbxcli_capi_no_verbose(int fd, int argc, char *argv[])
#endif
{
	int ret;
#ifdef CC_AST_HAS_VERSION_1_6
	int fd = a->fd;

	if (cmd == CLI_INIT) {
		e->command = CC_MESSAGE_NAME " no verbose";
		e->usage = capi_no_verbose_usage;
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;
	if (a->argc != e->args)
		return CLI_SHOWUSAGE;
#else
	if (argc != 3)
		return RESULT_SHOWUSAGE;
#endif


	ret = diva_verbose_stop();
	if (ret == 0) {
		ast_cli(fd, CC_MESSAGE_BIGNAME " Message Verboser Disabled\n");
	} else {
		ast_cli(fd, CC_MESSAGE_BIGNAME " Failed to disable Message Verboser\n");
	}

#ifdef CC_AST_HAS_VERSION_1_6
	return ((ret == 0) ? CLI_SUCCESS : CLI_FAILURE);
#else
	return ((ret == 0) ? RESULT_SUCCESS : RESULT_FAILURE);
#endif
}


void diva_verbose_load(void)
{
	divaDbgIfc = -1;
	open_diva_mnt();
	if (divaDbgIfc >= 0) {
		diva_verbose_start();
	}
}

void diva_verbose_unload(void)
{
	if (verbose_registered != 0) {
		verbose_registered = !(ast_unregister_verbose(diva_verbose_write) == 0);
	}
	if (verbose_registered != 0)
		cc_mutex_lock(&dbgIfcLock);
	if (divaDbgIfc >= 0) {
		close(divaDbgIfc);
		divaDbgIfc = -1;
	}
	if (verbose_registered != 0)
		cc_mutex_unlock(&dbgIfcLock);
}
