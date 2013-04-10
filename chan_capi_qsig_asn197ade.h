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

#ifndef PBX_QSIG_ASN197ADE_H
#define PBX_QSIG_ASN197ADE_H

#define ASN197ADE_NUMDIGITS_STRSIZE	20

struct asn197ade_numberscreened {
	char *partyNumber;
	enum {
		userProvidedNotScreened,
		userProvidedVerifiedAndPassed,
		userProvidedVerifiedAndFailed,
		networkProvided
	} screeningInd;
};

extern unsigned int cc_qsig_asn197ade_get_partynumber(char *buf, int buflen, int *idx, unsigned char *data);
extern unsigned int cc_qsig_asn197ade_get_numdigits(char *buf, int buflen, int *idx, unsigned char *data);

extern unsigned int cc_qsig_asn197ade_add_numdigits(char *buf, int buflen, int *idx, unsigned char *data);

extern unsigned int cc_qsig_asn197ade_get_pns(unsigned char *data, int *idx, struct asn197ade_numberscreened *ns);

#endif
