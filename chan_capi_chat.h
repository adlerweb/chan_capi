/*
 * An implementation of Common ISDN API 2.0 for Asterisk
 *
 * Copyright (C) 2006-2009 Cytronics & Melware
 *
 * Armin Schindler <armin@melware.de>
 * 
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 */
 
#ifndef _PBX_CAPI_CHAT_H
#define _PBX_CAPI_CHAT_H

/*
 * prototypes
 */
extern void pbx_capi_chat_init_module(void);
extern int pbx_capi_chat(struct ast_channel *c, char *param);
extern int pbx_capi_chat_associate_resource_plci(struct ast_channel *c, char *param);
extern struct capi_pvt* pbx_check_resource_plci(struct ast_channel *c);
#ifdef CC_AST_HAS_VERSION_1_6
extern char *pbxcli_capi_chatinfo(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
#else
extern int pbxcli_capi_chatinfo(int fd, int argc, char *argv[]);
#endif
extern int pbx_capi_chat_command (struct ast_channel *c, char *param);
extern int pbx_capi_chat_mute(struct ast_channel *c, char *param);
extern int pbx_capi_chat_play(struct ast_channel *c, char *param);
extern int pbx_capi_chat_connect(struct ast_channel *c, char *param);
int pbx_capi_chat_remove_user(const char* room, const char* name);

struct capichat_s;
const struct capichat_s *pbx_capi_chat_get_room_c(const struct capichat_s * room);
const char* pbx_capi_chat_get_room_name(const struct capichat_s * room);
unsigned int pbx_capi_chat_get_room_number(const struct capichat_s * room);
unsigned int pbx_capi_chat_get_room_members(const struct capichat_s * room);
struct ast_channel *pbx_capi_chat_get_room_channel(const struct capichat_s * room);
const struct capi_pvt* pbx_capi_chat_get_room_interface_c(const struct capichat_s * room);
int pbx_capi_chat_is_member_operator(const struct capichat_s * room);
int pbx_capi_chat_is_room_muted(const struct capichat_s * room);
int pbx_capi_chat_is_member_muted(const struct capichat_s * room);
int pbx_capi_chat_is_member_listener(const struct capichat_s * room);
int pbx_capi_chat_is_most_recent_user(const struct capichat_s * room);
unsigned int pbx_capi_chat_get_room_group (const struct capichat_s * room);
unsigned int pbx_capi_chat_get_room_group_members (const struct capichat_s * room);

void pbx_capi_lock_chat_rooms(void);
void pbx_capi_unlock_chat_rooms(void);

#endif
