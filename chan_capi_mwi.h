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
#ifndef __CHAN_CAPI_MWI_H__
#define __CHAN_CAPI_MWI_H__

extern int pbx_capi_mwi(struct ast_channel *c, char *info);
extern int pbx_capi_xmit_mwi(
	const struct cc_capi_controller *controller,
	unsigned short basicService, 
	unsigned int   numberOfMessages,
	unsigned short messageStatus,
	unsigned short messageReference,
	unsigned short invocationMode,
	const unsigned char* receivingUserNumber,
	const unsigned char* controllingUserNumber,
	const unsigned char* controllingUserProvidedNumber,
	const unsigned char* timeX208);
extern int pbx_capi_xmit_mwi_deactivate(
	const struct cc_capi_controller *controller,
	unsigned short basicService,
	unsigned short invocationMode,
	const unsigned char* receivingUserNumber,
	const unsigned char* controllingUserNumber);
extern void pbx_capi_register_mwi(struct cc_capi_controller *controller);
extern void pbx_capi_refresh_mwi(struct cc_capi_controller *controller);
extern void pbx_capi_unregister_mwi(struct cc_capi_controller *controller);
extern void pbx_capi_cleanup_mwi(struct cc_capi_controller *controller);
extern unsigned char* pbx_capi_build_facility_number(
	unsigned char mwifacptynrtype,
	unsigned char mwifacptynrton,
	unsigned char mwifacptynrpres,
	const char* number);

void pbx_capi_init_mwi_server (
	struct cc_capi_controller *mwiController,
	const struct cc_capi_conf *conf);

#endif
