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
 * \brief Declares interface to diva trace system
 */
#ifndef __DIVA_VERBOSER_H__
#define __DIVA_VERBOSER_H__

#ifdef DIVA_VERBOSE

void diva_verbose_load(void);
void diva_verbose_unload(void);

#ifdef CC_AST_HAS_VERSION_1_6
char *pbxcli_capi_do_verbose(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
char *pbxcli_capi_no_verbose(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
#else
int pbxcli_capi_do_verbose(int fd, int argc, char *argv[]);
int pbxcli_capi_no_verbose(int fd, int argc, char *argv[]);
#endif

#define CC_CLI_TEXT_CAPI_DO_VERBOSE "Connect " CC_MESSAGE_BIGNAME " to debug driver"
#define CC_CLI_TEXT_CAPI_NO_VERBOSE "Disconnect " CC_MESSAGE_BIGNAME " from debug driver"


#ifndef CC_AST_HAS_VERSION_1_6
extern char capi_do_verbose_usage[];
extern char capi_no_verbose_usage[];
#endif

#else

#define diva_verbose_load() do{}while(0)
#define diva_verbose_unload() do{}while(0)

#endif

#endif

