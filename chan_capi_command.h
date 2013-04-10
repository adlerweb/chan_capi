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
#ifndef __CHAN_CAPI_COMMAND_H__
#define __CHAN_CAPI_COMMAND_H__

int pbx_capi_voicecommand (struct ast_channel *c, char *param);
int pbx_capi_voicecommand_transparency (struct ast_channel *c, char *param);
int pbx_capi_voicecommand_process_digit (struct capi_pvt *i, struct ast_channel *owner, char digit);
int pbx_capi_voicecommand_cleanup (struct capi_pvt *i);

#endif

