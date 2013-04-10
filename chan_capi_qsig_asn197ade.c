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

/*
 *	Decoding of addressing-data-elements from asn1-97
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

/*
 * Returns an "Party Number" from an string, encoded as in addressing-data-elements-asn1-97
 *	data should be a buffer with max. 20 bytes, according to spec
 *  return:
 *	index counter
 */
unsigned int cc_qsig_asn197ade_get_partynumber(char *buf, int buflen, int *idx, unsigned char *data)
{
	int myidx = *idx;
	int datalength;
	int numtype;
	
	datalength = data[myidx++];
	
	if (!datalength) {
		return 0;
	}
	
	numtype = (data[myidx++] & 0x0F);	/* defines type of Number: numDigits, publicPartyNum, nsapEncNum, dataNumDigits */
	
	/* cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " * num type %i,%i\n", numtype, myidx);   */
	switch (numtype){
		case 0:
			if (data[myidx] > 0) {	/* length of this context data */
				if (data[myidx+1] == ASN1_TC_CONTEXTSPEC) {
					myidx += 2;
					myidx += cc_qsig_asn197ade_get_numdigits(buf, buflen, &myidx, data);
				} else {
					myidx += cc_qsig_asn197ade_get_numdigits(buf, buflen, &myidx, data);
				}
			}
			break;
		case 1:			/* publicPartyNumber (E.164) not supported yet */
			return 0;
			break;
		case 2:			/* NsapEncodedNumber (some ATM stuff) not supported yet */
			return 0;
			break;
		case 3:
			if (data[myidx++] > 0) {	/* length of this context data */
				if (data[myidx+1] == ASN1_TC_CONTEXTSPEC) {
					myidx += 2;
					myidx += cc_qsig_asn197ade_get_numdigits(buf, buflen, &myidx, data);
				} else {
					myidx += cc_qsig_asn197ade_get_numdigits(buf, buflen, &myidx, data);
				}
			}
			break;
	};
	/* cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " * num type %i,%i\n", numtype, myidx);    */
	return myidx - *idx;
}

/*
 * Returns an string from ASN.1 encoded string
 */
unsigned int cc_qsig_asn197ade_get_numdigits(char *buf, int buflen, int *idx, unsigned char *data)
{
	int strsize;
	int myidx = *idx;
	
	strsize = data[myidx++];
	if (strsize > buflen)	
		strsize = buflen;
	memcpy(buf, &data[myidx], strsize);
	buf[strsize] = 0;
	
 	/* cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " * string length %i,%i\n", strsize, *idx); */
	return strsize;
}

/*
 * Returns an string from ASN.1 encoded string
 */
unsigned int cc_qsig_asn197ade_add_numdigits(char *buf, int buflen, int *idx, unsigned char *data)
{
	int myidx=0;
	
	if ((1 + buflen) > sizeof(*buf)) {
		/* String exceeds buffer size */
		return 0;
	}
	
	buf[myidx++] = buflen;
	memcpy(&buf[myidx], data, buflen);
	myidx = 1 + strlen(buf);
	return myidx;
}

/*
 * Returns an "PresentedNumberScreened" from an string, encoded as in addressing-data-elements-asn1-97
 *	data is pointer to PresentedNumberScreened struct
 *  return:
 *	index counter
 */
unsigned int cc_qsig_asn197ade_get_pns(unsigned char *data, int *idx, struct asn197ade_numberscreened *ns)
{	/* sample data:  a0 08 80 03>513<0a 01 00 */
	int myidx = *idx;
	char buf[ASN197ADE_NUMDIGITS_STRSIZE+1];
	unsigned int buflen = sizeof(buf);
	unsigned res;
	int numtype;
	
	ns->partyNumber = NULL;
	ns->screeningInd = userProvidedNotScreened;
	
	numtype = (data[myidx++] & 0x0F);	/* defines type of Number */
	
	/* cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " * num type %i,%i\n", numtype, myidx);   */
	switch (numtype){
		case 0:
			/* myidx points now to length */
			res = cc_qsig_asn197ade_get_partynumber(buf, buflen, &myidx, data);
			/* cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " * res %i\n", numtype);   */
			if (!res)
				return 0;
			
			myidx += res;
			if (strlen(buf)) {
				ns->partyNumber = ast_strdup(buf);
			}
			
			/* get screening indicator */
			if (data[myidx] == ASN1_ENUMERATED) {	/* HACK: this is not safe - check length of this parameter */
				myidx++;
				ns->screeningInd = cc_qsig_asn1_get_integer(data, &myidx);
			}
			
			break;
		case 1:			/* presentation restricted */
			myidx += data[myidx] + 1;	/* this val should be zero */
			break;
		case 2:			/* number not available due to interworking */
			myidx += data[myidx] + 1;	/* this val should be zero */
			break;
		case 3:
			/* myidx points now to length */
			res = cc_qsig_asn197ade_get_partynumber(buf, buflen, &myidx, data);
			if (!res)
				return 0;
			
			myidx += res;
			if (strlen(buf)) {
				ns->partyNumber = ast_strdup(buf);
			}
			
			/* get screening indicator */
			if (data[myidx] == ASN1_ENUMERATED) {   /* HACK: this is not safe - check length of this parameter */
				myidx++;
				ns->screeningInd = cc_qsig_asn1_get_integer(data, &myidx);
			}
			
			break;
	};

	return myidx - *idx;
}
