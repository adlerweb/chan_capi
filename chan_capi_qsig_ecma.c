/*
 * An implementation of Common ISDN API 2.0 for Asterisk
 *
 * Copyright (C) 2005-2008 Cytronics & Melware
 * Copyright (C) 2007 Mario Goegel
 *
 * Armin Schindler <armin@melware.de>
 * Mario Goegel <m.goegel@gmx.de>
 *
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
		
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_utils.h"
#include "chan_capi_qsig.h"
#include "chan_capi_qsig_ecma.h"
#include "chan_capi_qsig_asn197ade.h"
#include "chan_capi_qsig_asn197no.h"


/* 
 * Handle Operation: 1.3.12.9.0-3		ECMA/ISDN/NAMEPRESENTATION 
 * 
 * This function decodes the namepresentation facility
 * The name will be copied in the cid.cid_name field of the asterisk channel struct
 *
 * parameters
 *	invoke	struct, which contains encoded data from facility
 *	i	is pointer to capi channel
 * returns
 * 	nothing
 */
void cc_qsig_op_ecma_isdn_namepres(struct cc_qsig_invokedata *invoke, struct capi_pvt *i)
{
	char callername[51];	/* ECMA defines max length to 50 */
	unsigned int namelength = 0;
	unsigned int datalength;
	int myidx = 0;
	char *nametype = NULL;
	
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "Handling Name Operation (id# %#x)\n", invoke->id);

	callername[0] = 0;
	datalength = invoke->datalen;
	
	myidx = cc_qsig_asn197no_get_name(callername, ASN197NO_NAME_STRSIZE, &namelength, &myidx, invoke->data );
	
	if (namelength == 0) {
		return;
	}
		
	/* TODO: Maybe we do some charset conversions */
	
	switch (invoke->type) {
		case 0:	/* Calling Name */
			nametype = "CALLING NAME";
			break;
		case 1:	/* Called Name */
			nametype = "CALLED NAME";
			break;
		case 2: /* Connected Name */
			nametype = "CONNECTED NAME";
			break;
		case 3: /* Busy Name */
			nametype = "BUSY NAME";
			break;
	}
		
	switch (invoke->type) {
		case 0:	/* Calling Name */
#ifdef CC_AST_HAS_VERSION_1_8
			/* ast_set_callerid updates CDR, but __ast_pbx_run updates CDR too.
					__ast_pbx_run does not uses the channel lock and this results in destruction
					of CDR list
					Do notcall this function until problem resolved
					ast_set_callerid(i->owner, NULL, callername, NULL);
					Use code from ast_set_callerid but do not update CDR
					*/
				i->owner->caller.id.name.valid = 1;
				ast_free(i->owner->caller.id.name.str);
				i->owner->caller.id.name.str = ast_strdup(callername);
#else
			i->owner->cid.cid_name = ast_strdup(callername);	/* Save name to callerid */
#endif
			break;
		case 1:	/* Called Name */
		case 2: /* Connected Name */
		case 3: /* Busy Name */
 			if (i->qsig_data.dnameid) {	/* this facility may come more than once - if so, then update this value */
				cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * deleting previously received name.\n", nametype, namelength, callername); 
			 	ast_free(i->qsig_data.dnameid);
			}
			i->qsig_data.dnameid = ast_strdup(callername);	/* save name as destination in qsig specific fields */
									/* there's no similarly field in asterisk */
			break;
		default:
			break;
	}
	
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Got %s: \"%s\" (%i byte(s))\n", nametype, callername, namelength); 

	/* if there was an sequence tag, we have more informations here, but we will ignore it at the moment */
	
}

/* 
 * Encode Operation: 1.3.12.9.0-3		ECMA/ISDN/NAMEPRESENTATION 
 * 
 * This function encodes the namepresentation facility
 * The name will be copied from the cid.cid_name field of the asterisk channel struct.
 * We create an invoke struct with the complete encoded invoke.
 *
 * parameters
 *	buf 	is pointer to facility array, not used now
 *	idx	current idx in facility array, not used now
 *	invoke	struct, which contains encoded data for facility
 *	i	is pointer to capi channel
 * returns
 * 	always 0
 */
int cc_qsig_encode_ecma_name_invoke(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, int nametype, const char * name)
{
	unsigned char namebuf[51];
	unsigned char data[255];
	int dataidx = 0;
	int namelen = 0;
	
	if (name)
		namelen = strlen(name);
	
	if (namelen < 1) {	/* There's no name available, try to take Interface-Name */
		if (i->name) {
			if (strlen(i->name) >= 1) {
				if (namelen > 50)
					namelen = 50;
				namelen = strlen(i->name);
				memcpy(namebuf, i->name, namelen);
			}
		}
	} else {
		if (namelen > 50)
			namelen = 50;
		memcpy(namebuf, name, namelen);
	}
	namebuf[namelen] = 0;
	
	invoke->id = 1;
  	invoke->descr_type = -1;	/* Let others do the work: qsig_add_invoke */
	invoke->type = (nametype % 4);	/* Invoke Operation Number, if OID it's the last byte*/
	
	if (namelen>0) {
		data[dataidx++] = 0x80;	/* We send only simple Name, Namepresentation allowed */
		data[dataidx++] = namelen;
		memcpy(&data[dataidx], namebuf, namelen);
		dataidx += namelen;
	} else {
		data[dataidx++] = 0x84;	/* Name not available */
		data[dataidx++] = 0;
	}
	
	invoke->datalen = dataidx;
	memcpy(invoke->data, data, dataidx);
	
 	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Sending \"%s\": (%i byte(s))\n", namebuf, namelen); 

	return 0;
}

/* 
 * Encode Operation: 1.3.12.9.22		ECMA/ISDN/LEG_INFO3
 * 
 * This function encodes the namepresentation facility
 * The name will be copied from the cid.cid_name field of the asterisk channel struct.
 * We create an invoke struct with the complete encoded invoke.
 *
 * parameters
 *	buf 	is pointer to facility array, not used now
 *	idx	current idx in facility array, not used now
 *	invoke	struct, which contains encoded data for facility
 *	i	is pointer to capi channel
 * returns
 * 	always 0
 */
int cc_qsig_encode_ecma_isdn_leginfo3_invoke(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *name)
{
	unsigned char namebuf[51];
	unsigned char data[255];
	
	int dataidx = 0;
	int namelen = 0;
	
	if (name)
		namelen = strlen(name);
	
	if (namelen < 1) {	/* There's no name available, try to take Interface-Name */
		if (i->name) {
			if (strlen(i->name) >= 1) {
				if (namelen > 50)
					namelen = 50;
				namelen = strlen(i->name);
				memcpy(namebuf, i->name, namelen);
			}
		}
	} else {
		if (namelen > 50)
			namelen = 50;
		memcpy(namebuf, name, namelen);
	}
	
	invoke->id = 1;
	invoke->descr_type = -1;	/* Let others do the work: qsig_add_invoke */
	invoke->type = 22;		/* Invoke Operation Number, if OID it's the last byte*/
	
	data[dataidx++] = ASN1_TF_CONSTRUCTED | ASN1_SEQUENCE;
	data[dataidx++] = 5 + namelen;
	
	data[dataidx++] = ASN1_BOOLEAN;		/* PresentationAllowedIndicator */
	data[dataidx++] = 1;
	data[dataidx++] = 1;
	
	if (namelen>0) {
		data[dataidx++] = 0x80;	/* We send only simple Name, Namepresentation allowed */
		data[dataidx++] = namelen;
		memcpy(&data[dataidx], namebuf, namelen);
		dataidx += namelen;
	} else {
		data[dataidx++] = 0x84;	/* Name not available */
		data[dataidx++] = 0;
	}
	
	invoke->datalen = dataidx;
	memcpy(invoke->data, data, dataidx);
	
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Sending QSIG_LEG_INFO3 \"%s\": (%i byte(s))\n", namebuf, namelen); 

	return 0;
}


/* 
 * Handle Operation: 1.3.12.9.21		ECMA/ISDN/LEG_INFORMATION2
 * 
 * This function decodes the LEG INFORMATION2 facility
 * The datas will be copied in the some Asterisk channel variables -> see README.qsig
 *
 * parameters
 *	invoke	struct, which contains encoded data from facility
 *	i	is pointer to capi channel
 * returns
 * 	nothing
 */
void cc_qsig_op_ecma_isdn_leginfo2(struct cc_qsig_invokedata *invoke, struct capi_pvt *i)
{
	
	unsigned int datalength;
	unsigned int seqlength = 0;
	int myidx = 0;
	
	unsigned int parameter = 0;
	unsigned int divCount = 0;
	unsigned int divReason = 0;
	unsigned int orgDivReason = 0;
	char tempstr[5];
	char divertNum[ASN197ADE_NUMDIGITS_STRSIZE+1];
	char origCalledNum[ASN197ADE_NUMDIGITS_STRSIZE+1];
	struct asn197ade_numberscreened divertPNS, origPNS;
	char divertName[ASN197NO_NAME_STRSIZE+1];
	char origCalledName[ASN197NO_NAME_STRSIZE+1];
	unsigned int temp = 0;
	unsigned int temp2 = 0;
	
	divertNum[0] = 0;
	origCalledNum[0] = 0;
	divertNum[0] = 0;
	divertName[0] = 0;
	origCalledName[0] = 0;
	
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "Handling QSIG LEG INFO2 (id# %#x)\n", invoke->id);

	origPNS.partyNumber = NULL;
	divertPNS.partyNumber = NULL;
	
	if (invoke->data[myidx++] != (ASN1_SEQUENCE | ASN1_TC_UNIVERSAL | ASN1_TF_CONSTRUCTED)) { /* 0x30 */
		/* We do not handle this, because it should start with an sequence tag */
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG LEG INFO2 - not a sequence\n");
		return;
	}
	
	/* This facility is encoded as SEQUENCE */
	seqlength = invoke->data[myidx++];
	datalength = invoke->datalen;
	if (datalength < (seqlength+1)) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG LEG INFO2 - buffer error\n");
		return;
	}
	
	if (invoke->data[myidx++] == ASN1_INTEGER) 
		divCount = cc_qsig_asn1_get_integer(invoke->data, &myidx);
	
	if (invoke->data[myidx++] == ASN1_ENUMERATED) 
		divReason = cc_qsig_asn1_get_integer(invoke->data, &myidx);
	
	while (myidx < datalength) {
		parameter = (invoke->data[myidx++] & 0x0f);
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * Found parameter %i\n", parameter);
		switch (parameter) {
			case 0:
				myidx++;	/* Ignore Length of enumeration tag*/
				if (invoke->data[myidx++] == ASN1_ENUMERATED)
					orgDivReason = cc_qsig_asn1_get_integer(invoke->data, &myidx);
				break;
			case 1:
				temp = invoke->data[myidx++]; /* keep the length of this info - maybe we don't get all data now */
				cc_qsig_asn197ade_get_pns(invoke->data, &myidx, &divertPNS);
				myidx += temp;
				break;
			case 2:
				temp = invoke->data[myidx++]; /* keep the length of this info - maybe we don't get all data now */
				cc_qsig_asn197ade_get_pns(invoke->data, &myidx, &origPNS);
				myidx += temp;
				break;
			case 3:
				/* Redirecting Name */
				temp = invoke->data[myidx++]; /* keep the length of this info - maybe we don't get all data now */
 				cc_qsig_asn197no_get_name(divertName, ASN197NO_NAME_STRSIZE, &temp2, &myidx, invoke->data);
				myidx += temp + 1;
				break;
			case 4:
				/* origCalled Name */
				temp = invoke->data[myidx++]; /* keep the length of this info - maybe we don't get all data now */
				cc_qsig_asn197no_get_name(origCalledName, ASN197NO_NAME_STRSIZE, &temp2, &myidx, invoke->data);
				myidx += temp + 1;
				break;
			default:
				cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * unknown parameter %i\n", parameter);
				break;
		}
	}

	snprintf(tempstr, 5, "%i", divReason);
	pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_DIVREASON", tempstr);
	snprintf(tempstr, 5, "%i", orgDivReason);
	pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_ODIVREASON", tempstr);
	snprintf(tempstr, 5, "%i", divCount);
	pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_DIVCOUNT", tempstr);
	
	if (divertPNS.partyNumber)
		pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_DIVNUM", divertPNS.partyNumber);
	if (origPNS.partyNumber)
		pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_ODIVNUM", origPNS.partyNumber);
	pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_DIVNAME", divertName);
	pbx_builtin_setvar_helper(i->owner, "_QSIG_LI2_ODIVNAME", origCalledName);

	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Got QSIG_LEG_INFO2: %i(%i), %ix %s->%s, %s->%s\n", divReason, orgDivReason, divCount, origPNS.partyNumber, divertPNS.partyNumber, origCalledName, divertName);
	
	return;
	
}


/* 
 * Encode Operation: 1.3.12.9.12		ECMA/ISDN/CALLTRANSFER
 * 
 * This function encodes the call transfer facility
 *
 * We create an invoke struct with the complete encoded invoke.
 *
 * parameters
 *	buf 	is pointer to facility array, not used now
 *	idx	current idx in facility array, not used now
 *	invoke	struct, which contains encoded data for facility
 *	i	is pointer to capi channel
 *	param	is parameter from capicommand
 *	info	this facility is part of 2, 0 is facility 1, 1 is facility 2
 * returns
 * 	always 0
 */
void cc_qsig_encode_ecma_calltransfer(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param, int info)
{
	char *cid, *ccanswer;
	char *name = NULL;
	int icanswer = 0;
	int cidlen = 0;
	int namelength = 0;
	int seqlen = 13;
	char c[255];
	int ix = 0;

	if (param) { /* got Call Transfer Parameters */
		if (info) {
			cid = strsep(&param, COMMANDSEPARATOR);
			cidlen = strlen(cid);
			if (cidlen > 20)	/* HACK: stop action here, maybe we have invalid data */
				cidlen = 20;
		} else {
			char *tmp = strsep(&param, COMMANDSEPARATOR);
			tmp = NULL;
			cid = strsep(&param, COMMANDSEPARATOR);
			cidlen = strlen(cid);
			if (cidlen > 20)	/* HACK: stop action here, maybe we have invalid data */
				cidlen = 20;
			
			ccanswer = strsep(&param, COMMANDSEPARATOR);
			if (ccanswer[0])
				icanswer = ccanswer[0] - 0x30;
		}
	} else {
/* 		cid = ast_strdup(i->owner->cid.cid_num);*/ /* Here we get the Asterisk extension */
		if (info) { /* info is >0 on outbound channel (second facility) */
 			struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);
			
			cid = ast_strdup(i->cid);
			cidlen = strlen(cid);
			
			if (ii) {
				/* send callers name to user B */
#ifdef CC_AST_HAS_VERSION_1_8
				if (ii->owner->caller.id.name.valid ) {
					name = ast_strdupa(S_COR(ii->owner->caller.id.name.valid, ii->owner->caller.id.name.str, ""));
					namelength = strlen(name);
				}
#else
				if (ii->owner->cid.cid_name) {
					name = ast_strdupa(ii->owner->cid.cid_name);
					namelength = strlen(name);
				}
#endif
			}
		} else { /* have to build first facility - send destination number back to inbound channel */
			struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);
			cid = ast_strdup(ii->dnid);
			cidlen = strlen(cid);
			
			if (ii) {
				/* send destination name to user A */
				if (ii->qsig_data.dnameid) {
					name = ast_strdupa(ii->qsig_data.dnameid);
					namelength = strlen(name);
				}
			}
		}
		
		if (!info)
			icanswer = i->qsig_data.calltransfer_onring % 1;
	}
	
	seqlen += cidlen;
	if (namelength)
		seqlen += 4 + namelength;
	
	c[ix++] = ASN1_SEQUENCE | ASN1_TF_CONSTRUCTED;	/* start of SEQUENCE */
	c[ix++] = seqlen;
		
	c[ix++] = ASN1_ENUMERATED;					/* End Designation */
	c[ix++] = 1; /* length */
	c[ix++] = info;

	c[ix++] = (ASN1_TC_CONTEXTSPEC | ASN1_TF_CONSTRUCTED) + 0;	/* val 0 - Source Caller ID struct */
	c[ix++] = 5 + cidlen;
		c[ix++] = ASN1_TC_CONTEXTSPEC;				/* CallerID */
		c[ix++] = cidlen;
		memcpy(&c[ix], cid, cidlen);
		ix += cidlen;
		c[ix++] = ASN1_ENUMERATED;					/* Screening Indicator */
		c[ix++] = 1; /* length */
		c[ix++] = 1; /* 01 = userProvidedVerifiedAndPassed    ...we hope so */
	
	{
		if (namelength) {
			c[ix++] = (ASN1_TC_CONTEXTSPEC | ASN1_TF_CONSTRUCTED) + 1;	/* val 1 - Source Caller ID struct */
			c[ix++] = 2 + namelength;
			c[ix++] = ASN1_OCTETSTRING;				/* CallerID */
			c[ix++] = namelength;
			memcpy(&c[ix], name, namelength);
			ix += namelength;
		}
	}
		
	c[ix++] = ASN1_ENUMERATED;			/* val 3 - wait for connect ? */
	c[ix++] = 1;
	c[ix++] = icanswer;
	
					/* end of SEQUENCE */
	/* there are optional data possible here */
	
	invoke->id = 12;
	invoke->descr_type = -1;
	invoke->type = 12;	/* Invoke Operation Code */
	
	invoke->datalen = ix;
	memcpy(invoke->data, c, ix);
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Sending QSIG_CT: %i->%s\n", info, cid);
	
	if (cid)
		ast_free(cid);
	
}

/* 
 * Decode Operation: 1.3.12.9.12		ECMA/ISDN/CALLTRANSFER
 * 
 * This function decodes the call transfer facility
 *
 * We create an invoke struct with the complete encoded invoke.
 *
 * parameters
 *	buf 	is pointer to facility array, not used now
 *	idx	current idx in facility array, not used now
 *	invoke	struct, which contains encoded data from facility
 *	i	is pointer to capi channel
 * returns
 * 	transfer to destination number
 */
unsigned int cc_qsig_decode_ecma_calltransfer(struct cc_qsig_invokedata *invoke, struct capi_pvt *i, struct cc_qsig_ctcomplete *ctc)
{
	unsigned int datalength;
	unsigned int seqlength = 0;
	unsigned char *data = invoke->data;
	int myidx = 0;
	/* TODO: write more code */
	
	char *ct_status_txt[] = { "ANSWERED", "ALERTING" };
	char ct_name[ASN197NO_NAME_STRSIZE+1] = { "EMPTY" };
	unsigned int namelength = 0;
	int temp = 0;
	
	ctc->endDesignation = primaryEnd;
	ctc->redirectionNumber.partyNumber = NULL;
	ctc->redirectionNumber.screeningInd = userProvidedNotScreened;
	ctc->basicCallInfoElements = NULL;
	ctc->redirectionName = NULL;
	ctc->callStatus = answered;
	ctc->argumentExtension = NULL;	/* unhandled yet */
	
#define ct_err(x...)	{ cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG CALL TRANSFER - "x); return 0; }
	
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "Handling QSIG CALL TRANSFER (id# %#x)\n", invoke->id);

	if (data[myidx++] != (ASN1_SEQUENCE | ASN1_TC_UNIVERSAL | ASN1_TF_CONSTRUCTED)) { /* 0x30 */
		/* We do not handle this, because it should start with an sequence tag */
		ct_err("not a sequence\n");
	}
	
	/* This facility is encoded as SEQUENCE */
	seqlength = data[myidx++];
	datalength = invoke->datalen;
	if (datalength < (seqlength+1)) {
		ct_err("buffer error\n");
	}
	
	if (data[myidx++] == ASN1_ENUMERATED) {
			ctc->endDesignation = cc_qsig_asn1_get_integer(data, &myidx);
	} else {
		ct_err("no endDesignation information.\n");
	}
	
	temp = cc_qsig_asn197ade_get_pns(data, &myidx, &ctc->redirectionNumber);
	
	if (!temp) {
		ct_err("error on decoding PresentedNumberScreened value.\n");
	}
	myidx += temp;
	
	if (myidx < datalength) {
		if (data[myidx] == ASN1_TC_APPLICATION) {
			myidx++;
			/* TODO: check size -> could be bigger than 256 bytes - MSB is set then */
			ctc->basicCallInfoElements = ast_malloc(data[myidx]);
			if (ctc->basicCallInfoElements) {
				memcpy(ctc->basicCallInfoElements, &data[myidx+1], data[myidx] );
			} else {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * QSIG CALL TRANSFER - couldn't allocate memory for basicCallInfoElements.\n", (int)data[myidx]);
			}
			myidx += data[myidx] + 1;
		}
	}
	
	if (myidx < datalength) {
		if (data[myidx] != ASN1_ENUMERATED) { /* Maybe we get an name (OPTIONAL) */
			myidx += cc_qsig_asn197no_get_name(ct_name, ASN197NO_NAME_STRSIZE+1, &namelength, &myidx, data );
			if (namelength)
				ctc->redirectionName = ast_strdup(ct_name);
		}
	}
	
	if (myidx < datalength) {
		if (data[myidx++] == ASN1_ENUMERATED) { /* Call Status */
			ctc->callStatus = cc_qsig_asn1_get_integer(data, &myidx);
		}
	}	
	
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Got QSIG CALL TRANSFER endDesignation: %i partyNumber: %s (ScreeningInd: %i), partyName: \"%s\", Call state: %s\n", 
		   ctc->endDesignation, ctc->redirectionNumber.partyNumber, ctc->redirectionNumber.screeningInd, ctc->redirectionName, ct_status_txt[ctc->callStatus]);
	
	return 1;
#undef ct_err
}

/* 
 * Encode Operation: 1.3.12.9.99		ECMA/ISDN/SINGLESTEPCALLTRANSFER
 * 
 * This function encodes the single step call transfer facility
 *
 * We create an invoke struct with the complete encoded invoke.
 *
 * parameters
 *	buf 	is pointer to facility array, not used now
 *	idx	current idx in facility array, not used now
 *	invoke	struct, which contains encoded data for facility
 *	i	is pointer to capi channel
 *	param	is parameter from capicommand
 * returns
 * 	always 0
 */
void cc_qsig_encode_ecma_sscalltransfer(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param)
{
	char *cidsrc, *ciddst;
	int srclen, dstlen;
	int seqlen = 12;
	char c[255];
	int ix = 0;

	cidsrc = strsep(&param, COMMANDSEPARATOR);
	srclen = strlen(cidsrc);
	if (srclen > 20)	/* HACK: stop action here, maybe we have invalid data */
		srclen = 20;
	
	ciddst = strsep(&param, COMMANDSEPARATOR);
	dstlen = strlen(ciddst);
	if (dstlen > 20)	/* HACK: stop action here, maybe we have invalid data */
		dstlen = 20;
	
	seqlen += srclen + dstlen;
	
	
	c[ix++] = ASN1_SEQUENCE | ASN1_TF_CONSTRUCTED;	/* start of SEQUENCE */
	c[ix++] = seqlen;
		
	c[ix++] = ASN1_TC_CONTEXTSPEC;		/* val 1 - Destination CallerID */
	c[ix++] = dstlen;
	memcpy(&c[ix], ciddst, dstlen);
	ix += dstlen;
	
	c[ix++] = ASN1_TC_CONTEXTSPEC | ASN1_TF_CONSTRUCTED;	/* val 2 - Source Caller ID struct */
	c[ix++] = 5 + srclen;
	c[ix++] = ASN1_TC_CONTEXTSPEC;				/* CallerID */
	c[ix++] = srclen;
	memcpy(&c[ix], cidsrc, srclen);
	ix += srclen;
	c[ix++] = ASN1_ENUMERATED;					/* Screening Indicator */
	c[ix++] = 1; /* length */
	c[ix++] = 1; /* 01 = userProvidedVerifiedAndPassed    ...we hope so */
	
	c[ix++] = ASN1_BOOLEAN;			/* val 3 - wait for connect ? */
	c[ix++] = 1;
	c[ix++] = 0;
	
	/* end of SEQUENCE */
	/* there are optional data possible here */
	
	invoke->id = 99;
	invoke->descr_type = -1;
	invoke->type = 99;

	invoke->datalen = ix;
	memcpy(invoke->data, c, ix);
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Sending QSIG_SSCT: %s->%s\n", cidsrc, ciddst);
	
}

/* 
 * Handle Operation: 1.3.12.9.19		ECMA/ISDN/PATH REPLACEMENT PROPOSE
 * 
 * This function decodes the PATH REPLACEMENT PROPOSE facility
 * The datas will be copied in the some capi_pvt channel variables 
 *
 * parameters
 *	invoke	struct, which contains encoded data from facility
 *	i	is pointer to capi channel
 * returns
 * 	nothing
 */
void cc_qsig_op_ecma_isdn_prpropose(struct cc_qsig_invokedata *invoke, struct capi_pvt *i)
{
	
	unsigned int datalength;
	unsigned int seqlength = 0;
	int myidx = 0;
	/* TODO: write more code */
	
	char callid[4+1];
	char reroutingnr[ASN197ADE_NUMDIGITS_STRSIZE+1];
	int temp = 0;
	
	callid[0] = 0;
	reroutingnr[0] = 0;
	
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "Handling QSIG PATH REPLACEMENT PROPOSE (id# %#x)\n", invoke->id);

	if (invoke->data[myidx++] != (ASN1_SEQUENCE | ASN1_TC_UNIVERSAL | ASN1_TF_CONSTRUCTED)) { /* 0x30 */
		/* We do not handle this, because it should start with an sequence tag */
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG REPLACEMENT PROPOSE - not a sequence\n");
		return;
	}
	
	/* This facility is encoded as SEQUENCE */
	seqlength = invoke->data[myidx++];
	datalength = invoke->datalen;
	if (datalength < (seqlength+1)) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG REPLACEMENT PROPOSE - buffer error\n");
		return;
	}
	
	if (invoke->data[myidx++] == ASN1_NUMERICSTRING) {
		int strsize;
		strsize = cc_qsig_asn1_get_string((unsigned char*)&callid, sizeof(callid), &invoke->data[myidx]);
		myidx += strsize +1;
	} else {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG REPLACEMENT PROPOSE - NUMERICSTRING expected\n");
		return;
	}
	
 	if (invoke->data[myidx++] == ASN1_TC_CONTEXTSPEC)
		temp = cc_qsig_asn1_get_string((unsigned char*)&reroutingnr, sizeof(reroutingnr), &invoke->data[myidx]);
	
	if (temp) {
		myidx += temp;
	} else {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * not Handling QSIG REPLACEMENT PROPOSE - partyNumber expected (%i)\n", myidx);
		return;
	}
	

	i->qsig_data.pr_propose_cid  = ast_strdup(callid);
	i->qsig_data.pr_propose_pn = ast_strdup(reroutingnr);
	
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Got QSIG_PATHREPLACEMENT_PROPOSE Call identity: %s, Party number: %s (%i)\n", callid, reroutingnr, temp);
	
	return;
}

/* 
 * Encode Operation: 1.3.12.9.19		ECMA/ISDN/PATH REPLACEMENT PROPOSE
 * 
 * This function encodes the path replacement propose
 *
 * We create an invoke struct with the complete encoded invoke.
 *
 * parameters
 *	buf 	is pointer to facility array, not used now
 *	idx	current idx in facility array, not used now
 *	invoke	struct, which contains encoded data for facility
 *	i	is pointer to capi channel
 *	param	is parameter from capicommand
 * returns
 * 	always 0
 */
void cc_qsig_encode_ecma_prpropose(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param)
{
 	int invokeop = 4;
	
	char *callid, *reroutingnr;
	int cidlen, rrnlen;
	int seqlen = 4;
	char c[255];
	int ix = 0;

	int res = 0;
	int ii = 0;
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;

	if (!i->qsig_data.pr_propose_cid)
		return ;
	
	if (!i->qsig_data.pr_propose_pn)
		return ;

	callid = i->qsig_data.pr_propose_cid;
	reroutingnr = i->qsig_data.pr_propose_pn;
	
	cidlen = strlen(callid);
	rrnlen = strlen(reroutingnr);
	seqlen += cidlen + rrnlen;
	

#if 0	
	c[ix++] = ASN1_SEQUENCE | ASN1_TF_CONSTRUCTED;	/* start of SEQUENCE */
	c[ix++] = seqlen;
		
	c[ix++] = ASN1_NUMERICSTRING;		/* val 1 - CallID */
	c[ix++] = cidlen;
	memcpy(&c[ix], callid, cidlen);
	ix += cidlen;
	
	c[ix++] = ASN1_TC_CONTEXTSPEC;	/* val 2 - Rerouting number*/
	c[ix++] = rrnlen;
	memcpy(&c[ix], reroutingnr, rrnlen);
	ix += rrnlen;
#else
	ASN1_ADD_SIMPLE(comp, (ASN1_SEQUENCE | ASN1_TF_CONSTRUCTED), c, ii);
	ASN1_PUSH(compstk, compsp, comp);
	
	res = cc_qsig_asn1_add_string2(ASN1_NUMERICSTRING, &c[ii], sizeof(c) - ii, 20, callid, cidlen);
	if (res < 0)
		return;
	ii += res;
	
	res = cc_qsig_asn1_add_string2(ASN1_TC_CONTEXTSPEC, &c[ii], sizeof(c) - ii, 20, reroutingnr, rrnlen);
	if (res < 0)
		return;
	ii += res;

	ASN1_FIXUP(compstk, compsp, c, ii);
	ix = ii;
#endif
	
	/* end of SEQUENCE */
	/* there are optional data possible here */
	
	invoke->id = invokeop;
	invoke->descr_type = -1;
	invoke->type = invokeop;

	invoke->datalen = ix;
	memcpy(invoke->data, c, ix);
	
	cc_qsig_verbose( 0, VERBOSE_PREFIX_4 "  * Sending QSIG_PATHREPLACEMENT_PROPOSE: Call identity: %s, Party number: %s\n", callid, reroutingnr);
	
	return;
}

void cc_qsig_encode_ecma_ccnr_req(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i, char *param)
{
	int invokeop = 27;
	
	int ii = 0;
	unsigned char c[256];
	struct rose_component *comp = NULL, *compstk[10];
	int compsp = 0;
	int ix = 0;

	ASN1_ADD_SIMPLE(comp, (ASN1_TF_CONSTRUCTED | ASN1_SEQUENCE), c, ii);
	ASN1_PUSH(compstk, compsp, comp);
	
	
	
#if 0	/* Constructed data - ECMAv1 HICOM/HIPATH */
	ASN1_ADD_SIMPLE(comp, (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTOR | ASN1_TAG_1), buffer, i);
	ASN1_PUSH(compstk, compsp, comp);
	res = asn1_string_encode(ASN1_OCTETSTRING, &buffer[i], sizeof(buffer)-i,  50, c->callername, namelen);
	if (res < 0)
		return -1;
	i += res;
	ASN1_FIXUP(compstk, compsp, buffer, i);
#endif
	
	invoke->id = invokeop;
	invoke->descr_type = -1;
	invoke->type = invokeop;

	invoke->datalen = ix;
	memcpy(invoke->data, c, ix);
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * Sending QSIG_CCNR_REQ\n");

	return ;
}
