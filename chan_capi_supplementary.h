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
 
#ifndef _PBX_CAPI_SUPP_H
#define _PBX_CAPI_SUPP_H

/*
 * prototypes
 */
extern void ListenOnSupplementary(unsigned controller);
extern int handle_facility_indication_supplementary(
	_cmsg *CMSG, unsigned int PLCI, unsigned int NCCI, struct capi_pvt *i);
extern void handle_facility_confirmation_supplementary(
	_cmsg *CMSG, unsigned int PLCI, unsigned int NCCI, struct capi_pvt **i,
	struct ast_channel** interface_owner);
extern int pbx_capi_ccbs(struct ast_channel *c, char *data);
extern int pbx_capi_ccbsstop(struct ast_channel *c, char *data);
extern int pbx_capi_ccpartybusy(struct ast_channel *c, char *data);
extern void cleanup_ccbsnr(void);
extern unsigned int capi_get_ccbsnrcontroller(unsigned int handle);
extern _cword capi_ccbsnr_take_ref(unsigned int handle);

#endif
