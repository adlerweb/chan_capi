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
 
#ifndef _PBX_CAPI_UTILS_H
#define _PBX_CAPI_UTILS_H

/*
 * prototypes
 */
extern int capidebug;
extern char *emptyid;

extern void cc_verbose_internal(char *text, ...);

static inline int cc_verbose_check(int o_v, int c_d)
{
	if (unlikely(((o_v == 0) || (option_verbose > o_v)) &&
		((!c_d) || ((c_d) && (capidebug))))) {
		return 1;
	}

	return 0;
}

/*
 * helper for <pbx>_verbose with different verbose settings
 */
#define cc_verbose(o_v,c_d,text, args...) do { \
	if (cc_verbose_check(o_v, c_d) != 0) { \
		cc_verbose_internal(text , ## args); \
	} \
} while(0)

extern _cword get_capi_MessageNumber(void);
extern struct capi_pvt *capi_find_interface_by_msgnum(unsigned short msgnum);
extern struct capi_pvt *capi_find_interface_by_plci(unsigned int plci);
extern MESSAGE_EXCHANGE_ERROR capi_wait_conf(struct capi_pvt *i, unsigned short wCmd);
extern MESSAGE_EXCHANGE_ERROR capidev_check_wait_get_cmsg(_cmsg *CMSG);
extern char *capi_info_string(unsigned int info);
extern void show_capi_info(struct capi_pvt *i, _cword info);
extern unsigned capi_ListenOnController(unsigned int CIPmask, unsigned controller);
extern unsigned capi_ManufacturerAllowOnController(unsigned controller);
extern void capi_parse_dialstring(char *buffer, char **interface, char **dest, char **param, char **ocid);
extern char *capi_number_func(unsigned char *data, unsigned int strip, char *buf);
extern int cc_add_peer_link_id(struct ast_channel *c);
extern struct ast_channel *cc_get_peer_link_id(const char *p);
extern void capi_remove_nullif(struct capi_pvt *i);
extern struct capi_pvt *capi_mknullif(struct ast_channel *c, unsigned long long controllermask);
struct capi_pvt *capi_mkresourceif(struct ast_channel *c, unsigned long long controllermask, struct capi_pvt *data_plci_ifc, cc_format_t codecs, int all);
extern int capi_create_reader_writer_pipe(struct capi_pvt *i);
extern struct ast_frame *capi_read_pipeframe(struct capi_pvt *i);
extern int capi_write_frame(struct capi_pvt *i, struct ast_frame *f);
extern int capi_verify_resource_plci(const struct capi_pvt *i);
extern const char* pbx_capi_get_cid (const struct ast_channel* c, const char *notAvailableVisual);
extern const char* pbx_capi_get_callername (const struct ast_channel* c, const char *notAvailableVisual);
const char* pbx_capi_get_connectedname (const struct ast_channel* c, const char *notAvailableVisual);
char* pbx_capi_strsep_controller_list (char** param);

#define capi_number(data, strip) \
  capi_number_func(data, strip, alloca(AST_MAX_EXTENSION))

typedef struct capi_prestruct_s {
	unsigned short wLen;
	unsigned char *info;
} capi_prestruct_t;

/*
 * Eicon's capi_sendf() function to create capi messages easily
 * and send this message.
 * Copyright by Eicon Networks / Dialogic
 */
extern MESSAGE_EXCHANGE_ERROR capi_sendf(
	struct capi_pvt *capii, int waitconf,
	_cword command, _cdword Id, _cword Number, char * format, ...);

/*!
	\brief nulliflist
	*/
const struct capi_pvt *pbx_capi_get_nulliflist(void);
/*!
		\brief cc_mutex_lock(&nullif_lock)
	*/
void pbx_capi_nulliflist_lock(void);
/*!
		\brief cc_mutex_unlock(&nullif_lock)
	*/
void pbx_capi_nulliflist_unlock(void);

#endif
