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
 *	Decoding of name-operations from asn1-97
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
#include "chan_capi_qsig_asn197no.h"

/*
 * Returns an "Name" from an string, encoded as in name-operations-asn1-97
 *	data should be a buffer with max. 50 bytes, according to spec
 *	bufds returns size of name in "buf"
 *  return:
 *	index counter
 */

unsigned int cc_qsig_asn197no_get_name(char *buf, int buflen, unsigned int *bufds, int *idx, unsigned char *data)
{
	int myidx = *idx;
	unsigned int nametag;
	unsigned int namelength = 0;
	unsigned int nametype;
	unsigned int namesetlength = 0;
	unsigned int namepres;
	unsigned int charset = 1;
	unsigned int seqlength = 0;

	nametag = data[myidx++];
	if (nametag == (ASN1_SEQUENCE | ASN1_TC_UNIVERSAL | ASN1_TF_CONSTRUCTED)) { /* 0x30 */
		/* This facility is encoded as SEQUENCE */
		seqlength = data[++myidx];
		myidx++;
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  Got name sequence (Length= %i)\n", seqlength);
	}

	if (nametag < 0x80) {	/* Tag shows an simple String */
		namelength = cc_qsig_asn1_get_string((unsigned char *)buf, buflen, &data[myidx]);
	} else {		/* Tag shows an context specific String */
		nametype = (nametag & 0x0F);		/*	Type of Name-Struct */
		namepres = nametype;			/*	Name Presentation or Restriction */
				
		switch (nametype) {
			case 0:		/* Simple Name */
			case 2:		/* [LENGTH] [STRING] */
				namelength = cc_qsig_asn1_get_string((unsigned char *)buf, buflen, &data[myidx]);
				break;
			case 1:		/* Nameset */
			case 3:		/*  [LENGTH] [BIT-STRING] [LENGTH] [STRING] [INTEGER] [LENGTH] [VALUE] */
				namesetlength = data[myidx++];
				if (data[myidx++] == ASN1_OCTETSTRING) {
					/* should be so */
					namelength = cc_qsig_asn1_get_string((unsigned char *)buf, buflen, &data[myidx]);
					myidx += namelength + 1;
				} else {
					cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " Namestruct not ECMA conform (String expected)\n");
					break;
				}
				if (data[myidx++] == ASN1_INTEGER) {
					charset = cc_qsig_asn1_get_integer(data, &myidx);
				} else {
					cc_qsig_verbose( 1, VERBOSE_PREFIX_4 " Namestruct not ECMA conform (Integer expected)\n");
				}
				break;
			case 4:		/* Name not available */
				break;
			case 7:		/* Namepres. restricted NULL  - don't understand ECMA-164, Page 5 */
				break;
		}
	}
	if (namelength > 0) {
		myidx += namelength + 1;
		*bufds = namelength;
		return myidx - *idx;
	} else {
		return 0;
	}

}
