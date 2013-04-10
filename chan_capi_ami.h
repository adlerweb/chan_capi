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
#ifndef __CC_AMI_INTERFACE_H__
#define __CC_AMI_INTERFACE_H__

void pbx_capi_ami_register(void);
void pbx_capi_ami_unregister(void);
struct capichat_s;
void pbx_capi_chat_join_event(struct ast_channel* c, const struct capichat_s * room);
void pbx_capi_chat_leave_event(struct ast_channel* c,
															 const struct capichat_s *room,
															 long duration);
void pbx_capi_chat_conference_end_event(const char* roomName);


#endif

