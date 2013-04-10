/*
 *  (QSIG)
 *
 *  Implementation of QSIG extensions for chan_capi
 *  
 *  Copyright 2006-2007 (c) Mario Goegel
 *
 *  Mario Goegel <m.goegel@gmx.de>
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */

#include "chan_capi_qsig_asn197ade.h"

#ifndef PBX_QSIG_ECMA_H
#define PBX_QSIG_ECMA_H

/* ECMA Features structs */


/* Call Transfer Complete struct */
struct cc_qsig_ctcomplete {
	enum {
		primaryEnd,		/* 0 */
  		secondaryEnd		/* 1 */
	} endDesignation;
	
	struct asn197ade_numberscreened redirectionNumber;
	char *basicCallInfoElements;	/* OPTIONAL: ASN1_APPLICATION Type */
	char *redirectionName;		/* OPTIONAL */
	enum {
		answered,
  		alerting
	} callStatus;			/* DEFAULT: answered */
	char *argumentExtension;	/* OPTIONAL: ASN1_SEQUENCE - manufacturer specific extension */
};

/* CCBS struct */
struct cc_qsig_ccbsreq {
	char *numberA;			/* Simplified numbers*/
	char *numberB;
	char *PSS1InfoElement;		/* Bearer caps, LLC, HLC */
	char *subaddrA;
	char *subaddrB;
	int can_retain_service;
	int retain_sig_connection;
	char *extension;
};

/*
 *** ECMA QSIG Functions 
 */

extern void cc_qsig_op_ecma_isdn_namepres(struct cc_qsig_invokedata *invoke, struct capi_pvt *i);
extern int cc_qsig_encode_ecma_name_invoke(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, int nametype, const char *name);

extern int cc_qsig_encode_ecma_isdn_leginfo3_invoke(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *name);

extern void cc_qsig_op_ecma_isdn_leginfo2(struct cc_qsig_invokedata *invoke, struct capi_pvt *i);

extern void cc_qsig_op_ecma_isdn_prpropose(struct cc_qsig_invokedata *invoke, struct capi_pvt *i);
extern void cc_qsig_encode_ecma_prpropose(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param);

extern void cc_qsig_encode_ecma_sscalltransfer(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param);

extern void cc_qsig_encode_ecma_calltransfer(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param, int info);
extern unsigned int cc_qsig_decode_ecma_calltransfer(struct cc_qsig_invokedata *invoke, struct capi_pvt *i,  struct cc_qsig_ctcomplete *ctc);

#endif
